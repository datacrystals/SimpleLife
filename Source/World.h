/**
 * @file World.h
 * @brief The simulation coordinator. 100% headless and serializable.
 */
#pragma once
#include "SimConfig.h"
#include "Physics/PhysicsEngine.h"
#include "Biology/Organism.h"
#include "Biology/Serialization.h"
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <execution>
#include <iostream>
#include <cmath> 
#include <filesystem>
#include <numeric>

class World {
public:
    SimConfig config;
    PhysicsEngine engine; 
    int nextOrgId = 1;
    float worldTime = 0.0f; 

    int selectedOrgId = -1;


    // Autosave config
    bool autosaveEnabled = true;
    float autosaveIntervalSeconds = 60.0f; 
    float timeSinceLastSave = 0.0f;
    std::string worldSessionName = "vespers_garden";

public:
    std::vector<std::unique_ptr<Organism>> population;

    World(const SimConfig& cfg) : config(cfg), engine(cfg.worldWidth, cfg.worldHeight) {}

    SimConfig& getConfig() { return config; }

    void initializeEden() {
        population.clear();
        Genome edenDNA;
    
        // --- 1. MORPHOLOGY (Spine, Ribs, Muscles, Green Whiskers) ---
        //
        edenDNA.morphology.push_back({ColorType::WHITE, 0, -1, 5.0f, 0.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 5.0f, 0.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 3.0f, 90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 3.0f, -90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 2, -1, 3.0f, 90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 2, -1, 3.0f, -90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::DEAD, 1, 5, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 2, 3, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 1, 6, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 2, 4, 0.0f, 0.0f, false, -1, 0.0f});
        // Muscles linked to specific IO IDs (2 and 3)
        edenDNA.morphology.push_back({ColorType::YELLOW, 3, 5, 0.0f, 0.0f, true, 2, 0.0f});
        edenDNA.morphology.push_back({ColorType::YELLOW, 4, 6, 0.0f, 0.0f, true, 3, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 3, -1, 4.0f, 135.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 4, -1, 4.0f, -135.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 5, -1, 4.0f, 45.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 6, -1, 4.0f, -45.0f, false, -1, 0.0f});
    
        edenDNA.initializeBaseBrain();
    
        // --- 4. POPULATION SPAWN ---
        //
        for (int i = 0; i < 20; ++i) {
            float x = (rand() % (int)config.worldWidth);
            float y = (rand() % (int)config.worldHeight);
            population.push_back(std::make_unique<Organism>(nextOrgId++, edenDNA, config.startingEnergy, x, y));
        }
    }

