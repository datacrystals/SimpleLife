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