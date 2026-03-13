/**
 * @file Brain.cpp
 * @brief Implementation of the LIF SNN dynamics.
 */
#include "Brain.h"
#include <algorithm>

void SNNBrain::tick(float dt) {
    // 1. Gather all incoming synaptic currents based on last tick's spikes
    std::vector<float> incomingCurrents(neurons.size(), 0.0f);
    
    for (const auto& syn : synapses) {
        if (neurons[syn.source_idx].spikedThisTick) {
            float current = syn.weight;
            // Dale's principle: Source neuron polarity dictates the sign of the current
            if (neurons[syn.source_idx].polarity == NeuronPolarity::INHIBITORY) {
                current = -current; 
            }
            incomingCurrents[syn.target_idx] += current;
        }
    }

    // 2. Integrate currents, apply leaks, and evaluate spikes
    for (size_t i = 0; i < neurons.size(); ++i) {
        LIFNeuron& n = neurons[i];
        n.spikedThisTick = false; // Reset spike state for this new tick

        if (n.refractoryTimer > 0.0f) {
            n.refractoryTimer -= dt;
            continue; // Neuron is resting, ignore inputs
        }

        // Apply Leak (Decay towards rest potential)
        float leakAmount = (n.restPotential - n.membranePotential) * n.leakRate * dt;
        n.membranePotential += leakAmount;

        // Apply synaptic current and external sensory stimulus
        n.membranePotential += incomingCurrents[i];
        n.membranePotential += n.externalStimulus * dt; 
        
        // Clear stimulus after consumption
        n.externalStimulus = 0.0f;

        // Check for Action Potential (Spike)
        if (n.membranePotential >= n.threshold) {
            n.spikedThisTick = true;
            n.membranePotential = n.restPotential; // Reset
            n.refractoryTimer = 0.01f;             // Hardcoded absolute refractory period
        }
    }
}

void SNNBrain::setSensoryInputs(const std::vector<float>& inputs) {
    // Inject current into sensory neurons based on environmental data (vision, joint angles, etc.)
    int limit = std::min(inputs.size(), sensoryIndices.size());
    for (int i = 0; i < limit; ++i) {
        neurons[sensoryIndices[i]].externalStimulus = inputs[i];
    }
}

std::vector<bool> SNNBrain::getMotorSpikes() const {
    std::vector<bool> motorOut(motorIndices.size());
    for (size_t i = 0; i < motorIndices.size(); ++i) {
        motorOut[i] = neurons[motorIndices[i]].spikedThisTick;
    }
    return motorOut;
}