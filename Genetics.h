/**
 * @file Genetics.h
 * @brief Defines the genetic blueprint for morphology and SNN connectomes.
 */
#pragma once
#include <vector>
#include "Brain.h" // For NeuronRole and NeuronPolarity

enum class ColorType { GREEN, RED, PURPLE, BLUE, YELLOW, WHITE, DEAD };

struct MorphologyGene {
    ColorType type;
    int p1_geneIndex; // Which previous node to attach to
    int p2_geneIndex; // If -1, creates a NEW node at 'length' and 'angle'. 
                      // If >= 0, connects to an EXISTING node (forming trusses/muscles).
    float length;
    float branchAngle; 
    
    bool isMuscle;
    int motorNeuronId;
};

struct NeuronGene {
    int id;              // Unique ID to track topology across generations
    NeuronRole role;
    NeuronPolarity polarity;
    float threshold;
    float leakRate;
    float restPotential;
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
};