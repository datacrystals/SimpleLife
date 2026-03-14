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
    int ioNeuronIndex;    // Direct index into the SNN arrays for fast runtime access
    float sensorRange;
    float currentTension; // For visualizing muscle contraction
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
        // 1. Build Physical Body
        points.push_back({startX, startY, startX, startY, 0, 0, 1.0f}); // Root node
        
        std::vector<int> geneToPointMap;
        geneToPointMap.push_back(0); // Map Gene 0 to Node 0

        for (size_t i = 0; i < dna.morphology.size(); ++i) {
            const auto& mGene = dna.morphology[i];
            
            int p1_idx = (mGene.p1_geneIndex >= 0 && mGene.p1_geneIndex < geneToPointMap.size()) 
                         ? geneToPointMap[mGene.p1_geneIndex] : 0;

            int p2_idx = -1;

            if (mGene.p2_geneIndex >= 0 && mGene.p2_geneIndex < geneToPointMap.size()) {
                // Connect to an existing node
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

            // Map the SNN IO ID to a local index we can use quickly at runtime
            int localIoIdx = -1;
            if (mGene.isMuscle || mGene.type == ColorType::RED) {
                for (size_t m = 0; m < dna.neurons.size(); ++m) {
                    if (dna.neurons[m].role == NeuronRole::MOTOR && dna.neurons[m].id == mGene.ioNeuronId) {
                        localIoIdx = m; 
                        break;
                    }
                }
            } else if (mGene.sensorRange > 0.0f) {
                for (size_t m = 0; m < dna.neurons.size(); ++m) {
                    if (dna.neurons[m].role == NeuronRole::SENSORY && dna.neurons[m].id == mGene.ioNeuronId) {
                        localIoIdx = m; 
                        break;
                    }
                }
            }

            // Calculate actual resting length based on geometry
            float dx = points[p2_idx].x - points[p1_idx].x;
            float dy = points[p2_idx].y - points[p1_idx].y;
            float actualLength = std::sqrt(dx*dx + dy*dy);

            springs.push_back({p1_idx, p2_idx, actualLength, mGene.isMuscle ? 0.3f : 1.0f});
            bodyParts.push_back({mGene.type, mGene.isMuscle, actualLength, localIoIdx, mGene.sensorRange, 0.0f});
        }

        // 2. Build SNN Brain
        for (size_t i = 0; i < dna.neurons.size(); ++i) {
            const auto& nGene = dna.neurons[i];
            
            LIFNeuron n;
            n.id = nGene.id;
            n.role = nGene.role;
            n.polarity = nGene.polarity;
            n.threshold = nGene.threshold;
            n.leakRate = nGene.leakRate;
            n.restPotential = nGene.restPotential;
            n.x = nGene.x;
            n.y = nGene.y;
            
            brain.neurons.push_back(n);

            if (nGene.role == NeuronRole::SENSORY) brain.sensoryIndices.push_back(i);
            if (nGene.role == NeuronRole::MOTOR) brain.motorIndices.push_back(i);
        }

        for (const auto& sGene : dna.synapses) {
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
     * @brief High-efficiency biological update.
     * @param localShade 0.0 (full sun) to 1.0 (total darkness/crowded).
     * @param combatEnergy Energy gained from Red segments hitting others.
     */
    void updateBiology(float dt, const SimConfig& cfg, float localShade, float combatEnergy) {
        if (!isAlive) return;
        
        age += dt;
        if (reproCooldown > 0) reproCooldown -= dt;

        // 1. SNN TICK & COST
        // Every part of the nervous system has a metabolic rent.
        brain.tick(dt);

        float netEnergy = combatEnergy; // Energy gained from predation
        float totalSegments = static_cast<float>(bodyParts.size());

        // 2. SEGMENT LOOP
        for (size_t i = 0; i < springs.size(); ++i) {
            // Photosynthesis with "Anti-Giant" Scaling:
            // Energy = (Rate * Sunlight) / (TotalSegments^2)
            // This makes a 1-segment plant 100x more efficient than a 10-segment plant.
            if (bodyParts[i].type == ColorType::GREEN) {
                float sunlight = std::max(0.0f, 1.0f - localShade);
                float plantEfficiency = 1.0f / (1.0f + (totalSegments * 0.5f)); 
                netEnergy += (cfg.photosynthesisRate * sunlight * plantEfficiency) * bodyParts[i].baseLength * dt;
            }

            // Muscle Actuation
            if (bodyParts[i].isMuscle && bodyParts[i].ioNeuronIndex != -1) {
                bool spiked = brain.neurons[bodyParts[i].ioNeuronIndex].spikedThisTick;
                if (spiked) {
                    springs[i].target_length = bodyParts[i].baseLength * 0.2f; 
                    netEnergy -= cfg.movementCost * dt; // Cost to flex
                    bodyParts[i].currentTension = 1.0f;
                } else {
                    springs[i].target_length += (bodyParts[i].baseLength - springs[i].target_length) * 5.0f * dt;
                    bodyParts[i].currentTension *= 0.99f;
                }
            }
        }
        
        // 3. THE "TAXMAN" (METABOLISM)
        // Base metabolic rate scales slightly with size, but brain cost is flat per neuron.
        float baseMetabolism = (totalSegments * cfg.segmentCost);
        netEnergy -= (baseMetabolism) * dt; 
        
        // Age penalty: Ensures turnover so evolution can happen.
        netEnergy -= (age * 0.02f) * dt;
        
        energy += netEnergy;
        if (energy <= cfg.deathEnergyThreshold) isAlive = false;
    }

};