#pragma once
#include "Types.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h> 
#include <Jolt/Physics/Collision/GroupFilter.h> // NEW: For ignoring self-collisions
#include <iostream>
#include <mutex>
#include <unordered_map> 
#include <vector>

using namespace JPH;

namespace Layers {
    static constexpr ObjectLayer MOVING = 0;
    static constexpr ObjectLayer STATIC = 1;
    static constexpr uint32 NUM_LAYERS = 2;
};

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayer(0);
        mObjectToBroadPhase[Layers::STATIC] = BroadPhaseLayer(1);
    }
    virtual uint32 GetNumBroadPhaseLayers() const override { return 2; }
    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override { return "Layer"; }
#endif
private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override { return true; }
};

class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, ObjectLayer inLayer2) const override { return true; }
};

// NEW: A custom filter to tell Jolt to ignore collisions between limbs of the same organism!
class BiologyGroupFilter : public GroupFilter {
public:
    virtual bool CanCollide(const CollisionGroup &inGroup1, const CollisionGroup &inGroup2) const override {
        // If they share the same organism ID, they phase through each other.
        return inGroup1.GetGroupID() != inGroup2.GetGroupID();
    }
};

class BiologyContactListener : public ContactListener {
    public:
        virtual void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
            Segment* segA = reinterpret_cast<Segment*>(inBody1.GetUserData());
            Segment* segB = reinterpret_cast<Segment*>(inBody2.GetUserData());
            if (!segA || !segB || segA->parentOrg == segB->parentOrg) return;
    
            auto tryEatPlant = [](Segment* eater, Segment* food) {
                if (eater->type == ColorType::WHITE && food->type == ColorType::GREEN) {
                    // scoped_lock locks both organisms instantly without causing deadlocks!
                    std::scoped_lock lock(eater->parentOrg->orgMutex, food->parentOrg->orgMutex);
                    if (food->parentOrg->isAlive) {
                        eater->parentOrg->energy += 120.0f; 
                        food->parentOrg->markedForDeletion = true; 
                        food->parentOrg->isAlive = false;
                    }
                }
            };
            tryEatPlant(segA, segB);
            tryEatPlant(segB, segA);
            
            auto tryAttack = [](Segment* attacker, Segment* victim) {
                if (attacker->type == ColorType::RED && victim->type != ColorType::RED) {
                    std::scoped_lock lock(attacker->parentOrg->orgMutex, victim->parentOrg->orgMutex);
                    if (victim->parentOrg->isAlive) {
                        victim->parentOrg->energy -= 60.0f; 
                        attacker->parentOrg->energy += 40.0f; 
                    }
                }
            };
            tryAttack(segA, segB);
            tryAttack(segB, segA);
        }
    };
    


class JoltWrapper {
public:
    TempAllocatorImpl* tempAllocator;
    JobSystemThreadPool* jobSystem;
    BPLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphaseLayerFilter;
    ObjectLayerPairFilterImpl objectVsObjectLayerFilter;
    PhysicsSystem* physicsSystem;
    BiologyContactListener contactListener;
    
    Ref<GroupFilter> bioGroupFilter; // Stores our self-collision rules

    std::unordered_map<int, std::vector<Ref<Constraint>>> orgJoints;

    JoltWrapper() {
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        tempAllocator = new TempAllocatorImpl(1 * 1024 * 1024 * 1024);
        jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

        physicsSystem = new PhysicsSystem();
        physicsSystem->Init(65536, 0, 65536, 65536, broadPhaseLayerInterface, objectVsBroadphaseLayerFilter, objectVsObjectLayerFilter);
        physicsSystem->SetGravity(Vec3(0, 0, 0));
        physicsSystem->SetContactListener(&contactListener);
        
        // Initialize our custom group filter
        bioGroupFilter = new BiologyGroupFilter();
    }

    ~JoltWrapper() {
        delete physicsSystem;
        delete jobSystem;
        delete tempAllocator;
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }

