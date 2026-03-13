#pragma once
#include "Types.h"
#include <execution>
#include <random>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cmath>

#define WORLD_WIDTH 200
#define WORLD_HEIGHT 140

extern std::mt19937 rng;
extern std::uniform_real_distribution<float> randFloat;

const float SPATIAL_GRID_SIZE = 19.0f;

class World {
public:
    std::vector<OrganismRecord*> population;
    struct SpawnRequest { Genome dna; float x; float y; float energy; };
    std::vector<SpawnRequest> spawnRequests;
    std::mutex spawnMutex;
    
    float worldTime = 0.0f;
    int nextOrgId = 1;
    
    float timeScale = 1.0f;          
    int maxPopulation = 2000;       
    float maxLifespan = 40.0f;
    
    float mutationRate = 0.05f;      
    float baseMetabolism = 0.1f;
    float photosynthesisRate = 1.0f; 
    float movementCost = 0.004f;
    float thrustMultiplier = 0.5f;
    float turnMultiplier = 0.03f;

    World() { initEden(); }

    Genome mutateGenome(Genome parentDna) {
        Genome child = parentDna;
        for (auto& gene : child) {
            if (randFloat(rng) < mutationRate) { 
                if (randFloat(rng) < 0.2f) gene.type = static_cast<ColorType>(rand() % 6);
                
                // FIXED: Restored all parameter mutations so they can evolve movement and AI again
                gene.length += (randFloat(rng) - 0.5f) * 0.5f; 
                gene.param1 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.param2 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.weight_FoodSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.weight_HazardSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.bias += (randFloat(rng) - 0.5f) * 1.0f;
                
                if (gene.length < 0.2f) gene.length = 0.2f; 
            }
        }
        
        // FIXED: Restored segment growth so they can become multi-limbed
        if (randFloat(rng) < (mutationRate / 2.0f) && child.size() < 10) {
            ColorType newType = static_cast<ColorType>(rand() % 6);
            int attachIdx = rand() % child.size(); 
            child.push_back({newType, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, attachIdx, (randFloat(rng) - 0.5f) * 90.0f});
        }
        
        return child;
    }

    void queueOrganism(Genome dna, float x, float y, float startingEnergy) {
        OrganismRecord* org = new OrganismRecord(nextOrgId++, dna, startingEnergy);
        org->points.push_back({x, y, x, y, 0, 0});
        
        for (size_t i = 0; i < dna.size(); i++) {
            Gene& g = dna[i];
            int pIdx = (g.parentIndex >= 0 && g.parentIndex < (int)i) ? g.parentIndex + 1 : org->points.size() - 1;
            
            float angle = g.branchAngle * (3.14159f / 180.0f);
            float newX = org->points[pIdx].x + std::cos(angle) * g.length;
            float newY = org->points[pIdx].y + std::sin(angle) * g.length;
            
            org->points.push_back({newX, newY, newX, newY, 0, 0});
            org->sticks.push_back({pIdx, (int)org->points.size() - 1, g.length, g.type, 1.0f});
        }
        population.push_back(org); 
    }

    void initEden() {
        population.clear();
        for (int i=0; i<1; i++) {
            Genome dna = { {ColorType::GREEN, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1, 0.0f} };
            queueOrganism(dna, (randFloat(rng)-0.5f)*10.0f, (randFloat(rng)-0.5f)*10.0f, 390.0f);
        }
    }

    int getGridKey(float x, float y) {
        return (int)(std::floor(x / SPATIAL_GRID_SIZE)) + (int)(std::floor(y / SPATIAL_GRID_SIZE)) * 10000;
    }

