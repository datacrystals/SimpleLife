/**
 * @file Genetics.h
 * @brief Defines the genetic blueprint for morphology and SNN connectomes.
 */
#pragma once
#include <vector>

#include <cstdlib>
#include <cmath>

#include "SimConfig.h" // Ensure you include this
#include "Brain/Brain.h" // For NeuronRole and NeuronPolarity

enum class ColorType { GREEN, RED, PURPLE, BLUE, YELLOW, WHITE, DEAD };

struct MorphologyGene {
    ColorType type;
    int p1_geneIndex;
    int p2_geneIndex; // -1 if creating a new node
    float length;
    float branchAngle; // Used if p2_geneIndex is -1
    
    bool isMuscle;
    int ioNeuronId;    // Motor ID for muscles/damage, Sensory ID for eyes/antennae
    float sensorRange; // For vision/raycasts
};

struct NeuronGene {
    int id;              // Unique ID to track topology across generations
    NeuronRole role;
    NeuronPolarity polarity;
    float threshold;
    float leakRate;
    float restPotential;

    float x, y;
};

struct SynapseGene {
    int sourceId;
    int targetId;
    float weight;
};

struct Genome {
    std::vector<MorphologyGene> morphology;
    std::vector<NeuronGene> neurons;
    std::vector<SynapseGene> synapses;
    
    float lifespan = 40.0f;
    int symmetry = 1; 


    // Inside the Genome struct in Genetics.h
    void initializeBaseBrain(int numNeurons = 100) {
        // 1. INHERITANCE CHECK
        // If the genome already has neurons (e.g., from a parent), keep the existing map and exit!
        if (!neurons.empty()) return; 
        
        int sensoryCount = numNeurons * 0.2f; // 20% Sensory
        int motorCount = numNeurons * 0.2f;   // 20% Motor
        
        int sIdx = 0; 
        int mIdx = 0;

        // 2. CREATE STRUCTURED NEURON MAP
        for (int i = 0; i < numNeurons; ++i) {
            NeuronGene n;
            n.id = i;
            
            // Assign roles and strict spatial coordinates
            if (i < sensoryCount) {
                n.role = NeuronRole::SENSORY;
                n.x = 0.05f; // Hard left
                n.y = static_cast<float>(sIdx++) / std::max(1, sensoryCount - 1);
            } else if (i < sensoryCount + motorCount) {
                n.role = NeuronRole::MOTOR;
                n.x = 0.95f; // Hard right
                n.y = static_cast<float>(mIdx++) / std::max(1, motorCount - 1);
            } else {
                n.role = NeuronRole::HIDDEN;
                n.x = 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.6f; // "Goo" in the middle
                n.y = static_cast<float>(rand()) / RAND_MAX; // Full vertical spread
            }

            // Dale's Principle: Mix of Excitatory/Inhibitory
            n.polarity = (rand() % 10 < 8) ? NeuronPolarity::EXCITATORY : NeuronPolarity::INHIBITORY;
            n.threshold = 1.0f;
            n.leakRate = 0.1f;
            n.restPotential = 0.0f;
            
            neurons.push_back(n);
        }

        // 3. BOOTSTRAP SYNAPSES (Proximity-based, respecting constraints)
        for (size_t i = 0; i < neurons.size(); ++i) {
            for (size_t j = 0; j < neurons.size(); ++j) {
                if (i == j) continue; // No self-connections initially
                
                NeuronRole roleA = neurons[i].role;
                NeuronRole roleB = neurons[j].role;

                // THE GOLDEN RULES: 
                // 1. No direct Sensory -> Motor
                // 2. No Sensory -> Sensory
                // 3. No Motor -> Motor
                bool isIllegal = false;
                if (roleA == NeuronRole::SENSORY && roleB == NeuronRole::MOTOR) isIllegal = true;
                if (roleA == NeuronRole::SENSORY && roleB == NeuronRole::SENSORY) isIllegal = true;
                if (roleA == NeuronRole::MOTOR && roleB == NeuronRole::MOTOR) isIllegal = true;
                
                // Optional: You might also want to block Motor -> Sensory (backward flow)
                // if (roleA == NeuronRole::MOTOR && roleB == NeuronRole::SENSORY) isIllegal = true;

                if (isIllegal) continue; 

                float dx = neurons[i].x - neurons[j].x;
                float dy = neurons[i].y - neurons[j].y;
                float dist = std::sqrt(dx*dx + dy*dy);

                // Connect if close enough to form local processing clusters.
                if (dist < 0.35f && (static_cast<float>(rand()) / RAND_MAX) < 0.3f) {
                    float weight = (static_cast<float>(rand()) / RAND_MAX) * 2.5f; 
                    synapses.push_back({neurons[i].id, neurons[j].id, weight});
                }
            }
        }
    }

    // Update your existing mutate() function:
    void mutate(const SimConfig& cfg) {
        auto rnd = []() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };

