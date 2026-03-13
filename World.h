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

class World {
private:
    SimConfig config;
    PhysicsEngine engine;
    int nextOrgId = 1;

public:
    std::vector<std::unique_ptr<Organism>> population;

    World(const SimConfig& cfg) : config(cfg), engine(cfg.worldWidth, cfg.worldHeight) {}

    SimConfig& getConfig() { return config; }

    void initializeEden() {
        population.clear();
        // Create a basic starter genome
        Genome edenDNA;
        // 1. Bone 1 (Creates Node 1)
        edenDNA.morphology.push_back({ColorType::DEAD, 0, -1, 5.0f, 0.0f, false, -1});
        // 2. Bone 2 (Creates Node 2 off Node 1)
        edenDNA.morphology.push_back({ColorType::DEAD, 1, -1, 5.0f, 90.0f, false, -1});
        // 3. Muscle (Connects Node 0 to Node 2)
        edenDNA.morphology.push_back({ColorType::YELLOW, 0, 2, 0.0f, 0.0f, true, 0});
        
        edenDNA.neurons.push_back({0, NeuronRole::MOTOR, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        edenDNA.neurons.push_back({1, NeuronRole::SENSORY, NeuronPolarity::EXCITATORY, 1.0f, 0.1f, 0.0f});
        edenDNA.synapses.push_back({1, 0, 1.5f}); // Connect sensor to motor

        for (int i = 0; i < 20; ++i) {
            float x = (rand() % (int)config.worldWidth);
            float y = (rand() % (int)config.worldHeight);
            population.push_back(std::make_unique<Organism>(nextOrgId++, edenDNA, config.startingEnergy, x, y));
        }
    }

    void updateTick() {
        float dt = config.physicsDt * config.timeScale;
        
        // 1. Biological Updates (SNNs & Muscle actuation)
        for (auto& org : population) {
            // Provide fake sensory data (e.g., sine wave oscillator just to make it twitch)
            std::vector<float> sensors = { std::sin(nextOrgId * 0.1f) * 2.0f }; 
            org->brain.setSensoryInputs(sensors);
            
            org->updateBiology(dt);
        }

        // 2. Pure Physics Updates
        for (auto& org : population) {
            engine.step(org->points, org->springs, dt);
        }

        // 3. Environment & Collisions (Simplified for brevity)
        // ... (Spatial hashing and collision response would go here) ...
        
        // 4. Cleanup
        population.erase(std::remove_if(population.begin(), population.end(), [](const std::unique_ptr<Organism>& org) {
            return org->markedForDeletion || (!org->isAlive && org->energy < -50.0f);
        }), population.end());
    }
};