/**
 * @file PhysicsEngine.cpp
 * @brief Implementation of the 2D Verlet physics solver.
 */

#include "PhysicsEngine.h"
#include <cmath>


PhysicsEngine::PhysicsEngine(float width, float height) 
    : worldWidth(width), worldHeight(height) {}

void PhysicsEngine::getToroidalDiff(float x1, float y1, float x2, float y2, float& dx, float& dy) const {
    dx = x2 - x1;
    dy = y2 - y1;
    
    // Wrap around shortest path logic
    if (dx > worldWidth * 0.5f) dx -= worldWidth;
    else if (dx < -worldWidth * 0.5f) dx += worldWidth;
    
    if (dy > worldHeight * 0.5f) dy -= worldHeight;
    else if (dy < -worldHeight * 0.5f) dy += worldHeight;
}

void PhysicsEngine::step(std::vector<PhysicsPoint>& points, std::vector<PhysicsSpring>& springs, float dt) {
    if (dt <= 0.0f || points.empty()) return;

    // ---------------------------------------------------------
    // 1. Verlet Integration (Update positions based on inertia)
    // ---------------------------------------------------------
    for (auto& p : points) {
        // Derive velocity from position difference
        float vx = (p.x - p.old_x) * friction; 
        float vy = (p.y - p.old_y) * friction;

        // Instead of uniform friction, apply drag based on velocity vector
        float speedSq = vx * vx + vy * vy;
        if (speedSq > 0.0001f) {
            // Basic fluid drag: resistance increases with the square of velocity
            float drag = speedSq * 0.01f; 
            vx *= (1.0f - drag * dt);
            vy *= (1.0f - drag * dt);
        }

        // Save current position as old position
        p.old_x = p.x; 
        p.old_y = p.y;
        
        // Apply velocity and current frame's acceleration
        p.x += vx + p.ax * dt * dt;
        p.y += vy + p.ay * dt * dt;
        
        // Reset acceleration for the next frame
        p.ax = 0.0f; 
        p.ay = 0.0f;



        // Toroidal Boundary Enforcement (Wrap around screen edges)
        if (p.x < 0) { p.x += worldWidth; p.old_x += worldWidth; }
        else if (p.x >= worldWidth) { p.x -= worldWidth; p.old_x -= worldWidth; }
        
        if (p.y < 0) { p.y += worldHeight; p.old_y += worldHeight; }
        else if (p.y >= worldHeight) { p.y -= worldHeight; p.old_y -= worldHeight; }
    }

    // ---------------------------------------------------------
    // 2. Constraint Resolution (Satisfy resting lengths)
    // ---------------------------------------------------------
    for (int iter = 0; iter < constraintIterations; iter++) {
        for (auto& spring : springs) {
            // Bounds check (safety for evolutionary mutations)
            if (spring.p1_idx < 0 || spring.p1_idx >= points.size() ||
                spring.p2_idx < 0 || spring.p2_idx >= points.size()) {
                continue; 
            }

            PhysicsPoint& p1 = points[spring.p1_idx];
            PhysicsPoint& p2 = points[spring.p2_idx];
            
            float dx, dy;
            getToroidalDiff(p1.x, p1.y, p2.x, p2.y, dx, dy);
            
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 0.0001f) continue; // Prevent division by zero
            
            // Calculate how far the spring is from its target length
            // Apply stiffness: 1.0 = full correction, lower = soft/bouncy
            float diff = (spring.target_length - dist) / dist * 0.5f * spring.stiffness;
            
            // Mass-based resolution: heavier points get pulled less
            float totalMass = p1.mass + p2.mass;
            if (totalMass <= 0.0001f) continue; // Safety check

            float ratio1 = p2.mass / totalMass;
            float ratio2 = p1.mass / totalMass;

            // Push/pull points towards the target length
            p1.x -= dx * diff * ratio1 * 2.0f; 
            p1.y -= dy * diff * ratio1 * 2.0f;
            
            p2.x += dx * diff * ratio2 * 2.0f; 
            p2.y += dy * diff * ratio2 * 2.0f;
        }
    }
}
