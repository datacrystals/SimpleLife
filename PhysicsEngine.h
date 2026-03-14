/**
 * @file PhysicsEngine.h
 * @brief A standalone 2D Verlet integration engine with toroidal (wrap-around) space.
 * * Handles positional updates, velocity capping, and constraint resolution 
 * for networks of masses and springs.
 */

#pragma once
#include <vector>
#include "PhysicsTypes.h"


class PhysicsEngine {
public:
    float worldWidth;
    float worldHeight;
    
    // Physics solver configuration
    int constraintIterations = 4; // Higher = stiffer structures, more CPU cost
    float friction = 0.95f;       // Velocity multiplier per tick
    float maxVelocity = 25.0f;    // Terminal velocity cap

    PhysicsEngine(float width, float height);

    /**
     * @brief Calculates the shortest distance vector between two points in a wrap-around world.
     */
    void getToroidalDiff(float x1, float y1, float x2, float y2, float& dx, float& dy) const;

    /**
     * @brief Advances the physics simulation by one time step (dt).
     * * @param points The array of point masses to update.
     * @param springs The array of constraints connecting the points.
     * @param dt The delta time step.
     */
    void step(std::vector<PhysicsPoint>& points, std::vector<PhysicsSpring>& springs, float dt);

    /**
     * @brief Resolves overlap between points using a spatial hash grid for performance.
     */
    void resolveGlobalCollisions(const std::vector<PhysicsPoint*>& allPoints, float collisionRadius, float repulsionStrength, float dt);
};
 