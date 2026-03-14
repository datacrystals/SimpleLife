#include <random>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cmath>
#include <algorithm>

#include "Types.h"
#include "ThreadPool.h"

extern std::mt19937 rng;
extern std::uniform_real_distribution<float> randFloat;

const float SPATIAL_GRID_SIZE = 19.0f;

class World {
public:
    std::vector<OrganismRecord*> population;
    struct SpawnRequest { Genome dna; float x; float y; float energy; };
    std::vector<SpawnRequest> spawnRequests;
    std::mutex spawnMutex;
    
    ThreadPool pool;

    float worldTime = 0.0f;
    int nextOrgId = 1;
    
    float timeScale = 1.0f;          
    int maxPopulation = 2000;       
    
    float WORLD_WIDTH = 200.0f;
    float WORLD_HEIGHT = 140.0f;

    // Evolution & Balance Parameters
    float mutationRate = 0.05f;      
    float baseMetabolism = 0.1f;
    float photosynthesisRate = 1.0f; 
    float movementCost = 0.004f;
    float thrustMultiplier = 0.5f;
    float turnMultiplier = 0.03f;

    // UI Bound Parameters
    float segmentCost = 0.01f;
    float sizeDiscount = 0.005f;
    
    float mutChanceType = 0.2f;
    float mutChanceMotor = 0.2f;
    float mutChanceAddNode = 0.5f;
    
    int minSymmetry = 1;
    int maxSymmetry = 8;
    
    float minLifespan = 10.0f;
    float maxLifespan = 100.0f;
    
    float shadePenalty = 0.5f;
    float greenCrowdRadius = 10.0f;
    
    float herbivoreEatEnergy = 150.0f;
    float herbivoreAttackRange = 4.0f;
    
    float carnivoreAttackRange = 4.0f;
    float carnivoreEfficiency = 0.8f;

    // Shield & Combat Balance
    float shieldEfficiency = 0.7f; 
    float shieldCost = 0.05f;      
    float damageAmount = 150.0f;    

    World() { initEden(); }

    void getToroidalDiff(float x1, float y1, float x2, float y2, float& dx, float& dy) {
        dx = x2 - x1;
        dy = y2 - y1;
        if (dx > WORLD_WIDTH * 0.5f) dx -= WORLD_WIDTH;
        else if (dx < -WORLD_WIDTH * 0.5f) dx += WORLD_WIDTH;
        if (dy > WORLD_HEIGHT * 0.5f) dy -= WORLD_HEIGHT;
        else if (dy < -WORLD_HEIGHT * 0.5f) dy += WORLD_HEIGHT;
    }

    Genome mutateGenome(Genome parentDna) {
        Genome child = parentDna;
        for (auto& gene : child.genes) {
            if (randFloat(rng) < mutationRate) { 
                if (randFloat(rng) < mutChanceType) gene.type = static_cast<ColorType>(rand() % 6);
                if (randFloat(rng) < mutChanceMotor) gene.isMotorized = !gene.isMotorized; 
                gene.length += (randFloat(rng) - 0.5f) * 0.5f; 
                gene.param1 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.param2 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.weight_FoodSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.weight_HazardSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.bias += (randFloat(rng) - 0.5f) * 1.0f;
                if (gene.length < 0.2f) gene.length = 0.2f; 
            }
        }
        
        if (randFloat(rng) < (mutationRate * mutChanceAddNode) && child.genes.size() < 10) {
            ColorType newType = static_cast<ColorType>(rand() % 6);
            int attachIdx = rand() % child.genes.size(); 
            bool isMotor = randFloat(rng) > 0.5f;
            child.genes.push_back({newType, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, attachIdx, (randFloat(rng) - 0.5f) * 90.0f, isMotor});
        }
        
        // Mutate Lifespan
        if (randFloat(rng) < mutationRate) {
            child.lifespan += (randFloat(rng) - 0.5f) * 20.0f;
            if (child.lifespan < minLifespan) child.lifespan = minLifespan;
            if (child.lifespan > maxLifespan) child.lifespan = maxLifespan;
        }

        // Mutate Symmetry
        if (randFloat(rng) < mutationRate * 0.5f) {
            child.symmetry += (rand() % 3) - 1; 
            if (child.symmetry < minSymmetry) child.symmetry = minSymmetry;
            if (child.symmetry > maxSymmetry) child.symmetry = maxSymmetry;
        }
        return child;
    }