    void updateTick() {
        float dt = config.physicsDt * config.timeScale;
        worldTime += dt; 
        
        std::vector<std::unique_ptr<Organism>> babies;
        std::mutex babyMutex; 

        // --- AUTOSAVE LOGIC ---
        if (autosaveEnabled) {
            timeSinceLastSave += dt;
            if (timeSinceLastSave >= autosaveIntervalSeconds) {
                // NEW: Logic for organized, non-destructive autosaving
                saveWorldState("", true); 
                timeSinceLastSave = 0.0f;
            }
        }
        // 1. REFRESH ORGANISM GRID (Sequential but fast)
        engine.clearOrgGrid();
        for (auto& org : population) {
            if (org->isAlive) engine.addOrgToGrid(org->points[0].x, org->points[0].y, org.get());
        }

        // 2. BIOLOGY & PREDATION (64-CORE PARALLEL)
        std::for_each(std::execution::par, population.begin(), population.end(), [&](auto& org) {
            if (!org->isAlive) return;

            // Density for "Shade" penalty
            float density = engine.getNearbyCount(org->points[0].x, org->points[0].y, 30.0f);
            float shade = 0.0f;//std::min(1.0f, density / 12.0f); 

            // Combat Pass (Red segments)
            float combatGain = 0.0f;
            for (size_t i = 0; i < org->bodyParts.size(); ++i) {
                if (org->bodyParts[i].type == ColorType::RED) {
                    auto& spike = org->points[org->springs[i].p2_idx];
                    auto victims = engine.getNearbyOrganisms(spike.x, spike.y, 10.0f);
                    for (auto* victim : victims) {
                        if (victim->id == org->id) continue;
                        float damage = config.carnivoreDamagePerSec * dt;
                        std::lock_guard<std::mutex> lock(victim->orgMutex); // Thread-safe bite
                        victim->energy -= damage;
                        combatGain += damage * 0.75f; 
                    }
                }
            }

            // Brain Input: 0 = Energy, 1 = Local Population Density
            org->brain.setSensoryInputs({ org->energy * 0.01f, density * 0.1f });
            org->updateBiology(dt, config, shade, combatGain);

            // 5. REPRODUCTION 
            if (org->isAlive && org->energy >= config.reproductionEnergyThreshold && org->reproCooldown <= 0.0f) {
                org->energy -= config.reproductionEnergyThreshold * 0.5f; // Pay the energy cost of birth
                org->reproCooldown = 15.0f; // Reset cooldown so they don't explode with babies

                Genome childDNA = org->dna;
                childDNA.mutate(config);


                // Pre-Selection: Only spawn if the brain has at least one sensory-to-motor path
                bool hasPath = false;
                while (!hasPath) {
                    for (const auto& syn : childDNA.synapses) {
                        // Very simple check: Does a synapse exist between different roles?
                        if (syn.weight != 0) { hasPath = true; break; }
                    }
                }


                // Spawn the baby slightly offset from the parent
                // Note: rand() can cause thread contention. For a production-grade 
                // parallel loop, use a thread_local std::mt19937, but this works for now.
                float offsetX = ((rand() % 100) / 100.0f * 40.0f) - 20.0f;
                float offsetY = ((rand() % 100) / 100.0f * 40.0f) - 20.0f;
                float childX = org->points[0].x + offsetX;
                float childY = org->points[0].y + offsetY;

                // Lock the mutex ONLY for the shared resource updates to keep the loop fast
                std::lock_guard<std::mutex> lock(babyMutex);
                babies.push_back(std::make_unique<Organism>(nextOrgId++, childDNA, config.startingEnergy, childX, childY));
            }
        });

        // 2. Pure Physics Updates - Integration & Constraints (MULTI-THREADED)
        std::for_each(std::execution::par, population.begin(), population.end(), [&](auto& org) {
            engine.step(org->points, org->springs, dt);
        });

        // 2.5 Gather points for the spatial grid (100% LOCK-FREE, NO ATOMICS)
        // Sequential offset calculation avoids cache coherency deadlocks
        std::vector<size_t> offsets(population.size());
        size_t totalPoints = 0;
        for (size_t i = 0; i < population.size(); ++i) {
            offsets[i] = totalPoints;
            totalPoints += population[i]->points.size();
        }
        
        std::vector<PhysicsPoint*> allPoints(totalPoints);
        std::vector<size_t> indices(population.size());
        std::iota(indices.begin(), indices.end(), 0);

        // Threads write to perfectly isolated array indices
        std::for_each(std::execution::par, indices.begin(), indices.end(), [&](size_t i) {
            size_t writeIdx = offsets[i];
            for (auto& pt : population[i]->points) {
                pt.parentOrgId = population[i]->id;
                allPoints[writeIdx++] = &pt;
            }
        });

        // 2.6 Resolve overlaps globally
        engine.resolveGlobalCollisions(allPoints, 2.0f, 50.0f, dt);

        // 3. Introduce Babies to the World (SEQUENTIAL)
        for (auto& baby : babies) {
            population.push_back(std::move(baby));
        }

        // 4. Cleanup Dead Bodies (SEQUENTIAL)
        population.erase(std::remove_if(population.begin(), population.end(), [](const std::unique_ptr<Organism>& org) {
            return org->markedForDeletion || (!org->isAlive && org->energy < -50.0f);
        }), population.end());
    }

    void clearWorld() {
        population.clear();
    }

    void spawnSimpleGreen(float x, float y) {
        Genome greenDNA;
        greenDNA.initializeBaseBrain(); // <-- Add this
        greenDNA.morphology.push_back({ColorType::GREEN, 0, -1, 5.0f, 0.0f, false, -1, 0.0f});
        population.push_back(std::make_unique<Organism>(nextOrgId++, greenDNA, config.startingEnergy, x, y));
    }