    // FIX: We only clean up joints now. Bodies will be deleted in a hyper-fast batch.
    void cleanupJoints(int orgID) {
        auto it = orgJoints.find(orgID);
        if (it != orgJoints.end()) {
            for (auto& constraint : it->second) physicsSystem->RemoveConstraint(constraint);
            orgJoints.erase(it);
        }
    }

    void stepPhysics(float dt) {
        physicsSystem->Update(dt, 1, tempAllocator, jobSystem);
    }

    uint32_t createSegment(float x, float y, float w, float h, ColorType type, Segment* userData, int orgID) {
        BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        
        // FIX: Reverted back to heap allocation. Jolt's internal RefConst smart pointer 
        // takes ownership of this and safely deletes it when 'settings' goes out of scope.
        BoxShapeSettings* shapeSettings = new BoxShapeSettings(Vec3(w/2.0f, h/2.0f, 0.5f));
                
        BodyCreationSettings settings(shapeSettings, RVec3(x, y, 0), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
            
        settings.mLinearDamping = 3.0f; 
        settings.mAngularDamping = 5.0f;
        settings.mUserData = reinterpret_cast<uint64_t>(userData);
        
        // This is the real fix from last time: prevents limbs from exploding each other
        settings.mCollisionGroup.SetGroupFilter(bioGroupFilter);
        settings.mCollisionGroup.SetGroupID(orgID); 
        
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = 1.0f;
        
        settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::RotationZ;

        Body* body = bodyInterface.CreateBody(settings);
        bodyInterface.AddBody(body->GetID(), EActivation::Activate);
        return body->GetID().GetIndexAndSequenceNumber();
    }

    void createMuscle(int orgID, uint32_t idA, uint32_t idB, float anchorX, float anchorY) {
        BodyID bA(idA), bB(idB);
        BodyID ids[2] = { bA, bB };
        
        BodyLockMultiWrite locks(physicsSystem->GetBodyLockInterface(), ids, 2);
        
        Body* bodyA = locks.GetBody(0);
        Body* bodyB = locks.GetBody(1);
        
        if (bodyA && bodyB) {
            HingeConstraintSettings hinge;
            hinge.mPoint1 = hinge.mPoint2 = RVec3(anchorX, anchorY, 0);
            hinge.mHingeAxis1 = hinge.mHingeAxis2 = Vec3(0, 0, 1);
            hinge.mNormalAxis1 = hinge.mNormalAxis2 = Vec3(1, 0, 0);
            
            Ref<Constraint> constraint = hinge.Create(*bodyA, *bodyB);
            physicsSystem->AddConstraint(constraint);
            orgJoints[orgID].push_back(constraint);
        }
    }

    void cleanupOrganism(int orgID, const std::vector<uint32_t>& bodyIDs) {
        auto it = orgJoints.find(orgID);
        if (it != orgJoints.end()) {
            for (auto& constraint : it->second) {
                physicsSystem->RemoveConstraint(constraint);
            }
            orgJoints.erase(it);
        }
        
        for (uint32_t id : bodyIDs) {
            physicsSystem->GetBodyInterface().RemoveBody(BodyID(id));
            physicsSystem->GetBodyInterface().DestroyBody(BodyID(id));
        }
    }

    void applyThrust(uint32_t id, float force) {
        BodyInterface& bi = physicsSystem->GetBodyInterface();
        BodyID bID(id);
        if(!bi.IsAdded(bID)) return;
        Quat rot = bi.GetRotation(bID);
        Vec3 forward = rot * Vec3(0, 1, 0); 
        bi.AddForce(bID, forward * force);
    }

    void applyTorque(uint32_t id, float torque) {
        BodyInterface& bi = physicsSystem->GetBodyInterface();
        BodyID bID(id);
        if(!bi.IsAdded(bID)) return;
        bi.AddTorque(bID, Vec3(0, 0, torque));
    }
};