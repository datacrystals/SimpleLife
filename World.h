#pragma once
#include "Types.h"
#include "JoltWrapper.h"
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <execution>
#include <random>
#include <vector>
#include <mutex>
#include <cmath> // NEW: For the multithreaded rotation math

extern std::mt19937 rng;
extern std::uniform_real_distribution<float> randFloat;

class World {
public:
    JoltWrapper jolt;
    std::vector<OrganismRecord*> population;
    
    struct SpawnRequest { Genome dna; float x; float y; float energy; };
    std::vector<SpawnRequest> spawnRequests;
    std::mutex spawnMutex;
    
    float worldTime = 0.0f;
    int nextOrgId = 1;
    
    float timeScale = 4.0f;          
    int maxPopulation = 1000;       
    float maxLifespan = 90.0f;
    
    float mutationRate = 0.20f;      
    float baseMetabolism = 0.1f;
    float photosynthesisRate = 3.5f; 
    float movementCost = 0.001f;
    float thrustMultiplier = 50.0f;
    float turnMultiplier = 20.0f;

    World() { initEden(); }

    Genome mutateGenome(Genome parentDna) {
        Genome child = parentDna;
        for (auto& gene : child) {
            if (randFloat(rng) < mutationRate) { 
                if (randFloat(rng) < 0.2f) {
                    gene.type = static_cast<ColorType>(rand() % 6);
                }
                
                gene.length += (randFloat(rng) - 0.5f) * 0.5f; 
                gene.param1 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.param2 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.weight_FoodSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.weight_HazardSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.bias += (randFloat(rng) - 0.5f) * 1.0f;
                if (gene.length < 0.2f) gene.length = 0.2f; 
            }
        }
        
        if (randFloat(rng) < (mutationRate / 2.0f) && child.size() < 10) {
            ColorType newType = static_cast<ColorType>(rand() % 6);
            int attachIdx = rand() % child.size(); 
            child.push_back({newType, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, attachIdx, 45.0f});
            
            if (randFloat(rng) > 0.5f && child.size() < 10) {
                child.push_back({newType, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, attachIdx, -45.0f});
            }
        }
        return child;
    }

    void queueOrganism(Genome dna, float x, float y, float startingEnergy) {
        OrganismRecord* org = new OrganismRecord(nextOrgId++, dna, startingEnergy);
        
        for (size_t i = 0; i < dna.size(); i++) {
            Gene& g = dna[i];
            Segment* seg = new Segment{g.type, 0, 1.0f, g.length, org};
            
            float spawnX = x + (randFloat(rng) - 0.5f) * 0.1f;
            float spawnY = y + (randFloat(rng) - 0.5f) * 0.1f;
            
            seg->joltBodyID = jolt.createSegment(spawnX, spawnY, 1.0f, g.length, g.type, seg, org->id);
            org->segments.push_back(seg);

            if (i > 0) {
                int pIdx = (g.parentIndex >= 0 && g.parentIndex < (int)i) ? g.parentIndex : i - 1;
                jolt.createMuscle(org->id, org->segments[pIdx]->joltBodyID, seg->joltBodyID, spawnX, spawnY);
            }
        }
        population.push_back(org); 
    }

    void initEden() {
        for (int i=0; i<10; i++) {
            Genome dna = { {ColorType::GREEN, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1, 0.0f} };
            queueOrganism(dna, (randFloat(rng)-0.5f)*60.0f, (randFloat(rng)-0.5f)*60.0f, 390.0f);
        }
    }

