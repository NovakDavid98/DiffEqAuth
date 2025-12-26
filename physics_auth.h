// physics_auth_v2.h
// HARDENED physics-based authentication - Resistant to ML attacks
// Fixes: Added chaos, deep challenge integration, continuous seed usage

#ifndef PHYSICS_AUTH_V2_H
#define PHYSICS_AUTH_V2_H

#include <stdint.h>

// Configuration - INCREASED for security
#define PHI_SIZE 100
#define DT 0.05f              // Smaller timestep for more iterations
#define CHALLENGE_LENGTH 50
#define EVOLUTION_STEPS 200      // 4x more steps than v1

// Lorenz attractor parameters (for chaos)
#define LORENZ_SIGMA 10.0f
#define LORENZ_RHO 28.0f
#define LORENZ_BETA 2.666667f

// Secret key structure (unchanged)
typedef struct {
    float k;
    float gamma;
    uint32_t seed;
} AuthSecret;

// EXPANDED response structure (8 values instead of 4)
typedef struct {
    float psi;
    float i_val;
    float r_val;
    float phi_avg;
    // NEW: Additional chaos outputs
    float lorenz_x;
    float lorenz_y;
    float lorenz_z;
    float entropy_hash;    // Nonlinear hash of all state
} AuthResponse;

// Core API
void auth_init(void);
AuthResponse auth_compute_response(const float* challenge, int length, const AuthSecret* secret);
int auth_verify(const AuthResponse* received, const AuthResponse* expected, float tolerance);

#endif // PHYSICS_AUTH_V2_H
