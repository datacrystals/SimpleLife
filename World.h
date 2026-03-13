#pragma once
#include "Types.h"
#include <execution>
#include <random>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cmath>
#include <algorithm>

// Define world boundaries
#define WORLD_WIDTH 200.0f
#define WORLD_HEIGHT 140.0f

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
    
    // Evolution & Balance Parameters
    float mutationRate = 0.05f;      
    float baseMetabolism = 0.1f;
    float photosynthesisRate = 1.0f; 
    float movementCost = 0.004f;
    float thrustMultiplier = 0.5f;
    float turnMultiplier = 0.03f;

    // Shield & Combat Balance
    float shieldEfficiency = 0.7f; // Reduces damage by 70%
    float shieldCost = 0.05f;      // Extra energy drain per shield segment
    float damageAmount = 80.0f;    // Base damage per second

    World() { initEden(); }

    // --- Helper for Toroidal (Wrapped) Math ---
    // Calculates the shortest distance between two points in a wrapped world
    static void getToroidalDiff(float x1, float y1, float x2, float y2, float& dx, float& dy) {
        dx = x2 - x1;
        dy = y2 - y1;
        if (dx > WORLD_WIDTH * 0.5f) dx -= WORLD_WIDTH;
        else if (dx < -WORLD_WIDTH * 0.5f) dx += WORLD_WIDTH;
        if (dy > WORLD_HEIGHT * 0.5f) dy -= WORLD_HEIGHT;
        else if (dy < -WORLD_HEIGHT * 0.5f) dy += WORLD_HEIGHT;
    }

    Genome mutateGenome(Genome parentDna) {
        Genome child = parentDna;
        for (auto& gene : child) {
            if (randFloat(rng) < mutationRate) { 
                if (randFloat(rng) < 0.2f) gene.type = static_cast<ColorType>(rand() % 6);
                gene.length += (randFloat(rng) - 0.5f) * 0.5f; 
                gene.param1 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.param2 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.weight_FoodSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.weight_HazardSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.bias += (randFloat(rng) - 0.5f) * 1.0f;
                if (gene.length < 0.2f) gene.length = 0.2f; 
            }
        }
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
        // Start with a small seed population
        for (int i=0; i<10; i++) {
            Genome dna = { {ColorType::GREEN, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1, 0.0f} };
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
        std::for_each(std::execution::par_unseq, population.begin(), population.end(), [&](OrganismRecord* org) {
            if (org->markedForDeletion) return; 
    
            if (!org->isAlive) {
                org->energy -= 10.0f * dt;
                if (org->energy < -50.0f) org->markedForDeletion = true;
            } else {
                org->age += dt;
                if (org->reproCooldown > 0) org->reproCooldown -= dt;
                if (org->damageFlash > 0) org->damageFlash -= dt;
                if (org->age > maxLifespan) org->energy = 0;
    
                float netEnergy = 0.0f;
                org->sensorFoodDistance = 1.0f; 
                org->sensorHazardDistance = 1.0f;
                org->hasShield = false;

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
                                
                                if (dist < 2.0f) { 
                                    // 1. Damage & Energy Theft (Red sticks)
                                    // We iterate through all sticks of the 'org' to see if any are RED
                                    for (const auto& attackerStick : org->sticks) {
                                        if (attackerStick.type == ColorType::RED) {
                                            // Find distance between the attacker's stick and the other organism's head (or all points)
                                            float dx, dy;
                                            getToroidalDiff(org->points[attackerStick.p1_idx].x, org->points[attackerStick.p1_idx].y, 
                                                            other->points[0].x, other->points[0].y, dx, dy);
                                            float distSq = dx*dx + dy*dy;

                                            if (distSq < 4.0f) { // Collision threshold
                                                std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                                if (other->isAlive) {
                                                    float dmg = damageAmount * dt;
                                                    
                                                    // Shields reduce damage but cost the victim extra energy to "absorb"
                                                    if (other->hasShield) {
                                                        dmg *= (1.0f - shieldEfficiency);
                                                        other->energy -= (dmg * 0.5f); // Extra penalty for blocking
                                                    }

                                                    other->energy -= dmg;
                                                    org->energy += dmg * 0.8f; // Efficient energy transfer
                                                    other->damageFlash = 0.2f;
                                                }
                                            }
                                        }
                                    }

                                    // 2. Direct Consumption (White sticks / Mouths)
                                    for (const auto& mouthStick : org->sticks) {
                                        if (mouthStick.type == ColorType::WHITE) {
                                            float dx, dy;
                                            getToroidalDiff(org->points[mouthStick.p1_idx].x, org->points[mouthStick.p1_idx].y, 
                                                            other->points[0].x, other->points[0].y, dx, dy);
                                            if ((dx*dx + dy*dy) < 4.0f) {
                                                std::scoped_lock lock(org->orgMutex, other->orgMutex);
                                                if (other->isAlive) {
                                                    // Instantly kill and eat if it's a mouth-to-body contact
                                                    org->energy += 150.0f;
                                                    other->energy = -10.0f; 
                                                    other->isAlive = false;
                                                    other->markedForDeletion = true;
                                                }
                                            }
                                        }
                                    }
                                } else if (dist < 20.0f) { // Vision
                                    if (other->sticks[0].type == ColorType::GREEN) 
                                        org->sensorFoodDistance = std::min(org->sensorFoodDistance, dist / 20.0f);
                                    if (other->sticks[0].type == ColorType::RED) 
                                        org->sensorHazardDistance = std::min(org->sensorHazardDistance, dist / 20.0f);
                                }
                            }
                        }
                    }
                }
    
                // AI Processing & Shield State
                for (size_t i = 0; i < org->sticks.size(); i++) {
                    if (org->sticks[i].type == ColorType::GREEN) {
                        netEnergy += photosynthesisRate * org->sticks[i].rest_length * timeScale;
                    } else if (org->sticks[i].type == ColorType::PURPLE) {
                        org->hasShield = true; // Active defense
                        netEnergy -= shieldCost * timeScale;
                    } else {
                        netEnergy -= baseMetabolism * org->sticks[i].rest_length * timeScale;
                        
                        float neuralSignal = ((1.0f - org->sensorFoodDistance) * org->dna[i].weight_FoodSensor) +
                                             ((1.0f - org->sensorHazardDistance) * org->dna[i].weight_HazardSensor) + 
                                             org->dna[i].bias;
    
                        if (neuralSignal > 0.0f && org->points.size() > 1) {
                            Point& head = org->points[0];
                            Point& tail = org->points[1];
                            float dx, dy;
                            getToroidalDiff(tail.x, tail.y, head.x, head.y, dx, dy);
                            float len = std::sqrt(dx*dx + dy*dy);
                            if (len > 0.001f) { dx /= len; dy /= len; }
    
                            if (org->sticks[i].type == ColorType::YELLOW) { // Thrust
                                float thrust = org->dna[i].param1 * neuralSignal * thrustMultiplier * 200.0f;
                                head.ax += dx * thrust; head.ay += dy * thrust;
                                netEnergy -= std::abs(thrust / 200.0f) * movementCost * timeScale;
                            } else if (org->sticks[i].type == ColorType::BLUE) { // Torque
                                float torque = org->dna[i].param2 * neuralSignal * turnMultiplier * 100.0f;
                                head.ax += -dy * torque; head.ay += dx * torque;
                                tail.ax -= -dy * torque; tail.ay += dx * torque;
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
    
                // Inside updateTick loop, replacing the reproduction block:
                if (org->isAlive && org->energy >= 300.0f && org->reproCooldown <= 0.0f) {
                    // Probability scales linearly from 0% at 300 energy to ~3.3% per frame at 600 energy
                    float spawnChance = (org->energy - 300.0f) / 300.0f; 
                    
                    if (randFloat(rng) < spawnChance * dt * 2.0f) { 
                        std::lock_guard<std::mutex> lock(spawnMutex);
                        if (population.size() + spawnRequests.size() < (size_t)maxPopulation) {
                            org->energy -= 200.0f;
                            org->reproCooldown = 15.0f; 

                            float angle = randFloat(rng) * 6.283f;
                            // Ensure child spawns within wrapped world bounds
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

                // Speed limit to prevent "explosions"
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

                // Wrap around world borders - move both current and old to keep velocity
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
    
        // 3. Serial Cleanup & Spawning
        for (auto& req : spawnRequests) queueOrganism(req.dna, req.x, req.y, req.energy);
        spawnRequests.clear();
    
        population.erase(std::remove_if(population.begin(), population.end(), [](OrganismRecord* org) {
            if (org->markedForDeletion) { delete org; return true; }
            return false;
        }), population.end());
    }
};