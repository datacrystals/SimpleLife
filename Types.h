#pragma once
#include <vector>
#include <cstdint>
#include <mutex>

enum class ColorType { GREEN, RED, PURPLE, BLUE, YELLOW, WHITE, DEAD };

struct Point {
    float x, y;
    float old_x, old_y;
    float ax, ay; 
};

struct Stick {
    int p1_idx; 
    int p2_idx; 
    float rest_length;
    ColorType type;
    float width; 
};

struct Gene {
    ColorType type; float length; float param1; float param2; 
    float weight_FoodSensor; float weight_HazardSensor; float bias;
    int parentIndex; float branchAngle; 
};

// Replaced the vector typedef with a full Genome struct to hold new traits
struct Genome {
    std::vector<Gene> genes;
    float lifespan = 40.0f;
    int symmetry = 1; // 1 = none, 2 = 180 deg, 3 = 120 deg, etc.
};

struct OrganismRecord {
    int id; Genome dna; 
    std::vector<Point> points;
    std::vector<Stick> sticks;
    
    float energy; float age; bool isAlive; bool markedForDeletion; 
    float sensorFoodDistance; float sensorHazardDistance;
    float damageFlash = 0.0f; // Visual feedback for taking damage
    bool hasShield = false;   // Track if purple sticks are present

    std::mutex orgMutex;
    float reproCooldown = 5.0f;

    OrganismRecord(int _id, Genome _dna, float _energy) 
        : id(_id), dna(_dna), energy(_energy), age(0.0f), isAlive(true), 
          markedForDeletion(false), sensorFoodDistance(1.0f), sensorHazardDistance(1.0f) {}
};