    void updateTick() {
        float dt = (1.0f / 60.0f) * timeScale;
        if (dt > 0.0f) jolt.stepPhysics(dt);
        worldTime += dt;

        std::for_each(std::execution::par, population.begin(), population.end(), [&](OrganismRecord* org) {
            if (org->markedForDeletion) return; 

            org->age += dt;
            if (org->isAlive && org->age > maxLifespan) org->energy = 0;
            if (!org->isAlive) {
                org->energy -= 10.0f * dt; 
                if (org->energy < -50.0f) org->markedForDeletion = true; 
                return;
            }

            float netEnergy = 0.0f;
            org->sensorFoodDistance = 1.0f; 
            org->sensorHazardDistance = 1.0f; 

            if (org->segments[0]->type == ColorType::WHITE || org->segments[0]->type == ColorType::RED) {
                JPH::BodyID headID(org->segments[0]->joltBodyID);
                JPH::RVec3 pos = jolt.physicsSystem->GetBodyInterface().GetPosition(headID);
                JPH::Quat rot = jolt.physicsSystem->GetBodyInterface().GetRotation(headID);
                JPH::Vec3 forward = rot * JPH::Vec3(0, 1, 0); 

                JPH::RRayCast ray { pos + forward * 1.5f, forward * 25.0f }; 
                JPH::RayCastSettings settings;
                JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;

                jolt.physicsSystem->GetNarrowPhaseQuery().CastRay(ray, settings, collector);

                if (collector.HadHit()) {
                    JPH::BodyLockRead lock(jolt.physicsSystem->GetBodyLockInterface(), collector.mHit.mBodyID);
                    if (lock.Succeeded()) {
                        Segment* hitSeg = reinterpret_cast<Segment*>(lock.GetBody().GetUserData());
                        if (hitSeg && hitSeg->parentOrg != org) { 
                            if (hitSeg->type == ColorType::GREEN) org->sensorFoodDistance = collector.mHit.mFraction;
                            if (hitSeg->type == ColorType::RED) org->sensorHazardDistance = collector.mHit.mFraction;
                        }
                    }
                }
            }

            for (size_t i = 0; i < org->segments.size(); i++) {
                // --- MULTITHREADED RENDERING MATH ---
                // We calculate exactly where the corners of the quad are using the 64 cores.
                JPH::BodyID sID(org->segments[i]->joltBodyID);
                JPH::RVec3 pos = jolt.physicsSystem->GetBodyInterface().GetPosition(sID);
                JPH::Quat rot = jolt.physicsSystem->GetBodyInterface().GetRotation(sID);
                
                float cx = pos.GetX();
                float cy = pos.GetY();
                float angle = rot.GetEulerAngles().GetZ();
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);
                float hw = org->segments[i]->width / 2.0f;
                float hh = org->segments[i]->height / 2.0f;

                org->segments[i]->vX[0] = cx + (-hw * cosA - -hh * sinA);
                org->segments[i]->vY[0] = cy + (-hw * sinA + -hh * cosA);
                
                org->segments[i]->vX[1] = cx + (hw * cosA - -hh * sinA);
                org->segments[i]->vY[1] = cy + (hw * sinA + -hh * cosA);
                
                org->segments[i]->vX[2] = cx + (hw * cosA - hh * sinA);
                org->segments[i]->vY[2] = cy + (hw * sinA + hh * cosA);
                
                org->segments[i]->vX[3] = cx + (-hw * cosA - hh * sinA);
                org->segments[i]->vY[3] = cy + (-hw * sinA + hh * cosA);
                // ------------------------------------

                float surfaceArea = org->segments[i]->width * org->segments[i]->height;

                if (org->segments[i]->type == ColorType::GREEN) {
                    netEnergy += photosynthesisRate * surfaceArea * timeScale;
                } else {
                    float segMetabolism = baseMetabolism * surfaceArea * timeScale;
                    if (org->segments[i]->type == ColorType::PURPLE) segMetabolism *= 0.2f; 
                    netEnergy -= segMetabolism; 
                    
                    float neuralSignal = ((1.0f - org->sensorFoodDistance) * org->dna[i].weight_FoodSensor) +
                                         ((1.0f - org->sensorHazardDistance) * org->dna[i].weight_HazardSensor) +
                                         org->dna[i].bias;

                    if (neuralSignal > 0.0f) {
                        if (org->segments[i]->type == ColorType::YELLOW) {
                            float thrust = org->dna[i].param1 * neuralSignal * thrustMultiplier; 
                            jolt.applyThrust(org->segments[i]->joltBodyID, thrust);
                            netEnergy -= std::abs(thrust) * movementCost * timeScale;
                        } else if (org->segments[i]->type == ColorType::BLUE) {
                            float torque = org->dna[i].param2 * neuralSignal * turnMultiplier;
                            jolt.applyTorque(org->segments[i]->joltBodyID, torque);
                            netEnergy -= std::abs(torque) * movementCost * timeScale;
                        }
                    }
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(org->orgMutex);
                org->energy += netEnergy;
                if (org->energy > 800.0f) org->energy = 800.0f; 

                if (org->energy <= 0) {
                    org->isAlive = false;
                    for (auto* seg : org->segments) seg->type = ColorType::DEAD;
                }
            }

            if (org->energy >= 400.0f && population.size() < maxPopulation) {
                org->energy -= 200.0f;
                JPH::RVec3 pos = jolt.physicsSystem->GetBodyInterface().GetPosition(JPH::BodyID(org->segments[0]->joltBodyID));
                
                std::lock_guard<std::mutex> lock(spawnMutex);
                float spawnX = pos.GetX() + (randFloat(rng) - 0.5f) * 10.0f;
                float spawnY = pos.GetY() + (randFloat(rng) - 0.5f) * 10.0f;
                spawnRequests.push_back({mutateGenome(org->dna), spawnX, spawnY, 200.0f}); 
            }
        }); 

        for (auto& req : spawnRequests) queueOrganism(req.dna, req.x, req.y, req.energy);
        spawnRequests.clear();

        std::vector<JPH::BodyID> bodiesToRemove;
        population.erase(std::remove_if(population.begin(), population.end(), [&](OrganismRecord* org) {
            if (org->markedForDeletion) {
                for (auto* seg : org->segments) { 
                    bodiesToRemove.push_back(JPH::BodyID(seg->joltBodyID)); 
                }
                jolt.cleanupJoints(org->id); 
                for (auto* seg : org->segments) delete seg;
                delete org; 
                return true;
            }
            return false;
        }), population.end());
        
        if (!bodiesToRemove.empty()) {
            jolt.physicsSystem->GetBodyInterface().RemoveBodies(bodiesToRemove.data(), bodiesToRemove.size());
            jolt.physicsSystem->GetBodyInterface().DestroyBodies(bodiesToRemove.data(), bodiesToRemove.size());
        }
    }
};