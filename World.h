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

extern std::mt19937 rng;
extern std::uniform_real_distribution<float> randFloat;

class World {
public:
    JoltWrapper jolt;
    std::vector<OrganismRecord*> population;
    
    // Thread-safe spawning queue
    struct SpawnRequest { Genome dna; float x; float y; float energy; };
    std::vector<SpawnRequest> spawnRequests;
    std::mutex spawnMutex;
    
    float worldTime = 0.0f;
    int nextOrgId = 1;
    
    // Live Evolution Parameters
    float timeScale = 1.0f;
    int maxPopulation = 2000;
    float maxLifespan = 90.0f;
    
    float mutationRate = 0.15f;
    float baseMetabolism = 0.1f;
    float movementCost = 0.001f;
    float thrustMultiplier = 50.0f;
    float turnMultiplier = 20.0f;

    World() { initEden(); }

    Genome mutateGenome(Genome parentDna) {
        Genome child = parentDna;
        for (auto& gene : child) {
            if (randFloat(rng) < mutationRate) { 
                gene.length += (randFloat(rng) - 0.5f) * 0.5f; 
                gene.param1 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.param2 += (randFloat(rng) - 0.5f) * 5.0f; 
                gene.weight_FoodSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.weight_HazardSensor += (randFloat(rng) - 0.5f) * 2.0f;
                gene.bias += (randFloat(rng) - 0.5f) * 1.0f;
                if (gene.length < 0.2f) gene.length = 0.2f; 
            }
        }
        
        // COMPLEXITY MUTATION: Branching and Symmetry
        if (randFloat(rng) < (mutationRate / 2.0f) && child.size() < 10) {
            ColorType newType = (randFloat(rng) > 0.5f) ? ColorType::BLUE : ColorType::YELLOW;
            if (randFloat(rng) > 0.8f) newType = ColorType::PURPLE;

            // Pick a random existing segment to sprout from
            int attachIdx = rand() % child.size(); 
            
            // Sprout a new limb
            child.push_back({newType, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, attachIdx, 45.0f});
            
            // SYMMETRY: 50% chance to immediately mutate a mirrored copy on the other side!
            if (randFloat(rng) > 0.5f && child.size() < 10) {
                child.push_back({newType, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, attachIdx, -45.0f});
            }
        }
        return child;
    }

    void queueOrganism(Genome dna, float x, float y, float startingEnergy) {
        OrganismRecord* org = new OrganismRecord{nextOrgId++, dna, {}, {}, startingEnergy, 0.0f, true, false, 1.0f, 1.0f};
        
        for (size_t i = 0; i < dna.size(); i++) {
            Gene& g = dna[i];
            Segment* seg = new Segment{g.type, 0, 1.0f, g.length, org};
            
            // Stagger spawn positions slightly to prevent physics explosion
            float spawnX = x + (randFloat(rng) - 0.5f) * 0.1f;
            float spawnY = y + (randFloat(rng) - 0.5f) * 0.1f;
            
            seg->joltBodyID = jolt.createSegment(spawnX, spawnY, 1.0f, g.length, g.type, seg, org->id);
            org->segments.push_back(seg);

            // Connect to parent limb (or fallback to previous if DNA breaks)
            if (i > 0) {
                int pIdx = (g.parentIndex >= 0 && g.parentIndex < (int)i) ? g.parentIndex : i - 1;
                // FIX: Pass org->id in so JoltWrapper can track this joint
                jolt.createMuscle(org->id, org->segments[pIdx]->joltBodyID, seg->joltBodyID, spawnX, spawnY);
            }
        }
        population.push_back(org); // Safe: only called on main thread now
    }

    void initEden() {
        for (int i=0; i<300; i++) {
            Genome dna = { 
                {ColorType::WHITE, 1.0f, 0.0f, 0.0f, 2.0f, -2.0f, 0.0f, -1, 0.0f}, // Head
                {ColorType::YELLOW, 1.0f, 60.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0.0f}, // Tail thrust (attaches to 0)
                {ColorType::BLUE, 1.0f, 0.0f, 20.0f, 0.0f, 0.0f, 0.0f, 1, 0.0f}    // Tail turn (attaches to 1)
            };
            queueOrganism(dna, (randFloat(rng)-0.5f)*80.0f, (randFloat(rng)-0.5f)*80.0f, 400.0f);
        }
    }

