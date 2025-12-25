// physics_auth.c
// Optimized physics-based authentication engine
#include "physics_auth.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Physics constants (from ID14/ID26)
#define U_E 86.4f
#define I_CHAR 8.0f
#define R_CHAR 8.0f
#define ALPHA_PSI 3.0f
#define BETA_PSI 0.5f

// Agent state
typedef struct {
    float I;
    float R;
    float Psi;
    float Phi[PHI_SIZE];
} AgentState;

// Initialize physics engine
void auth_init(void) {
    // Reserved for future hardware initialization
}

// Simple random number generator (xorshift32)
static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// Fast tanh approximation
static inline float fast_tanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Initialize agent state with seed
static void init_agent_state(AgentState* agent, uint32_t seed) {
    uint32_t rng_state = seed;
    
    agent->I = 0.1f;
    agent->R = 0.1f;
    agent->Psi = 0.1f;
    
    // Initialize Phi field with seeded random values
    for (int i = 0; i < PHI_SIZE; i++) {
        agent->Phi[i] = (float)(xorshift32(&rng_state) & 0xFFFF) / 65535.0f * 0.1f;
    }
}

// Evolve physics by one time step (Euler method for speed)
static void evolve_step(AgentState* agent, float k, float gamma, float phi_input) {
    // Compute average Phi
    float phi_avg = 0.0f;
    for (int i = 0; i < PHI_SIZE; i++) {
        phi_avg += agent->Phi[i];
    }
    phi_avg /= PHI_SIZE;
    
    // ID14 dynamics (simplified for embedded)
    float dI = k * phi_avg - (U_E / I_CHAR) * agent->I * 0.5f;
    float dR = 0.1f * agent->I * agent->Psi - (2.0f * U_E / R_CHAR) * agent->R * 0.3f;
    float dPsi = ALPHA_PSI * agent->I - BETA_PSI * agent->R - gamma * agent->Psi;
    
    // Update state
    agent->I += dI * DT;
    agent->R += dR * DT;
    agent->Psi += dPsi * DT;
    
    // Update Phi field (simplified diffusion)
    for (int i = 0; i < PHI_SIZE; i++) {
        float source = fast_tanh(agent->Psi * 0.5f - agent->Phi[i]);
        agent->Phi[i] += (source * 0.1f + phi_input * 0.01f) * DT;
    }
}

// Compute authentication response
AuthResponse auth_compute_response(const float* challenge, int length, const AuthSecret* secret) {
    AgentState agent;
    
    // Initialize with secret
    init_agent_state(&agent, secret->seed);
    
    // Apply challenge perturbations
    for (int t = 0; t < length; t++) {
        evolve_step(&agent, secret->k, secret->gamma, challenge[t]);
    }
    
    // Compute final phi_avg
    float phi_avg = 0.0f;
    for (int i = 0; i < PHI_SIZE; i++) {
        phi_avg += agent.Phi[i];
    }
    phi_avg /= PHI_SIZE;
    
    // Return response
    AuthResponse resp;
    resp.psi = agent.Psi;
    resp.i_val = agent.I;
    resp.r_val = agent.R;
    resp.phi_avg = phi_avg;
    
    return resp;
}

// Verify response matches expected
int auth_verify(const AuthResponse* received, const AuthResponse* expected, float tolerance) {
    float diff_psi = fabsf(received->psi - expected->psi);
    float diff_i = fabsf(received->i_val - expected->i_val);
    float diff_r = fabsf(received->r_val - expected->r_val);
    float diff_phi = fabsf(received->phi_avg - expected->phi_avg);
    
    return (diff_psi < tolerance) && 
           (diff_i < tolerance) && 
           (diff_r < tolerance) && 
           (diff_phi < tolerance);
}