    void queueOrganism(Genome dna, float x, float y, float startingEnergy) {
        OrganismRecord* org = new OrganismRecord(nextOrgId++, dna, startingEnergy);
        org->points.push_back({x, y, x, y, 0, 0}); 
        
        std::vector<int> gene_p1(dna.genes.size());
        std::vector<int> gene_p2(dna.genes.size());
        std::vector<Stick> arm0_sticks;

        // Build the base structure (Arm 0)
        for (size_t i = 0; i < dna.genes.size(); i++) {
            Gene& g = dna.genes[i];
            int pIdx = (g.parentIndex >= 0 && g.parentIndex < (int)i) ? gene_p2[g.parentIndex] : 0;
            
            float angle = g.branchAngle * (3.14159f / 180.0f);
            float newX = org->points[pIdx].x + std::cos(angle) * g.length;
            float newY = org->points[pIdx].y + std::sin(angle) * g.length;
            
            org->points.push_back({newX, newY, newX, newY, 0, 0});
            int newPIdx = org->points.size() - 1;
            
            gene_p1[i] = pIdx;
            gene_p2[i] = newPIdx;
            
            Stick s = {pIdx, newPIdx, g.length, g.type, 1.0f, false, g.isMotorized, g.length, 0.0f, -1, (int)i};
            
            // Generate Structural Brace to lock joint
            if (g.parentIndex >= 0 && g.parentIndex < (int)i) {
                int grandP = gene_p1[g.parentIndex];
                if (grandP != newPIdx) { 
                    float dx = org->points[grandP].x - newX;
                    float dy = org->points[grandP].y - newY;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    Stick brace = {grandP, newPIdx, dist, ColorType::DEAD, 1.0f, true, false, dist, dist * 0.4f, -1, (int)i};
                    
                    s.brace_idx = arm0_sticks.size() + 1; 
                    arm0_sticks.push_back(s);
                    arm0_sticks.push_back(brace);
                } else {
                    arm0_sticks.push_back(s);
                }
            } else {
                arm0_sticks.push_back(s);
            }
        }

        // Add arm0 to org->sticks, resolving brace offsets
        int base_stick_idx = org->sticks.size();
        for(auto& st : arm0_sticks) {
            Stick copySt = st;
            if(copySt.brace_idx != -1) copySt.brace_idx += base_stick_idx;
            org->sticks.push_back(copySt);
        }

        // Apply Symmetry (Duplicate and rotate)
        if (dna.symmetry > 1) {
            int symCount = std::min(dna.symmetry, 8); 
            float angleStep = (2.0f * 3.14159265f) / symCount;
            
            for (int s_idx = 1; s_idx < symCount; s_idx++) {
                float theta = s_idx * angleStep;
                float cosT = std::cos(theta);
                float sinT = std::sin(theta);
                
                std::unordered_map<int, int> pointMap;
                pointMap[0] = 0; // Root point is shared
                
                // Collect unique points used by arm0
                std::vector<int> unique_points;
                for(auto& st : arm0_sticks) {
                    if (st.p1_idx != 0 && pointMap.find(st.p1_idx) == pointMap.end()) { unique_points.push_back(st.p1_idx); pointMap[st.p1_idx] = -1; }
                    if (st.p2_idx != 0 && pointMap.find(st.p2_idx) == pointMap.end()) { unique_points.push_back(st.p2_idx); pointMap[st.p2_idx] = -1; }
                }
                
                for (int oldIdx : unique_points) {
                    Point& pOld = org->points[oldIdx];
                    float dx = pOld.x - x;
                    float dy = pOld.y - y;
                    float rotX = dx * cosT - dy * sinT;
                    float rotY = dx * sinT + dy * cosT;
                    
                    org->points.push_back({x + rotX, y + rotY, x + rotX, y + rotY, 0, 0});
                    pointMap[oldIdx] = org->points.size() - 1;
                }
                
                // Duplicate connections
                int new_base_stick_idx = org->sticks.size();
                for (const Stick& st : arm0_sticks) {
                    Stick symSt = st;
                    symSt.p1_idx = pointMap[st.p1_idx];
                    symSt.p2_idx = pointMap[st.p2_idx];
                    if (symSt.brace_idx != -1) {
                        symSt.brace_idx = new_base_stick_idx + st.brace_idx; 
                    }
                    org->sticks.push_back(symSt);
                }
            }
        }
        population.push_back(org); 
    }

    void initEden() {
        population.clear();
        Genome dna;
        dna.genes = { {ColorType::GREEN, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1, 0.0f, false} };
        dna.lifespan = 40.0f;
        dna.symmetry = 1;

        for (int i=0; i<10; i++) {
            float rx = randFloat(rng) * WORLD_WIDTH;
            float ry = randFloat(rng) * WORLD_HEIGHT;
            queueOrganism(dna, rx, ry, 390.0f);
        }
    }

