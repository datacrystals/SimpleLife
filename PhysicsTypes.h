/**
 * @file PhysicsTypes.h
 * @brief Core data structures for the 2D Verlet physics engine.
 * * These structures represent pure physical properties. They contain no 
 * biological, genetic, or rendering metadata.
 */

#pragma once


/**
 * @brief A physical point mass in the 2D world.
 */
struct PhysicsPoint {
    float x, y;           // Current position
    float old_x, old_y;   // Previous position (used for Verlet velocity derivation)
    float ax, ay;         // Accumulated acceleration for the current frame
    float mass = 1.0f;    // Affects how much this point yields during constraint resolution
};

/**
 * @brief A structural constraint connecting two points.
 * Acts as both rigid bones and dynamic muscles depending on stiffness 
 * and how target_length is manipulated over time.
 */
struct PhysicsSpring {
    int p1_idx;           // Index of the first connected point
    int p2_idx;           // Index of the second connected point
    float target_length;  // The resting length the spring attempts to maintain
    float stiffness = 1.0f; // 1.0 = Rigid, < 1.0 = Elastic/Soft
};
 