    void updateTick() {
        float dt = (1.0f / 60.0f) * timeScale;
        if (dt > 0.0f) jolt.stepPhysics(dt);
        worldTime += dt;

        // --- MULTITHREADED PHYSICS & BRAIN PHASE ---
        std::for_each(std::execution::par, population.begin(), population.end(), [&](OrganismRecord* org) {
            if (org->markedForDeletion || org->segments[0]->type == ColorType::GREEN) return; 

            // Aging and Death
            org->age += dt;
            if (org->isAlive && org->age > maxLifespan) org->energy = 0;
            if (!org->isAlive) {
                org->energy -= 10.0f * dt; // Decay
                if (org->energy < -50.0f) org->markedForDeletion = true; 
                return;
            }

            float netEnergy = 0.0f;
            
            // Sensor Reset & Raycasting (Head Only)
            org->sensorFoodDistance = 1.0f; 
            org->sensorHazardDistance = 1.0f; 

            if (org->segments[0]->type == ColorType::WHITE) {
                JPH::BodyID headID(org->segments[0]->joltBodyID);
                JPH::RVec3 pos = jolt.physicsSystem->GetBodyInterface().GetPosition(headID);
                JPH::Quat rot = jolt.physicsSystem->GetBodyInterface().GetRotation(headID);
                JPH::Vec3 forward = rot * JPH::Vec3(0, 1, 0); 

                // Offset ray by 1.5 units so it doesn't hit its own head
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

            // Neural Calculation & Actions
            for (size_t i = 0; i < org->segments.size(); i++) {
                // Cost scales with surface area
                float surfaceArea = org->segments[i]->width * org->segments[i]->height;
                float segMetabolism = baseMetabolism * surfaceArea * timeScale;

                // PURPLE nodes reduce metabolism
                if (org->segments[i]->type == ColorType::PURPLE) {
                    segMetabolism *= 0.2f; 
                }
                
                netEnergy -= segMetabolism; 
                
                // Calculate network output
                float neuralSignal = ((1.0f - org->sensorFoodDistance) * org->dna[i].weight_FoodSensor) +
                                     ((1.0f - org->sensorHazardDistance) * org->dna[i].weight_HazardSensor) +
                                     org->dna[i].bias;

                if (neuralSignal > 0.0f) {
                    // YELLOW: Forward Thruster
                    if (org->segments[i]->type == ColorType::YELLOW) {
                        float thrust = org->dna[i].param1 * neuralSignal * thrustMultiplier; 
                        jolt.applyThrust(org->segments[i]->joltBodyID, thrust);
                        netEnergy -= std::abs(thrust) * movementCost * timeScale;
                    }
                    // BLUE: Steering / Torque
                    else if (org->segments[i]->type == ColorType::BLUE) {
                        float torque = org->dna[i].param2 * neuralSignal * turnMultiplier;
                        jolt.applyTorque(org->segments[i]->joltBodyID, torque);
                        netEnergy -= std::abs(torque) * movementCost * timeScale;
                    }
                }
            }
            org->energy += netEnergy;

            // Starvation Check
            if (org->energy <= 0) {
                org->isAlive = false;
                for (auto* seg : org->segments) seg->type = ColorType::DEAD;
            }

            // Thread-Safe Reproduction (Queue it up!)
            if (org->energy >= 400.0f && population.size() < maxPopulation) {
                org->energy -= 200.0f;
                std::lock_guard<std::mutex> lock(spawnMutex);
                spawnRequests.push_back({mutateGenome(org->dna), 0, 0, 200.0f}); 
            }
        }); 
        // --- END OF PARALLEL LOOP ---


        // --- SINGLE-THREADED WORLD MANAGEMENT ---
        
        // Spawn Environmental Food
        if (randFloat(rng) < 0.8f * timeScale && population.size() < maxPopulation) {
            spawnRequests.push_back({ { {ColorType::GREEN, 1.0f, 0, 0, 0, 0, 0, -1, 0} }, (randFloat(rng)-0.5f)*100.0f, (randFloat(rng)-0.5f)*100.0f, 100.0f });
        }

        // Process all pending births safely
        for (auto& req : spawnRequests) {
            queueOrganism(req.dna, req.x, req.y, req.energy);
        }
        spawnRequests.clear();

        // Safely destroy Jolt bodies, constraints, and memory for dead entities
        population.erase(std::remove_if(population.begin(), population.end(), [this](OrganismRecord* org) {
            if (org->markedForDeletion) {
                // Collect all body IDs for this organism
                std::vector<uint32_t> bodyIDs;
                for (auto* seg : org->segments) {
                    bodyIDs.push_back(seg->joltBodyID);
                    delete seg;
                }
                
                // Let Jolt cleanly remove constraints AND bodies
                jolt.cleanupOrganism(org->id, bodyIDs);
                
                delete org; 
                return true;
            }
            return false;
        }), population.end());
    }
};