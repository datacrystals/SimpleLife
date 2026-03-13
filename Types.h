#pragma once
#include <vector>
#include <cstdint>

// Forward declare Jolt's BodyID so we don't need to include all of Jolt here
namespace JPH { class BodyID; }

enum class ColorType { GREEN, RED, PURPLE, BLUE, YELLOW, WHITE, DEAD };

struct OrganismRecord;

struct Segment {
    ColorType type;
    uint32_t joltBodyID; // Jolt uses IDs instead of raw pointers for thread safety
    float width, height;
    OrganismRecord* parentOrg; 
};

struct Gene {
    ColorType type;
    float length;
    float param1; 
    float param2; 
    float weight_FoodSensor; 
    float weight_HazardSensor; 
    float bias;
    
    // NEW: Allow complex branching shapes!
    int parentIndex;   // -1 for root (head), otherwise index of the segment it attaches to
    float branchAngle; // Used for symmetrical mirroring
};

typedef std::vector<Gene> Genome;

struct OrganismRecord {
    int id;
    Genome dna;
    std::vector<Segment*> segments;
    std::vector<uint32_t> muscleJointIDs; // Jolt Constraint IDs
    
    float energy;
    float age; 
    bool isAlive;
    bool markedForDeletion; 

    float sensorFoodDistance;
    float sensorHazardDistance;
};