        if (rnd() > cfg.globalMutationRate) return;

        // --- MORPHOLOGY MUTATIONS ---
        for (auto& m : morphology) {
            if (rnd() < cfg.mutChanceType) m.type = static_cast<ColorType>(rand() % 7); 
            if (rnd() < cfg.mutChanceMotor) m.isMuscle = !m.isMuscle;
            if (rnd() < 0.2f) m.length += (rnd() - 0.5f) * 2.0f;
            if (rnd() < 0.2f) m.branchAngle += (rnd() - 0.5f) * 15.0f;
            if (m.length < 1.0f) m.length = 1.0f; 
        }

        // Add a entirely new segment (WITH AUTO-WIRING)
        if (rnd() < cfg.mutChanceAddNode && !morphology.empty()) {
            MorphologyGene newGene;
            newGene.type = static_cast<ColorType>(rand() % 7);
            newGene.p1_geneIndex = rand() % morphology.size();
            newGene.p2_geneIndex = -1; 
            newGene.length = 3.0f + rnd() * 4.0f;
            newGene.branchAngle = rnd() * 360.0f;
            newGene.isMuscle = (rnd() < 0.3f);
            newGene.sensorRange = (rnd() < 0.1f) ? 30.0f : 0.0f;
            
            // --> CHANGE: Auto-wire to the brain
            newGene.ioNeuronId = -1;
            if (newGene.isMuscle || newGene.type == ColorType::RED) {
                std::vector<int> motorIds;
                for (const auto& n : neurons) if (n.role == NeuronRole::MOTOR) motorIds.push_back(n.id);
                if (!motorIds.empty()) newGene.ioNeuronId = motorIds[rand() % motorIds.size()];
            } else if (newGene.sensorRange > 0.0f) {
                std::vector<int> sensoryIds;
                for (const auto& n : neurons) if (n.role == NeuronRole::SENSORY) sensoryIds.push_back(n.id);
                if (!sensoryIds.empty()) newGene.ioNeuronId = sensoryIds[rand() % sensoryIds.size()];
            }
            
            morphology.push_back(newGene);
        }

        // --- SNN MUTATIONS ---
        // Add a new neuron
        if (rnd() < cfg.mutChanceAddNeuron) {
            NeuronGene n;
            n.id = neurons.empty() ? 0 : neurons.back().id + 1; 
            n.role = static_cast<NeuronRole>(rand() % 3);
            n.polarity = (rnd() < 0.5f) ? NeuronPolarity::EXCITATORY : NeuronPolarity::INHIBITORY;
            n.threshold = 0.5f + rnd() * 1.0f;
            n.leakRate = 0.01f + rnd() * 0.05f;
            n.restPotential = 0.0f;

            // Calculate the Bounding Box of existing neurons
            float minX = 0.0f, maxX = 1.0f, minY = 0.0f, maxY = 1.0f;
            if (!neurons.empty()) {
                minX = neurons[0].x; maxX = neurons[0].x;
                minY = neurons[0].y; maxY = neurons[0].y;
                for (const auto& existingN : neurons) {
                    if (existingN.x < minX) minX = existingN.x;
                    if (existingN.x > maxX) maxX = existingN.x;
                    if (existingN.y < minY) minY = existingN.y;
                    if (existingN.y > maxY) maxY = existingN.y;
                }
            }

            // Spawn strictly within the bounds
            n.x = minX + (rnd() * (maxX - minX));
            n.y = minY + (rnd() * (maxY - minY));

            neurons.push_back(n);
        }

        
        // Add a new synapse (PREVENTING ILLEGAL CONNECTIONS)
        if (rnd() < cfg.mutChanceAddSynapse && neurons.size() >= 2) {
            int idxA = rand() % neurons.size();
            int idxB = (rnd() < 0.5f) ? (idxA + 1) % neurons.size() : rand() % neurons.size();
            
            if (idxA != idxB) {
                NeuronRole roleA = neurons[idxA].role;
                NeuronRole roleB = neurons[idxB].role;
                
                bool isIllegalConnection = false;
                if (roleA == NeuronRole::SENSORY && roleB == NeuronRole::MOTOR) isIllegalConnection = true;
                if (roleA == NeuronRole::SENSORY && roleB == NeuronRole::SENSORY) isIllegalConnection = true;
                if (roleA == NeuronRole::MOTOR && roleB == NeuronRole::MOTOR) isIllegalConnection = true;
                
                if (!isIllegalConnection) {
                    SynapseGene s;
                    s.sourceId = neurons[idxA].id;
                    s.targetId = neurons[idxB].id;
                    s.weight = (rnd() - 0.5f) * 2.0f;
                    synapses.push_back(s);
                }
            }
        }

        for (auto& s : synapses) {
            if (rnd() < cfg.mutChanceChangeWeight) {
                s.weight += (rnd() - 0.5f) * 0.5f; 
            }
        }
    }


    

};