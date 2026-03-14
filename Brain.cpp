/**
 * @file Brain.cpp
 * @brief Implementation of the LIF SNN dynamics.
 */
#include "Brain.h"
#include <algorithm>

void SNNBrain::tick(float dt) {
    localTime += dt;
    
    // 1. Process spikes currently in transit
    std::vector<float> incomingCurrents(neurons.size(), 0.0f);
    
    for (auto& syn : synapses) {
        // If the source fired last tick, inject a new spike into the axon
        if (neurons[syn.source_idx].spikedThisTick) {
            syn.spikeDeliveryTimes.push(localTime + syn.delay);
        }

        // Process any spikes that have physically arrived at the dendrite
        while (!syn.spikeDeliveryTimes.empty() && localTime >= syn.spikeDeliveryTimes.front()) {
            syn.spikeDeliveryTimes.pop();
            
            float current = syn.weight;
            // Dale's principle
            if (neurons[syn.source_idx].polarity == NeuronPolarity::INHIBITORY) {
                current = -current; 
            }
            incomingCurrents[syn.target_idx] += current;
        }
    }

    // 2. Integrate currents, apply leaks, and evaluate spikes
    for (size_t i = 0; i < neurons.size(); ++i) {
        LIFNeuron& n = neurons[i];
        n.spikedThisTick = false; 

        if (n.refractoryTimer > 0.0f) {
            n.refractoryTimer -= dt;
            continue; 
        }

        // Apply Leak
        float leakAmount = (n.restPotential - n.membranePotential) * n.leakRate * dt;
        n.membranePotential += leakAmount;

        // Apply arrived synaptic currents and external stimuli
        n.membranePotential += incomingCurrents[i];
        n.membranePotential += n.externalStimulus * dt; 
        
        n.externalStimulus = 0.0f;

        // Action Potential 
        if (n.membranePotential >= n.threshold) {
            n.spikedThisTick = true;
            n.membranePotential = n.restPotential; 
            n.refractoryTimer = 0.01f;             
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