    int getGridKey(float x, float y) {
        int gx = (int)std::floor(x / SPATIAL_GRID_SIZE);
        int gy = (int)std::floor(y / SPATIAL_GRID_SIZE);
        return gx + gy * 10000;
    }

    void updateTick() {
        float dt = (1.0f / 60.0f) * timeScale;
        if (dt <= 0.0f) return;
        worldTime += dt;
    
        // 1. Build Spatial Hash Grid
        std::unordered_map<int, std::vector<OrganismRecord*>> spatialGrid;
        for (auto* org : population) {
            if (org->isAlive && !org->points.empty()) {
                spatialGrid[getGridKey(org->points[0].x, org->points[0].y)].push_back(org);
            }
        }
    
        // 2. Main Parallel Simulation Loop
        for (auto* org : population) {
            pool.enqueue([&, org]() {
                if (org->markedForDeletion) return; 
        
                if (!org->isAlive) {
                    org->energy -= 10.0f * dt;
                    if (org->energy < -50.0f) org->markedForDeletion = true;
                } else {
                    org->age += dt;
                    if (org->reproCooldown > 0) org->reproCooldown -= dt;
                    if (org->damageFlash > 0) org->damageFlash -= dt;
                    if (org->age > org->dna.lifespan) org->energy = 0; // Natural Death
        
                    float netEnergy = 0.0f;
                    org->sensorFoodDistance = 1.0f; 
                    org->sensorHazardDistance = 1.0f;
                    org->hasShield = false;
                    
                    int shadeCount = 0; // Track overcrowding for plants

                    // --- Wrapped Proximity Checks ---
                    int curGx = (int)std::floor(org->points[0].x / SPATIAL_GRID_SIZE);
                    int curGy = (int)std::floor(org->points[0].y / SPATIAL_GRID_SIZE);
                    int gridCols = (int)std::ceil(WORLD_WIDTH / SPATIAL_GRID_SIZE);
                    int gridRows = (int)std::ceil(WORLD_HEIGHT / SPATIAL_GRID_SIZE);

                    for (int row = -1; row <= 1; row++) {
                        for (int col = -1; col <= 1; col++) {
                            int gx = (curGx + col + gridCols) % gridCols;
                            int gy = (curGy + row + gridRows) % gridRows;
                            auto it = spatialGrid.find(gx + gy * 10000);
                            
                            if (it != spatialGrid.end()) {
                                for (auto* other : it->second) {
                                    if (other == org || !other->isAlive) continue;
                                    
                                    float dx, dy;
                                    getToroidalDiff(org->points[0].x, org->points[0].y, other->points[0].x, other->points[0].y, dx, dy);
                                    float distSq = dx*dx + dy*dy;
                                    float dist = std::sqrt(distSq);
                                    
                                    // Check for plant overcrowding (Shade Penalty)
                                    if (dist < greenCrowdRadius && !other->sticks[0].isHidden && other->sticks[0].type == ColorType::GREEN) {
                                        shadeCount++;
                                    }
                                    
                                    if (dist < 20.0f) { 
                                        // 1. Vision Sensors 
                                        if (!other->sticks[0].isHidden) {
                                            if (other->sticks[0].type == ColorType::GREEN) 
                                                org->sensorFoodDistance = std::min(org->sensorFoodDistance, dist / 20.0f);
                                            if (other->sticks[0].type == ColorType::RED) 
                                                org->sensorHazardDistance = std::min(org->sensorHazardDistance, dist / 20.0f);
                                        }

                                        // 2. Physical Point-to-Point Pushing
                                        float pushRadius = 1.2f; 
                                        float pushRadiusSq = pushRadius * pushRadius;
                                        for (size_t i = 0; i < org->points.size(); ++i) {
                                            for (size_t j = 0; j < other->points.size(); ++j) {
                                                float px, py;
                                                getToroidalDiff(other->points[j].x, other->points[j].y, org->points[i].x, org->points[i].y, px, py);
                                                float pDistSq = px*px + py*py;
                                                
                                                if (pDistSq > 0.0001f && pDistSq < pushRadiusSq) {
                                                    float pDist = std::sqrt(pDistSq);
                                                    float overlap = pushRadius - pDist;
                                                    float force = overlap * 150.0f; // Soft-body spring repulsion
                                                    float fx = (px / pDist) * force;
                                                    float fy = (py / pDist) * force;
                                                    
                                                    std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                                    org->points[i].ax += fx;
                                                    org->points[i].ay += fy;
                                                    other->points[j].ax -= fx;
                                                    other->points[j].ay -= fy;
                                                }
                                            }
                                        }

                                        // 3. Combat Damage & Eating
                                        if (dist < 15.0f) { 
                                            for (const auto& attackerStick : org->sticks) {
                                                if (attackerStick.isHidden) continue;
                                                
                                                if (attackerStick.type == ColorType::RED) {
                                                    float sx, sy;
                                                    getToroidalDiff(org->points[attackerStick.p1_idx].x, org->points[attackerStick.p1_idx].y, 
                                                                    other->points[0].x, other->points[0].y, sx, sy);
                                                    
                                                    if (sx*sx + sy*sy < carnivoreAttackRange * carnivoreAttackRange) { 
                                                        std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                                        if (other->isAlive) {
                                                            float dmg = damageAmount * dt;
                                                            if (other->hasShield) {
                                                                dmg *= (1.0f - shieldEfficiency);
                                                                other->energy -= (dmg * 0.5f); 
                                                            }
                                                            other->energy -= dmg;
                                                            org->energy += dmg * carnivoreEfficiency; 
                                                            other->damageFlash = 0.2f;
                                                        }
                                                    }
                                                }
                                                else if (attackerStick.type == ColorType::WHITE) {
                                                    float sx, sy;
                                                    getToroidalDiff(org->points[attackerStick.p1_idx].x, org->points[attackerStick.p1_idx].y, 
                                                                    other->points[0].x, other->points[0].y, sx, sy);
                                                    
                                                    if (sx*sx + sy*sy < herbivoreAttackRange * herbivoreAttackRange) {
                                                        std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                                        if (other->isAlive) {
                                                            org->energy += herbivoreEatEnergy;
                                                            other->energy = -10.0f; 
                                                            other->isAlive = false;
                                                            other->markedForDeletion = true;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
        
                    // AI Processing & Movement
                    for (size_t i = 0; i < org->sticks.size(); i++) {
                        if (org->sticks[i].isHidden) continue;

                        int geneIdx = org->sticks[i].gene_idx;
                        if (geneIdx < 0 || geneIdx >= (int)org->dna.genes.size()) continue;
                        Gene& gene = org->dna.genes[geneIdx];

                        if (org->sticks[i].type == ColorType::GREEN) {
                            // Apply shade penalty
                            float actualPhotoRate = photosynthesisRate * std::max(0.0f, 1.0f - (shadeCount * shadePenalty));
                            netEnergy += actualPhotoRate * org->sticks[i].rest_length * timeScale;
                        } else if (org->sticks[i].type == ColorType::PURPLE) {
                            org->hasShield = true; 
                            netEnergy -= shieldCost * timeScale;
                        } else {
                            // Calculate upkeep based on segment cost and size discount
                            float currentCost = std::max(0.001f, baseMetabolism + segmentCost - (sizeDiscount * org->sticks.size()));
                            netEnergy -= currentCost * org->sticks[i].rest_length * timeScale;
                            
                            float neuralSignal = ((1.0f - org->sensorFoodDistance) * gene.weight_FoodSensor) +
                                                 ((1.0f - org->sensorHazardDistance) * gene.weight_HazardSensor) + 
                                                 gene.bias;
        
                            // Flex Motorized Joints using their invisible structural brace
                            if (org->sticks[i].isMotorized && org->sticks[i].brace_idx != -1) {
                                int bIdx = org->sticks[i].brace_idx;
                                float targetFlex = std::clamp(neuralSignal, -1.0f, 1.0f);
                                
                                org->sticks[bIdx].rest_length = org->sticks[bIdx].base_length + targetFlex * org->sticks[bIdx].flex_range;
                                netEnergy -= std::abs(targetFlex) * movementCost * 0.5f * timeScale;
                            }

                            if (neuralSignal > 0.0f && org->points.size() > 1) {
                                Point& p1 = org->points[org->sticks[i].p1_idx];
                                Point& p2 = org->points[org->sticks[i].p2_idx];
                                
                                float sdx, sdy;
                                getToroidalDiff(p1.x, p1.y, p2.x, p2.y, sdx, sdy);
                                float len = std::sqrt(sdx*sdx + sdy*sdy);
                                if (len > 0.001f) { sdx /= len; sdy /= len; }
        
                                if (org->sticks[i].type == ColorType::YELLOW) { // Thrust
                                    float thrust = gene.param1 * neuralSignal * thrustMultiplier * 200.0f;
                                    p2.ax += sdx * thrust; p2.ay += sdy * thrust;
                                    netEnergy -= std::abs(thrust / 200.0f) * movementCost * timeScale;
                                } else if (org->sticks[i].type == ColorType::BLUE) { // Torque
                                    float torque = gene.param2 * neuralSignal * turnMultiplier * 100.0f;
                                    p2.ax += -sdy * torque; p2.ay += sdx * torque;
                                    p1.ax -= -sdy * torque; p1.ay += sdx * torque;
                                    netEnergy -= std::abs(torque / 100.0f) * movementCost * timeScale;
                                }
                            }
                        }
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(org->orgMutex);
                        org->energy += netEnergy;
                        if (org->energy > 800.0f) org->energy = 800.0f; 
                        if (org->energy <= 0) { 
                            org->isAlive = false; 
                            for(auto& s : org->sticks) s.type = ColorType::DEAD; 
                        }
                    }
        
                    // Reproduction
                    if (org->isAlive && org->energy >= 300.0f && org->reproCooldown <= 0.0f) {
                        float spawnChance = (org->energy - 300.0f) / 300.0f; 
                        if (randFloat(rng) < spawnChance * dt * 2.0f) { 
                            std::lock_guard<std::mutex> lock(spawnMutex);
                            if (population.size() + spawnRequests.size() < (size_t)maxPopulation) {
                                org->energy -= 200.0f;
                                org->reproCooldown = 15.0f; 

                                float angle = randFloat(rng) * 6.283f;
                                float spawnX = std::fmod(org->points[0].x + std::cos(angle) * 8.0f + WORLD_WIDTH, WORLD_WIDTH);
                                float spawnY = std::fmod(org->points[0].y + std::sin(angle) * 8.0f + WORLD_HEIGHT, WORLD_HEIGHT);
                                
                                spawnRequests.push_back({mutateGenome(org->dna), spawnX, spawnY, 200.0f}); 
                            }
                        }
                    }
                } 
        
                // --- 3. Verlet Integration with Velocity Clamp & World Wrap ---
                for (auto& p : org->points) {
                    float vx = (p.x - p.old_x) * 0.95f; 
                    float vy = (p.y - p.old_y) * 0.95f;

                    float maxSpeed = 25.0f;
                    float speedSq = vx*vx + vy*vy;
                    if (speedSq > maxSpeed * maxSpeed) {
                        float ratio = maxSpeed / std::sqrt(speedSq);
                        vx *= ratio; vy *= ratio;
                    }

                    p.old_x = p.x; p.old_y = p.y;
                    p.x += vx + p.ax * dt * dt;
                    p.y += vy + p.ay * dt * dt;
                    p.ax = 0; p.ay = 0;

                    if (p.x < 0) { p.x += WORLD_WIDTH; p.old_x += WORLD_WIDTH; }
                    else if (p.x > WORLD_WIDTH) { p.x -= WORLD_WIDTH; p.old_x -= WORLD_WIDTH; }
                    if (p.y < 0) { p.y += WORLD_HEIGHT; p.old_y += WORLD_HEIGHT; }
                    else if (p.y > WORLD_HEIGHT) { p.y -= WORLD_HEIGHT; p.old_y -= WORLD_HEIGHT; }
                }
        
                // --- 4. Toroidal Constraint Solving (Rigidity) ---
                for (int iter = 0; iter < 4; iter++) {
                    for (auto& stick : org->sticks) {
                        Point& p1 = org->points[stick.p1_idx];
                        Point& p2 = org->points[stick.p2_idx];
                        float dx, dy;
                        getToroidalDiff(p1.x, p1.y, p2.x, p2.y, dx, dy);
                        float dist = std::sqrt(dx*dx + dy*dy);
                        if (dist == 0) continue;
                        float diff = (stick.rest_length - dist) / dist * 0.5f;
                        p1.x -= dx * diff; p1.y -= dy * diff;
                        p2.x += dx * diff; p2.y += dy * diff;
                    }
                }
            }); 
        }
        
        // Wait for all organism tasks to finish before proceeding to cleanup
        pool.waitAll();
    
        // 3. Serial Cleanup & Spawning
        for (auto& req : spawnRequests) queueOrganism(req.dna, req.x, req.y, req.energy);
        spawnRequests.clear();
    
        population.erase(std::remove_if(population.begin(), population.end(), [](OrganismRecord* org) {
            if (org->markedForDeletion) { delete org; return true; }
            return false;
        }), population.end());
    }
};