/**
 * @file Organism.h
 * @brief Represents a single entity. Ties physics, biology, and the SNN together.
 */
#pragma once
#include "PhysicsTypes.h"
#include "Genetics.h"
#include "Brain.h"
#include <vector>
#include <mutex>
#include <cmath>

struct BodyPart {
    ColorType type;
    bool isMuscle;
    float baseLength;
    int motorNeuronIndex; // Direct index into the motor spikes array for fast runtime access
};

class Organism {
public:
    int id;
    Genome dna;
    float energy;
    float age = 0.0f;
    bool isAlive = true;
    bool markedForDeletion = false;
    float reproCooldown = 5.0f;
    
    std::mutex orgMutex;

    // --- The Physical Body ---
    std::vector<PhysicsPoint> points;
    std::vector<PhysicsSpring> springs;
    std::vector<BodyPart> bodyParts; // Maps 1:1 with springs
    
    // --- The Connectome ---
    SNNBrain brain;

    Organism(int _id, const Genome& _dna, float _energy, float startX, float startY) 
        : id(_id), dna(_dna), energy(_energy) 
    {
        buildPhenotype(startX, startY);
    }

    /**
     * @brief Translates the DNA into physical points, constraints, and an SNN.
     */
    void buildPhenotype(float startX, float startY) {
        points.push_back({startX, startY, startX, startY, 0, 0, 1.0f}); // Node 0
        std::vector<int> geneToPointMap;
        geneToPointMap.push_back(0); // Map Gene 0 to Node 0
    
        for (size_t i = 0; i < dna.morphology.size(); ++i) {
            const auto& mGene = dna.morphology[i];
            
            int p1_idx = (mGene.p1_geneIndex >= 0 && mGene.p1_geneIndex < geneToPointMap.size()) 
                         ? geneToPointMap[mGene.p1_geneIndex] : 0;
    
            int p2_idx = -1;
    
            if (mGene.p2_geneIndex >= 0 && mGene.p2_geneIndex < geneToPointMap.size()) {
                // Connect to an existing node (Creates a loop, truss, or muscle across a joint)
                p2_idx = geneToPointMap[mGene.p2_geneIndex];
            } else {
                // Create a brand new node extending out
                float angle = mGene.branchAngle * (3.14159f / 180.0f);
                float nx = points[p1_idx].x + std::cos(angle) * mGene.length;
                float ny = points[p1_idx].y + std::sin(angle) * mGene.length;
                points.push_back({nx, ny, nx, ny, 0, 0, 1.0f});
                p2_idx = points.size() - 1;
            }
    
            geneToPointMap.push_back(p2_idx);
    
            // Map SNN Motor ID
            int localMotorIdx = -1;
            if (mGene.isMuscle) {
                for (size_t m = 0; m < dna.neurons.size(); ++m) {
                    if (dna.neurons[m].role == NeuronRole::MOTOR && dna.neurons[m].id == mGene.motorNeuronId) {
                        localMotorIdx = m; break;
                    }
                }
            }
    
            // Calculate actual resting length based on geometry
            float dx = points[p2_idx].x - points[p1_idx].x;
            float dy = points[p2_idx].y - points[p1_idx].y;
            float actualLength = std::sqrt(dx*dx + dy*dy);
    
            springs.push_back({p1_idx, p2_idx, actualLength, mGene.isMuscle ? 0.3f : 1.0f});
            bodyParts.push_back({mGene.type, mGene.isMuscle, actualLength, localMotorIdx});
        }

        // 2. Build SNN Brain
        for (size_t i = 0; i < dna.neurons.size(); ++i) {
            const auto& nGene = dna.neurons[i];
            brain.neurons.push_back({nGene.id, nGene.role, nGene.polarity, nGene.threshold, nGene.leakRate, nGene.restPotential});
            if (nGene.role == NeuronRole::SENSORY) brain.sensoryIndices.push_back(i);
            if (nGene.role == NeuronRole::MOTOR) brain.motorIndices.push_back(i);
        }

        for (const auto& sGene : dna.synapses) {
            // Find local vector indices from genetic IDs
            int srcIdx = -1, tgtIdx = -1;
            for (size_t i = 0; i < brain.neurons.size(); ++i) {
                if (brain.neurons[i].id == sGene.sourceId) srcIdx = i;
                if (brain.neurons[i].id == sGene.targetId) tgtIdx = i;
            }
            if (srcIdx != -1 && tgtIdx != -1) {
                brain.synapses.push_back({srcIdx, tgtIdx, sGene.weight});
            }
        }
    }

    /**
     * @brief Reads motor spikes and actuates muscles via physics target_lengths.
     */
    void updateBiology(float dt) {
        if (!isAlive) return;
        
        age += dt;
        if (reproCooldown > 0) reproCooldown -= dt;

        // Run the SNN
        brain.tick(dt);
        auto motorSpikes = brain.getMotorSpikes();

        // Translate Spikes to Muscle Contractions
        for (size_t i = 0; i < springs.size(); ++i) {
            if (bodyParts[i].isMuscle && bodyParts[i].motorNeuronIndex != -1) {
                // Determine which motor spike controls this muscle
                bool spiked = false;
                if (bodyParts[i].motorNeuronIndex < motorSpikes.size()) {
                    spiked = motorSpikes[bodyParts[i].motorNeuronIndex];
                }

                if (spiked) {
                    // Contract muscle (shrink target length)
                    springs[i].target_length = bodyParts[i].baseLength * 0.5f; 
                    energy -= 0.05f * dt; // Cost of flexing
                } else {
                    // Relax back to base length gradually
                    springs[i].target_length += (bodyParts[i].baseLength - springs[i].target_length) * 5.0f * dt;
                }
            }
        }
        
        // Passive energy drain
        energy -= bodyParts.size() * 0.01f * dt; 
        if (energy <= 0.0f) isAlive = false;
    }
};