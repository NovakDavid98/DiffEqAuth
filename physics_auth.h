// physics_auth.h
// Optimized physics-based authentication for Raspberry Pi
#ifndef PHYSICS_AUTH_H
#define PHYSICS_AUTH_H

#include <stdint.h>

// Configuration
#define PHI_SIZE 100
#define DT 0.1f
#define CHALLENGE_LENGTH 50

// Secret key structure
typedef struct {
    float k;          // k_{I,phi} sensitivity parameter
    float gamma;      // gamma_psi decay parameter
    uint32_t seed;    // Initial condition seed
} AuthSecret;

// Response structure
typedef struct {
    float psi;
    float i_val;
    float r_val;
    float phi_avg;
} AuthResponse;

// Core API
void auth_init(void);
AuthResponse auth_compute_response(const float* challenge, int length, const AuthSecret* secret);
int auth_verify(const AuthResponse* received, const AuthResponse* expected, float tolerance);

#endif // PHYSICS_AUTH_H
