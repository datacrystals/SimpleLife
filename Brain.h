/**
 * @file Brain.h
 * @brief Unstructured Leaky Integrate-and-Fire (LIF) Spiking Neural Network.
 * Supports arbitrary topologies, recurrent connections, and Dale's principle 
 * (strict separation of excitatory and inhibitory neurons).
 */
#pragma once
#include <vector>
#include <cstdint>

enum class NeuronRole { SENSORY, HIDDEN, MOTOR };
enum class NeuronPolarity { EXCITATORY, INHIBITORY }; // Excitatory adds to potential, Inhibitory subtracts

struct LIFNeuron {
    int id; // Unique ID for genetic tracking
    NeuronRole role;
    NeuronPolarity polarity;

    float x, y;

    // LIF Dynamics
    float membranePotential = 0.0f;
    float threshold = 1.0f;
    float leakRate = 0.1f;
    float restPotential = 0.0f;
    
    // State
    bool spikedThisTick = false;
    float refractoryTimer = 0.0f;
    float externalStimulus = 0.0f; // Injected current from sensors
};

struct Synapse {
    int source_idx; // Index in the neurons vector
    int target_idx; 
    float weight;   // Strength of the connection (Absolute value. Polarity determines sign during integration)
};

class SNNBrain {
public:
    std::vector<LIFNeuron> neurons;
    std::vector<Synapse> synapses;
    
    // Pointers/indices to quickly access IO nodes without searching the graph every tick
    std::vector<int> sensoryIndices;
    std::vector<int> motorIndices;

    /**
     * @brief Steps the neural network forward in time.
     * Evaluates leaks, integrates synaptic currents, and fires spikes.
     */
    void tick(float dt);

    /**
     * @brief Injects raw float data into sensory neurons.
     */
    void setSensoryInputs(const std::vector<float>& inputs);

    /**
     * @brief Reads the spike states of motor neurons to actuate physical muscles.
     */
    std::vector<bool> getMotorSpikes() const;
};