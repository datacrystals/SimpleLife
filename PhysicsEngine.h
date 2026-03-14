#pragma once
#include <vector>
#include "PhysicsTypes.h"

class Organism; // Forward declaration

class PhysicsEngine {
public:
    float worldWidth;
    float worldHeight;
    
    // Grid for Organism-level lookups (Biology/Combat)
    float orgCellSize = 40.0f;
    std::vector<std::vector<Organism*>> orgGrid;
    int orgCols, orgRows;

    int constraintIterations = 4;
    float friction = 0.95f; 
    float maxVelocity = 25.0f;

    PhysicsEngine(float width, float height);
    
    // --- New Methods for World Logic ---
    void clearOrgGrid();
    void addOrgToGrid(float x, float y, Organism* org);
    float getNearbyCount(float x, float y, float radius) const;
    std::vector<Organism*> getNearbyOrganisms(float x, float y, float radius) const;

    void getToroidalDiff(float x1, float y1, float x2, float y2, float& dx, float& dy) const;
    void step(std::vector<PhysicsPoint>& points, std::vector<PhysicsSpring>& springs, float dt);
    void resolveGlobalCollisions(const std::vector<PhysicsPoint*>& allPoints, float collisionRadius, float repulsionStrength, float dt);
    void updateBounds(float w, float h);
};