    void spawnWorm(float x, float y) {
        Genome wormDNA;
        wormDNA.initializeBaseBrain();  // <-- Add this
        
        // Use existing motor IDs generated by the baseline brain
        int m1 = wormDNA.neurons.size() > 0 ? wormDNA.neurons.back().id : 0; // fallback if empty
        
        wormDNA.morphology.push_back({ColorType::YELLOW, 0, -1, 4.0f, 0.0f, true, m1, 0.0f});
        wormDNA.morphology.push_back({ColorType::YELLOW, 1, -1, 4.0f, 0.0f, true, m1, 0.0f});
        wormDNA.morphology.push_back({ColorType::YELLOW, 2, -1, 4.0f, 0.0f, true, m1, 0.0f});

        population.push_back(std::make_unique<Organism>(nextOrgId++, wormDNA, config.startingEnergy, x, y));
    }















    // ==========================================
    // SAVE / LOAD LOGIC
    // ==========================================



    void loadWorldState(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open save file: " << filepath << "\n";
            return;
        }

        json jWorld;
        file >> jWorld;

        clearWorld(); // Wipe the current simulation

        safe_get(jWorld, "worldTime", worldTime);
        safe_get(jWorld, "nextOrgId", nextOrgId);

        if (jWorld.contains("population")) {
            for (const auto& jOrg : jWorld["population"]) {
                int id = 0; float energy = config.startingEnergy, age = 0.0f, x = 0.0f, y = 0.0f;
                Genome dna;

                safe_get(jOrg, "id", id);
                safe_get(jOrg, "energy", energy);
                safe_get(jOrg, "age", age);
                safe_get(jOrg, "x", x);
                safe_get(jOrg, "y", y);
                safe_get(jOrg, "dna", dna);

                // Reconstruct the organism. The constructor will call buildPhenotype()
                auto org = std::make_unique<Organism>(id, dna, energy, x, y);
                org->age = age;
                population.push_back(std::move(org));
                
                // Keep nextOrgId ahead of any loaded IDs to prevent collisions
                if (id >= nextOrgId) nextOrgId = id + 1; 
            }
        }
        std::cout << "[World] Loaded " << population.size() << " organisms.\n";
    }

    // Helper to generate a unique timestamp string
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
        return ss.str();
    }

    void saveWorldState(std::string customName = "", bool isAutosave = false) {
        std::string folder = isAutosave ? "autosaves/" + worldSessionName : "saves/worlds";
        std::filesystem::create_directories(folder);

        std::string filename = customName.empty() ? "world_" + getTimestamp() + ".json" : customName;
        std::string fullPath = folder + "/" + filename;


        json jWorld;
        jWorld["worldTime"] = worldTime;
        jWorld["nextOrgId"] = nextOrgId;

        json jPop = json::array();
        for (const auto& org : population) {
            if (!org->isAlive || org->markedForDeletion) continue;
            
            json jOrg;
            jOrg["id"] = org->id;
            jOrg["energy"] = org->energy;
            jOrg["age"] = org->age;
            // Save the center-mass/root node position to spawn them back in the right spot
            jOrg["x"] = org->points.empty() ? 0.0f : org->points[0].x;
            jOrg["y"] = org->points.empty() ? 0.0f : org->points[0].y;
            jOrg["dna"] = org->dna;
            
            jPop.push_back(jOrg);
        }
        jWorld["population"] = jPop;


        // Use fullPath for the std::ofstream
        std::ofstream file(fullPath);
        if (file.is_open()) {
            file << std::setw(4) << jWorld << std::endl;
            std::cout << "[World] Saved to: " << fullPath << "\n";
        }
    }

    void exportOrganism(int targetId, std::string customName = "") {
        std::filesystem::create_directories("saves/creatures");
        
        std::string filename = customName.empty() ? 
            "org_" + std::to_string(targetId) + "_" + getTimestamp() + ".json" : customName;
        std::string fullPath = "saves/creatures/" + filename;

        for (const auto& org : population) {
            if (org->id == targetId) {
                json jOrg = org->dna;
                std::ofstream file(fullPath);
                if (file.is_open()) file << std::setw(4) << jOrg << std::endl;
                return;
            }
        }
    }

    // Load an exported organism blueprint and spawn it into the current world
    void importOrganism(const std::string& filepath, float x, float y) {
        std::ifstream file(filepath);
        if (!file.is_open()) return;

        json jOrg;
        file >> jOrg;

        Genome dna = jOrg.get<Genome>();
        population.push_back(std::make_unique<Organism>(nextOrgId++, dna, config.startingEnergy, x, y));
    }


















};