/**
 * @file SimConfig.h
 * @brief Master configuration for the simulation environment.
 * Contains all physical, biological, and evolutionary constants.
 */
#pragma once

struct SimConfig {
    // --- Environment & Spatial ---
    float worldWidth = 200.0f;
    float worldHeight = 140.0f;
    float spatialGridSize = 19.0f;
    float timeScale = 1.0f;
    float physicsDt = 1.0f / 60.0f;
    
    // --- Engine & Physics ---
    int physicsIterations = 4;
    float friction = 0.95f;
    float maxVelocity = 25.0f;
    float collisionPushRadius = 1.2f;
    float collisionForce = 150.0f;

    // --- Population Limits ---
    int maxPopulation = 2000;
    float startingEnergy = 10.0f;
    float reproductionEnergyThreshold = 300.0f;
    float reproductionEnergyCost = 200.0f;
    float reproductionCooldown = 15.0f;

    // --- Metabolism & Energy ---
    float baseMetabolism = 0.1f;
    float segmentCost = 0.02f;
    float sizeDiscount = 0.01f;
    float movementCost = 0.004f;
    float maxEnergy = 800.0f;
    float deathEnergyThreshold = -50.0f;

    // --- Phenotype Rules ---
    float minLifespan = 10.0f;
    float maxLifespan = 200.0f;
    int minSymmetry = 1;
    int maxSymmetry = 8;
    
    // Green (Plant)
    float photosynthesisRate = 1.0f;
    float shadePenalty = 0.5f;
    float greenCrowdRadius = 5.0f;
    
    // Red/White (Combat & Eating)
    float herbivoreEatEnergy = 150.0f;
    float herbivoreAttackRange = 2.0f;
    float carnivoreDamagePerSec = 80.0f;
    float carnivoreEfficiency = 0.8f;
    float carnivoreAttackRange = 2.0f;
    
    // Purple (Shield)
    float shieldEfficiency = 0.7f;
    float shieldCost = 0.05f;

    // --- Evolution & Mutation Rates ---
    float globalMutationRate = 0.05f;
    float mutChanceType = 0.2f;
    float mutChanceMotor = 0.2f;
    float mutChanceAddNode = 0.5f;
    
    // --- SNN Specific Config ---
    float mutChanceAddNeuron = 0.1f;
    float mutChanceAddSynapse = 0.2f;
    float mutChanceChangeWeight = 0.4f;
    float defaultNeuronThreshold = 1.0f;
    float defaultNeuronLeak = 0.1f;
};