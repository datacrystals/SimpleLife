/**
 * @file World.h
 * @brief The simulation coordinator. 100% headless and serializable.
 */
#pragma once
#include "SimConfig.h"
#include "PhysicsEngine.h"
#include "Organism.h"
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <execution>
#include <cmath> 
#include <numeric>

class World {
public:
    SimConfig config;
    PhysicsEngine engine; 
    int nextOrgId = 1;
    float worldTime = 0.0f; 

    int selectedOrgId = -1;

public:
    std::vector<std::unique_ptr<Organism>> population;

    World(const SimConfig& cfg) : config(cfg), engine(cfg.worldWidth, cfg.worldHeight) {}

    SimConfig& getConfig() { return config; }

    void initializeEden() {
        population.clear();
        Genome edenDNA;

        // --- BONES (The Spine) ---
        edenDNA.morphology.push_back({ColorType::WHITE, 0, -1, 5.0f, 0.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 5.0f, 0.0f, false, -1, 0.0f});

        // --- BONES (The Ribs) ---
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 3.0f, 90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 3.0f, -90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 2, -1, 3.0f, 90.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::WHITE, 2, -1, 3.0f, -90.0f, false, -1, 0.0f});

        // --- STRUCTURAL BRACES ---
        edenDNA.morphology.push_back({ColorType::DEAD, 1, 5, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 2, 3, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 1, 6, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 2, 4, 0.0f, 0.0f, false, -1, 0.0f});

        // --- MUSCLES ---
        edenDNA.morphology.push_back({ColorType::YELLOW, 3, 5, 0.0f, 0.0f, true, 2, 0.0f});
        edenDNA.morphology.push_back({ColorType::YELLOW, 4, 6, 0.0f, 0.0f, true, 3, 0.0f});

        // --- GREEN WHISKERS ---
        edenDNA.morphology.push_back({ColorType::GREEN, 3, -1, 4.0f, 135.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 4, -1, 4.0f, -135.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 5, -1, 4.0f, 45.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 6, -1, 4.0f, -45.0f, false, -1, 0.0f});

        // --- SNN CONNECTOME ---
        edenDNA.neurons.push_back({0, NeuronRole::SENSORY, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        edenDNA.neurons.push_back({1, NeuronRole::SENSORY, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        edenDNA.neurons.push_back({2, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.2f, 0.0f});
        edenDNA.neurons.push_back({3, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.2f, 0.0f});

        edenDNA.synapses.push_back({0, 2, 1.5f}); 
        edenDNA.synapses.push_back({1, 3, 1.5f});

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
            float shade = std::min(1.0f, density / 12.0f); 

            // Combat Pass (Red segments)
            float combatGain = 0.0f;
            for (size_t i = 0; i < org->bodyParts.size(); ++i) {
                if (org->bodyParts[i].type == ColorType::RED) {
                    auto& spike = org->points[org->springs[i].p2_idx];
                    auto victims = engine.getNearbyOrganisms(spike.x, spike.y, 10.0f);
                    for (auto* victim : victims) {
                        if (victim->id == org->id) continue;
                        float damage = 60.0f * dt;
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
        greenDNA.morphology.push_back({ColorType::GREEN, 0, -1, 5.0f, 0.0f, false, -1, 0.0f});
        population.push_back(std::make_unique<Organism>(nextOrgId++, greenDNA, config.startingEnergy, x, y));
    }

    void spawnWorm(float x, float y) {
        Genome wormDNA;
        wormDNA.morphology.push_back({ColorType::YELLOW, 0, -1, 4.0f, 0.0f, true, 0, 0.0f});
        wormDNA.morphology.push_back({ColorType::YELLOW, 1, -1, 4.0f, 0.0f, true, 1, 0.0f});
        wormDNA.morphology.push_back({ColorType::YELLOW, 2, -1, 4.0f, 0.0f, true, 2, 0.0f});

        wormDNA.neurons.push_back({0, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        wormDNA.neurons.push_back({1, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        wormDNA.neurons.push_back({2, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});

        population.push_back(std::make_unique<Organism>(nextOrgId++, wormDNA, config.startingEnergy, x, y));
    }
};