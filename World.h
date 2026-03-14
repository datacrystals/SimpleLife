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
#include <cmath> 

class World {
private:
    SimConfig config;
    PhysicsEngine engine; // Namespace dropped!
    int nextOrgId = 1;
    float worldTime = 0.0f; 

public:
    std::vector<std::unique_ptr<Organism>> population;

    World(const SimConfig& cfg) : config(cfg), engine(cfg.worldWidth, cfg.worldHeight) {}

    SimConfig& getConfig() { return config; }

    void initializeEden() {
        population.clear();
        Genome edenDNA;

        // --- BONES (The Spine) ---
        // Node 0 to 1 (Spine 1)
        edenDNA.morphology.push_back({ColorType::WHITE, 0, -1, 5.0f, 0.0f, false, -1, 0.0f});
        // Node 1 to 2 (Spine 2)
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 5.0f, 0.0f, false, -1, 0.0f});

        // --- BONES (The Ribs) ---
        // Node 1 to 3 (Top Rib)
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 3.0f, 90.0f, false, -1, 0.0f});
        // Node 1 to 4 (Bottom Rib)
        edenDNA.morphology.push_back({ColorType::WHITE, 1, -1, 3.0f, -90.0f, false, -1, 0.0f});
        // Node 2 to 5 (Top Rib 2)
        edenDNA.morphology.push_back({ColorType::WHITE, 2, -1, 3.0f, 90.0f, false, -1, 0.0f});
        // Node 2 to 6 (Bottom Rib 2)
        edenDNA.morphology.push_back({ColorType::WHITE, 2, -1, 3.0f, -90.0f, false, -1, 0.0f});

        // --- STRUCTURAL BRACES (Invisible scaffolding to stop the skeleton from folding) ---
        // Cross-brace the top ribs to the opposite spine nodes
        edenDNA.morphology.push_back({ColorType::DEAD, 1, 5, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 2, 3, 0.0f, 0.0f, false, -1, 0.0f}); 
        // Cross-brace the bottom ribs to the opposite spine nodes
        edenDNA.morphology.push_back({ColorType::DEAD, 1, 6, 0.0f, 0.0f, false, -1, 0.0f}); 
        edenDNA.morphology.push_back({ColorType::DEAD, 2, 4, 0.0f, 0.0f, false, -1, 0.0f});

        // --- MUSCLES ---
        // Top Muscle (Driven by Motor Neuron ID 2)
        edenDNA.morphology.push_back({ColorType::YELLOW, 3, 5, 0.0f, 0.0f, true, 2, 0.0f});
        // Bottom Muscle (Driven by Motor Neuron ID 3)
        edenDNA.morphology.push_back({ColorType::YELLOW, 4, 6, 0.0f, 0.0f, true, 3, 0.0f});

        // --- GREEN WHISKERS ---
        // Attached to the ribs, hanging loosely
        edenDNA.morphology.push_back({ColorType::GREEN, 3, -1, 4.0f, 135.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 4, -1, 4.0f, -135.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 5, -1, 4.0f, 45.0f, false, -1, 0.0f});
        edenDNA.morphology.push_back({ColorType::GREEN, 6, -1, 4.0f, -45.0f, false, -1, 0.0f});

        // --- SNN CONNECTOME ---
        // N0: Sensory (Receives Sine Wave A)
        edenDNA.neurons.push_back({0, NeuronRole::SENSORY, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        // N1: Sensory (Receives Sine Wave B)
        edenDNA.neurons.push_back({1, NeuronRole::SENSORY, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        
        // N2: Motor Top Muscle
        edenDNA.neurons.push_back({2, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.2f, 0.0f});
        // N3: Motor Bottom Muscle
        edenDNA.neurons.push_back({3, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.2f, 0.0f});

        // Synapses: Sensor A (0) drives Top Muscle (2), Sensor B (1) drives Bottom Muscle (3)
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
        
        // Temporary vector to hold babies so we don't invalidate iterators
        std::vector<std::unique_ptr<Organism>> babies;

        // 1. Biological Updates & Reproduction
        for (auto& org : population) {
            // Provide alternating sine wave sensory data to simulate a central pattern generator
            std::vector<float> sensors = { 
                std::max(0.0f, (float)std::sin(worldTime * 5.0f) * 2.0f),  
                std::max(0.0f, (float)-std::sin(worldTime * 5.0f) * 2.0f)  
            }; 
            org->brain.setSensoryInputs(sensors);
            
            org->updateBiology(dt, config);

            // Reproduction Logic
            if (org->isAlive && 
                org->energy >= config.reproductionEnergyThreshold && 
                org->reproCooldown <= 0.0f &&
                (population.size() + babies.size()) < (size_t)config.maxPopulation) 
            {
                // Pay the energy cost
                org->energy -= config.reproductionEnergyCost;
                org->reproCooldown = config.reproductionCooldown;
    
                // Clone the genome
                Genome childDNA = org->dna; 

                childDNA.mutate(config);
    
                // Spawn the baby slightly offset from the parent
                float childX = org->points[0].x + 10.0f;
                float childY = org->points[0].y + 10.0f;
                
                babies.push_back(std::make_unique<Organism>(
                    nextOrgId++, 
                    childDNA, 
                    config.startingEnergy, 
                    childX, 
                    childY
                ));
            }
        }

        // 2. Pure Physics Updates
        std::vector<PhysicsPoint*> allPoints;

        for (auto& org : population) {
            engine.step(org->points, org->springs, dt);
            
            // Gather points for the spatial grid and assign IDs
            for (auto& pt : org->points) {
                pt.parentOrgId = org->id; 
                allPoints.push_back(&pt);
            }
        }

        // 2.5 Resolve overlaps globally
        engine.resolveGlobalCollisions(allPoints, 2.0f, 50.0f, dt);

        // 3. Introduce Babies to the World
        for (auto& baby : babies) {
            population.push_back(std::move(baby));
        }

        // 4. Cleanup Dead Bodies
        population.erase(std::remove_if(population.begin(), population.end(), [](const std::unique_ptr<Organism>& org) {
            return org->markedForDeletion || (!org->isAlive && org->energy < -50.0f);
        }), population.end());
    }
};