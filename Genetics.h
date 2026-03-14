/**
 * @file Genetics.h
 * @brief Defines the genetic blueprint for morphology and SNN connectomes.
 */
#pragma once
#include <vector>

#include <cstdlib>
#include "SimConfig.h" // Ensure you include this
#include "Brain.h" // For NeuronRole and NeuronPolarity

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



    
    // Inside your Genome struct in Genetics.h:
    void mutate(const SimConfig& cfg) {
        auto rnd = []() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); };
    
        // Gatekeeper: Does a mutation happen at all?
        if (rnd() > cfg.globalMutationRate) return;
    
        // --- MORPHOLOGY MUTATIONS ---
        for (auto& m : morphology) {
            // Chance to completely change block type (e.g. Bone to Plant)
            if (rnd() < cfg.mutChanceType) {
                m.type = static_cast<ColorType>(rand() % 7); 
            }
            // Chance to toggle flexibility (Bone <-> Muscle)
            if (rnd() < cfg.mutChanceMotor) {
                m.isMuscle = !m.isMuscle;
            }
            // Micro-mutations: Slight tweaks to length and branch angle
            if (rnd() < 0.2f) m.length += (rnd() - 0.5f) * 2.0f;
            if (rnd() < 0.2f) m.branchAngle += (rnd() - 0.5f) * 15.0f;
            
            // Ensure values stay strictly safe
            if (m.length < 1.0f) m.length = 1.0f; 
        }
    
        // Add a entirely new segment
        if (rnd() < cfg.mutChanceAddNode && !morphology.empty()) {
            MorphologyGene newGene;
            newGene.type = static_cast<ColorType>(rand() % 7);
            newGene.p1_geneIndex = rand() % morphology.size(); // Attach to random existing node
            newGene.p2_geneIndex = -1;                         // Branch outwards
            newGene.length = 3.0f + rnd() * 4.0f;
            newGene.branchAngle = rnd() * 360.0f;
            newGene.isMuscle = (rnd() < 0.3f);
            newGene.ioNeuronId = -1; // Requires SNN mutation to map correctly later
            newGene.sensorRange = (rnd() < 0.1f) ? 30.0f : 0.0f;
            
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
            neurons.push_back(n);
        }
    
        // Add a new synapse
        // Instead of just random synapses, favor "local" clusters or loops
        if (rnd() < cfg.mutChanceAddSynapse && neurons.size() >= 2) {
            SynapseGene s;
            int idxA = rand() % neurons.size();
            // 50% chance to link to a nearby neuron in the vector (creating local functional clusters)
            int idxB = (rnd() < 0.5f) ? (idxA + 1) % neurons.size() : rand() % neurons.size();
            
            s.sourceId = neurons[idxA].id;
            s.targetId = neurons[idxB].id;
            s.weight = (rnd() - 0.5f) * 2.0f;
            synapses.push_back(s);
        }
    
        // Shift existing synaptic weights
        for (auto& s : synapses) {
            if (rnd() < cfg.mutChanceChangeWeight) {
                s.weight += (rnd() - 0.5f) * 0.5f; 
            }
        }
    }

};