/**
 * @file PhysicsEngine.cpp
 * @brief Implementation of the 2D Verlet physics solver.
 */

#include "PhysicsEngine.h"
#include <cmath>
#include <execution>
#include <vector>


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


#include <execution>
#include <cmath>

void PhysicsEngine::resolveGlobalCollisions(const std::vector<PhysicsPoint*>& allPoints, float collisionRadius, float repulsionStrength, float dt) {
    if (allPoints.empty()) return;

    // Cell size must be at least the max interaction distance to check only 1-ring neighbors
    float cellSize = 20.0f;//std::max(collisionRadius, 2.0f); 
    int cols = std::max(1, (int)std::ceil(worldWidth / cellSize));
    int rows = std::max(1, (int)std::ceil(worldHeight / cellSize));
    
    // 1. Populate the Read-Only Grid (Sequential, purely memory-bound)
    std::vector<std::vector<PhysicsPoint*>> grid(cols * rows);
    for (auto* p : allPoints) {
        // Prevent NaNs from poisoning the spatial hash and causing Segfaults
        if (std::isnan(p->x) || std::isnan(p->y)) continue;

        float wrappedX = std::fmod(p->x + worldWidth, worldWidth);
        float wrappedY = std::fmod(p->y + worldHeight, worldHeight);
        if (wrappedX < 0.0f) wrappedX += worldWidth;
        if (wrappedY < 0.0f) wrappedY += worldHeight;
        
        // Bulletproof modulo to guarantee we never get a negative array index
        int cx = ((static_cast<int>(wrappedX / cellSize) % cols) + cols) % cols;
        int cy = ((static_cast<int>(wrappedY / cellSize) % rows) + rows) % rows;
        
        grid[cy * cols + cx].push_back(p);
    }

    float minDistSq = collisionRadius * collisionRadius;

    // 2. 100% Core Saturation via Point-Level Parallelism
    // Threads read from the shared grid but ONLY write to their assigned p1, guaranteeing no races.
    std::for_each(std::execution::par, allPoints.begin(), allPoints.end(), [&](PhysicsPoint* p1) {
        if (std::isnan(p1->x) || std::isnan(p1->y)) return;

        float wrappedX = std::fmod(p1->x + worldWidth, worldWidth);
        float wrappedY = std::fmod(p1->y + worldHeight, worldHeight);
        if (wrappedX < 0.0f) wrappedX += worldWidth;
        if (wrappedY < 0.0f) wrappedY += worldHeight;

        int cx = ((static_cast<int>(wrappedX / cellSize) % cols) + cols) % cols;
        int cy = ((static_cast<int>(wrappedY / cellSize) % rows) + rows) % rows;
        
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = ((cx + dx) % cols + cols) % cols;
                int ny = ((cy + dy) % rows + rows) % rows;
                auto& neighborCell = grid[ny * cols + nx];

                for (auto* p2 : neighborCell) {
                    // Skip self and sibling nodes
                    if (p1 == p2 || p1->parentOrgId == p2->parentOrgId) continue;

                    float diffX, diffY;
                    getToroidalDiff(p1->x, p1->y, p2->x, p2->y, diffX, diffY);
                    float distSq = diffX*diffX + diffY*diffY;

                    if (distSq > 0.0001f && distSq < minDistSq) {
                        float dist = std::sqrt(distSq);
                        float overlap = collisionRadius - dist; // Fixed: No longer 2x radius
                        
                        // Prevent division by zero if points are perfectly stacked
                        float safeDist = std::max(dist, 0.001f);
                        
                        // Apply force as stable ACCELERATION. Fixed: Removed the 10x explosion multiplier.
                        float force = overlap * repulsionStrength; 
                        p1->ax -= (diffX / safeDist) * force;
                        p1->ay -= (diffY / safeDist) * force;
                    }
                }
            }
        }
    });
}


void PhysicsEngine::updateBounds(float w, float h) {
    worldWidth = w;
    worldHeight = h;
}