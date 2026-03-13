// Types.h
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
    float width; // Used for rendering and energy calculations
};

struct Gene {
    ColorType type; float length; float param1; float param2; 
    float weight_FoodSensor; float weight_HazardSensor; float bias;
    int parentIndex; float branchAngle; 
};

typedef std::vector<Gene> Genome;

struct OrganismRecord {
    int id; Genome dna; 
    std::vector<Point> points;
    std::vector<Stick> sticks;
    
    float energy; float age; bool isAlive; bool markedForDeletion; 
    float sensorFoodDistance; float sensorHazardDistance;

    std::mutex orgMutex;

    float reproCooldown = 5.0f; // Seconds between babies

    OrganismRecord(int _id, Genome _dna, float _energy) 
        : id(_id), dna(_dna), energy(_energy), age(0.0f), isAlive(true), 
          markedForDeletion(false), sensorFoodDistance(1.0f), sensorHazardDistance(1.0f) {}
};