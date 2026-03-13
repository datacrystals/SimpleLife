#pragma once
#include <vector>
#include <cstdint>
#include <mutex> 

namespace JPH { class BodyID; }
enum class ColorType { GREEN, RED, PURPLE, BLUE, YELLOW, WHITE, DEAD };

struct OrganismRecord;

struct Segment {
    ColorType type;
    uint32_t joltBodyID; 
    float width, height;
    OrganismRecord* parentOrg; 
    
    // NEW: Multithreaded Render Vertices
    // We will use the 64 cores to calculate exactly where the corners are!
    float vX[4] = {0,0,0,0};
    float vY[4] = {0,0,0,0};
};

struct Gene {
    ColorType type; float length; float param1; float param2; 
    float weight_FoodSensor; float weight_HazardSensor; float bias;
    int parentIndex; float branchAngle; 
};

typedef std::vector<Gene> Genome;

struct OrganismRecord {
    int id; Genome dna; std::vector<Segment*> segments; std::vector<uint32_t> muscleJointIDs; 
    float energy; float age; bool isAlive; bool markedForDeletion; 
    float sensorFoodDistance; float sensorHazardDistance;

    std::mutex orgMutex;

    OrganismRecord(int _id, Genome _dna, float _energy) 
        : id(_id), dna(_dna), energy(_energy), age(0.0f), isAlive(true), 
          markedForDeletion(false), sensorFoodDistance(1.0f), sensorHazardDistance(1.0f) {}
};