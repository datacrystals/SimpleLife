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
#include <Jolt/Physics/Body/BodyLockMulti.h> // FIX: Included this for BodyLockMultiWrite
#include <iostream>
#include <mutex>
#include <unordered_map> // FIX: Included for joint tracking
#include <vector>

using namespace JPH;

// Jolt requires broad-phase layers. We'll use 0 for moving, 1 for static (food).
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

// Thread-safe eating!
class BiologyContactListener : public ContactListener {
    std::mutex eatingMutex;
public:
    virtual void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
        Segment* segA = reinterpret_cast<Segment*>(inBody1.GetUserData());
        Segment* segB = reinterpret_cast<Segment*>(inBody2.GetUserData());
        if (!segA || !segB || segA->parentOrg == segB->parentOrg) return;

        std::lock_guard<std::mutex> lock(eatingMutex);
        
        // Instant Eating (Food)
        if (segA->type != ColorType::GREEN && segB->type == ColorType::GREEN) {
            segA->parentOrg->energy += 40.0f; segB->parentOrg->markedForDeletion = true;
        } else if (segB->type != ColorType::GREEN && segA->type == ColorType::GREEN) {
            segB->parentOrg->energy += 40.0f; segA->parentOrg->markedForDeletion = true;
        }
        
        // Combat
        if (segA->type == ColorType::RED && segB->parentOrg->isAlive && segB->type != ColorType::RED) {
            segB->parentOrg->energy -= 25.0f; segA->parentOrg->energy += 25.0f;
        }
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

    // Safely tracks the reference-counted joints for each organism
    std::unordered_map<int, std::vector<Ref<Constraint>>> orgJoints;

    JoltWrapper() {
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);
        jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

        physicsSystem = new PhysicsSystem();
        physicsSystem->Init(10240, 0, 10240, 10240, broadPhaseLayerInterface, objectVsBroadphaseLayerFilter, objectVsObjectLayerFilter);
        physicsSystem->SetGravity(Vec3(0, 0, 0));
        physicsSystem->SetContactListener(&contactListener);
    }

    ~JoltWrapper() {
        delete physicsSystem;
        delete jobSystem;
        delete tempAllocator;
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }

    void stepPhysics(float dt) {
        physicsSystem->Update(dt, 1, tempAllocator, jobSystem);
    }

    uint32_t createSegment(float x, float y, float w, float h, ColorType type, Segment* userData, int orgID) {
        BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        BoxShapeSettings* shapeSettings = new BoxShapeSettings(Vec3(w/2.0f, h/2.0f, 0.5f));
                
        BodyCreationSettings settings(shapeSettings, RVec3(x, y, 0), Quat::sIdentity(), 
            (type == ColorType::GREEN) ? EMotionType::Static : EMotionType::Dynamic, 
            (type == ColorType::GREEN) ? Layers::STATIC : Layers::MOVING);
            
        settings.mLinearDamping = 1.0f; 
        settings.mAngularDamping = 2.0f;
        settings.mUserData = reinterpret_cast<uint64_t>(userData);
        settings.mCollisionGroup.SetGroupID(orgID); 
        
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = 1.0f;
        
        // Lock Z axis and 3D rotations!
        settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::RotationZ;

        Body* body = bodyInterface.CreateBody(settings);
        bodyInterface.AddBody(body->GetID(), EActivation::Activate);
        return body->GetID().GetIndexAndSequenceNumber();
    }

    // Safe Locking & Memory Management for Muscles
    void createMuscle(int orgID, uint32_t idA, uint32_t idB, float anchorX, float anchorY) {
        BodyID bA(idA), bB(idB);
        BodyID ids[2] = { bA, bB };
        
        // JPH::BodyLockMultiWrite automatically sorts locks and deduplicates mutexes to prevent deadlocks!
        BodyLockMultiWrite locks(physicsSystem->GetBodyLockInterface(), ids, 2);
        
        Body* bodyA = locks.GetBody(0);
        Body* bodyB = locks.GetBody(1);
        
        if (bodyA && bodyB) {
            HingeConstraintSettings hinge;
            hinge.mPoint1 = hinge.mPoint2 = RVec3(anchorX, anchorY, 0);
            hinge.mHingeAxis1 = hinge.mHingeAxis2 = Vec3(0, 0, 1);
            hinge.mNormalAxis1 = hinge.mNormalAxis2 = Vec3(1, 0, 0);
            
            // By assigning to Ref<Constraint>, the memory won't be instantly deleted!
            Ref<Constraint> constraint = hinge.Create(*bodyA, *bodyB);
            physicsSystem->AddConstraint(constraint);
            
            // Store it so we can safely delete it when the organism dies
            orgJoints[orgID].push_back(constraint);
        }
    }

    // A safe way to delete an organism's physical presence
    void cleanupOrganism(int orgID, const std::vector<uint32_t>& bodyIDs) {
        // Must remove constraints BEFORE removing the bodies they are attached to!
        auto it = orgJoints.find(orgID);
        if (it != orgJoints.end()) {
            for (auto& constraint : it->second) {
                physicsSystem->RemoveConstraint(constraint);
            }
            orgJoints.erase(it);
        }
        
        // Now safely remove and destroy the bodies
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
        Vec3 forward = rot * Vec3(0, 1, 0); // Jolt Y is up
        bi.AddForce(bID, forward * force);
    }

    void applyTorque(uint32_t id, float torque) {
        BodyInterface& bi = physicsSystem->GetBodyInterface();
        BodyID bID(id);
        if(!bi.IsAdded(bID)) return;
        
        // Jolt is right-handed. Rotating around the Z-axis turns the object on the 2D XY plane.
        bi.AddTorque(bID, Vec3(0, 0, torque));
    }
};