    void updateTick() {
        float dt = (1.0f / 60.0f) * timeScale;
        if (dt <= 0.0f) return;
        worldTime += dt;
    
        // 1. Build Spatial Hash Grid for optimized proximity checks
        std::unordered_map<int, std::vector<OrganismRecord*>> spatialGrid;
        for (auto* org : population) {
            if (org->isAlive && !org->points.empty()) {
                spatialGrid[getGridKey(org->points[0].x, org->points[0].y)].push_back(org);
            }
        }
    
        // 2. Main Parallel Simulation Loop
        std::for_each(std::execution::par_unseq, population.begin(), population.end(), [&](OrganismRecord* org) {
            if (org->markedForDeletion) return; 
    
            // Handle Dead Organisms: They decay but still need physics to fall/drift
            if (!org->isAlive) {
                org->energy -= 10.0f * dt;
                if (org->energy < -50.0f) org->markedForDeletion = true;
            } else {
                // Update Age and Cooldowns
                org->age += dt;
                if (org->reproCooldown > 0) org->reproCooldown -= dt;
                if (org->age > maxLifespan) org->energy = 0;
    
                float netEnergy = 0.0f;
                org->sensorFoodDistance = 1.0f; 
                org->sensorHazardDistance = 1.0f;
                int myKey = getGridKey(org->points[0].x, org->points[0].y);
                
                // Spatial Grid Proximity Checks (3x3 grid)
                int neighbors[9] = {0, 1, -1, 10000, -10000, 10001, -10001, 9999, -9999};
                for (int offset : neighbors) {
                    auto it = spatialGrid.find(myKey + offset);
                    if (it != spatialGrid.end()) {
                        for (auto* other : it->second) {
                            if (other == org || !other->isAlive) continue;
                            
                            float dx = other->points[0].x - org->points[0].x;
                            float dy = other->points[0].y - org->points[0].y;
                            float distSq = dx*dx + dy*dy;
                            float dist = std::sqrt(distSq);
                            
                            if (dist < 3.0f) { // Interaction range (Increased slightly to avoid cramming issues)
                                if (org->sticks[0].type == ColorType::WHITE && other->sticks[0].type == ColorType::GREEN) {
                                    std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                    if (other->isAlive) {
                                        org->energy += 120.0f;
                                        other->markedForDeletion = true;
                                        other->isAlive = false;
                                    }
                                }
                                if (org->sticks[0].type == ColorType::RED && other->sticks[0].type != ColorType::RED) {
                                    std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                    if (other->isAlive) {
                                        other->energy -= 60.0f;
                                        org->energy += 40.0f;
                                    }
                                }
                            } else if (dist < 15.0f) { // Vision range
                                if (other->sticks[0].type == ColorType::GREEN) 
                                    org->sensorFoodDistance = std::min(org->sensorFoodDistance, dist / 15.0f);
                                if (other->sticks[0].type == ColorType::RED) 
                                    org->sensorHazardDistance = std::min(org->sensorHazardDistance, dist / 15.0f);
                            }
                        }
                    }
                }
    
                // AI Processing & Physics Force Application
                for (size_t i = 0; i < org->sticks.size(); i++) {
                    if (org->sticks[i].type == ColorType::GREEN) {
                        netEnergy += photosynthesisRate * org->sticks[i].rest_length * timeScale;
                    } else {
                        netEnergy -= baseMetabolism * org->sticks[i].rest_length * timeScale;
                        
                        float neuralSignal = ((1.0f - org->sensorFoodDistance) * org->dna[i].weight_FoodSensor) +
                                             ((1.0f - org->sensorHazardDistance) * org->dna[i].weight_HazardSensor) + 
                                             org->dna[i].bias;
    
                        if (neuralSignal > 0.0f && org->points.size() > 1) {
                            Point& head = org->points[0];
                            Point& tail = org->points[1];
                            
                            float dirX = head.x - tail.x;
                            float dirY = head.y - tail.y;
                            float len = std::sqrt(dirX*dirX + dirY*dirY);
                            if (len > 0.001f) { dirX /= len; dirY /= len; }
    
                            if (org->sticks[i].type == ColorType::YELLOW) { // Thrust
                                float thrust = org->dna[i].param1 * neuralSignal * thrustMultiplier * 200.0f;
                                head.ax += dirX * thrust; head.ay += dirY * thrust;
                                netEnergy -= std::abs(thrust / 200.0f) * movementCost * timeScale;
                            } else if (org->sticks[i].type == ColorType::BLUE) { // Torque/Turn
                                float torque = org->dna[i].param2 * neuralSignal * turnMultiplier * 100.0f;
                                // Apply perpendicular forces to create rotation
                                head.ax += -dirY * torque; head.ay += dirX * torque;
                                tail.ax -= -dirY * torque; tail.ay -= dirX * torque;
                                netEnergy -= std::abs(torque / 100.0f) * movementCost * timeScale;
                            }
                        }
                    }
                }
                
                // Energy Management & Death Check
                {
                    std::lock_guard<std::mutex> lock(org->orgMutex);
                    org->energy += netEnergy;
                    if (org->energy > 800.0f) org->energy = 800.0f; 
                    if (org->energy <= 0) { 
                        org->isAlive = false; 
                        for(auto& s : org->sticks) s.type = ColorType::DEAD; 
                    }
                }
    
                // Reproduction Check (Now with Cooldown and Predictive Counting)
                if (org->energy >= 400.0f && org->reproCooldown <= 0.0f) {
                    std::lock_guard<std::mutex> lock(spawnMutex);
                    if (population.size() + spawnRequests.size() < maxPopulation) {
                        org->energy -= 200.0f;
                        org->reproCooldown = 15.0f; // 15s cooldown to prevent exponential cramming
    
                        // Spawn child with an ejection force to move them away from parent
                        float angle = randFloat(rng) * 2.0f * 3.14159f;
                        float dist = 8.0f; // Distance outside the "cramming" zone
                        float spawnX = org->points[0].x + std::cos(angle) * dist;
                        float spawnY = org->points[0].y + std::sin(angle) * dist;
                        
                        spawnRequests.push_back({mutateGenome(org->dna), spawnX, spawnY, 200.0f}); 
                    }
                }
            } 
    
            // --- Verlet Integration Phase ---
            const float maxVelocity = 15.0f; // Adjust based on your world scale
            const float maxVelSq = maxVelocity * maxVelocity;

            for (auto& p : org->points) {
                // 1. Calculate implicit velocity
                float vx = (p.x - p.old_x) * 0.95f; 
                float vy = (p.y - p.old_y) * 0.95f;
                
                // 2. Velocity Clamping
                float distSq = vx*vx + vy*vy;
                if (distSq > maxVelSq) {
                    float ratio = maxVelocity / std::sqrt(distSq);
                    vx *= ratio;
                    vy *= ratio;
                }

                // 3. Update positions
                p.old_x = p.x; 
                p.old_y = p.y;
                
                p.x += vx + p.ax * dt * dt;
                p.y += vy + p.ay * dt * dt;
                p.ax = 0; p.ay = 0;

                
                // 2. Wrap-around Logic
                // If we teleport the current position, we MUST teleport the old_x/y 
                // by the same amount so the "velocity" vector remains identical.
                
                if (p.x < 0) {
                    p.x += WORLD_WIDTH;
                    p.old_x += WORLD_WIDTH;
                } else if (p.x > WORLD_WIDTH) {
                    p.x -= WORLD_WIDTH;
                    p.old_x -= WORLD_WIDTH;
                }

                if (p.y < 0) {
                    p.y += WORLD_HEIGHT;
                    p.old_y += WORLD_HEIGHT;
                } else if (p.y > WORLD_HEIGHT) {
                    p.y -= WORLD_HEIGHT;
                    p.old_y -= WORLD_HEIGHT;
                }
            }
    
            // --- Constraint Solving Phase (Rigidity) ---
            for (int iter = 0; iter < 4; iter++) { // 4 iterations for stiffer bodies
                for (auto& stick : org->sticks) {
                    Point& p1 = org->points[stick.p1_idx];
                    Point& p2 = org->points[stick.p2_idx];
                    float dx = p2.x - p1.x; float dy = p2.y - p1.y;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist == 0) continue;
                    float diff = (stick.rest_length - dist) / dist * 0.5f;
                    float offsetX = dx * diff; float offsetY = dy * diff;
                    p1.x -= offsetX; p1.y -= offsetY;
                    p2.x += offsetX; p2.y += offsetY;
                }
            }
        }); 
    
        // 3. Serial Cleanup & Spawning
        for (auto& req : spawnRequests) queueOrganism(req.dna, req.x, req.y, req.energy);
        spawnRequests.clear();
    
        population.erase(std::remove_if(population.begin(), population.end(), [](OrganismRecord* org) {
            if (org->markedForDeletion) { delete org; return true; }
            return false;
        }), population.end());
    }
};