// physics_auth_v2.c
// HARDENED physics-based authentication engine
// 
// SECURITY FIXES IMPLEMENTED:
// 1. Added Lorenz attractor for true chaos (sensitive to initial conditions)
// 2. Challenge deeply integrated (affects ALL dynamics, not just input)
// 3. Seed used CONTINUOUSLY (not just initialization)
// 4. 4x more evolution steps (200 vs 50)
// 5. 8 output channels instead of 4 (harder to predict)
// 6. Nonlinear entropy hash (destroys ML predictability)

#include "physics_auth.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Physics constants
#define U_E 86.4f
#define I_CHAR 8.0f
#define R_CHAR 8.0f
#define ALPHA_PSI 3.0f
#define BETA_PSI 0.5f

// Agent state - now includes Lorenz variables
typedef struct {
    float I;
    float R;
    float Psi;
    float Phi[PHI_SIZE];
    // NEW: Lorenz attractor state
    float Lx, Ly, Lz;
    // NEW: Running entropy accumulator
    float entropy;
} AgentState;

void auth_init(void) {
    // Reserved for future hardware initialization
}

// xorshift32 PRNG
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

// NEW: Nonlinear mixing function (breaks ML predictability)
static inline float mix_nonlinear(float a, float b, float c) {
    float x = sinf(a * 3.14159f) * cosf(b * 2.71828f);
    float y = fast_tanh(c * x);
    return x * y + sinf(a * b + c);
}

// Initialize agent state with seed - MODIFIED: affects Lorenz initial conditions
static void init_agent_state(AgentState* agent, uint32_t seed) {
    uint32_t rng_state = seed;
    
    agent->I = 0.1f;
    agent->R = 0.1f;
    agent->Psi = 0.1f;
    
    // Initialize Phi field with seeded random values
    for (int i = 0; i < PHI_SIZE; i++) {
        agent->Phi[i] = (float)(xorshift32(&rng_state) & 0xFFFF) / 65535.0f * 0.1f;
    }
    
    // NEW: Initialize Lorenz attractor with seed-dependent starting point
    // Small differences here cause MASSIVE divergence due to chaos
    agent->Lx = 1.0f + (float)(xorshift32(&rng_state) & 0xFFFF) / 65535.0f * 0.01f;
    agent->Ly = 1.0f + (float)(xorshift32(&rng_state) & 0xFFFF) / 65535.0f * 0.01f;
    agent->Lz = 1.0f + (float)(xorshift32(&rng_state) & 0xFFFF) / 65535.0f * 0.01f;
    
    agent->entropy = 0.0f;
}

// MODIFIED: V3 HARDENING - discrete chaotic maps + wrapping
static void evolve_step(AgentState* agent, float k, float gamma, 
                           float phi_input, uint32_t* seed_state, int step) {
    
    // ===== STABILITY via WRAPPING (Modular Arithmetic for Floats) =====
    // This destroys linearity immediately.
    // Instead of clamp(-50, 50), we do fmod(x, 50).
    
    // ===== LORENZ ATTRACTOR (True Chaos) =====
    float dt = 0.02f; 
    
    float dLx = LORENZ_SIGMA * (agent->Ly - agent->Lx);
    float dLy = agent->Lx * (LORENZ_RHO - agent->Lz) - agent->Ly;
    float dLz = agent->Lx * agent->Ly - LORENZ_BETA * agent->Lz;
    
    // Strong coupling
    dLx += phi_input * 2.0f;
    
    agent->Lx += dLx * dt;
    agent->Ly += dLy * dt;
    agent->Lz += dLz * dt;
    
    // WRAP Lorenz state (keeps it bounded but nonlinear)
    if (fabsf(agent->Lx) > 20.0f) agent->Lx = fmodf(agent->Lx, 20.0f);
    if (fabsf(agent->Ly) > 20.0f) agent->Ly = fmodf(agent->Ly, 20.0f);
    if (fabsf(agent->Lz) > 20.0f) agent->Lz = fmodf(agent->Lz, 20.0f);
    
    // ===== DISCRETE CHAOS INJECTION (Logistic Map) =====
    // x_{n+1} = r * x_n * (1 - x_n)
    // We map Psi/I/R into [0,1], iterate, map back.
    // This provides the "avalanche effect" that ODEs lack.
    
    float x = 0.5f + 0.5f * fast_tanh(agent->Psi); // map to (0,1)
    float r = 3.9f + 0.09f * fast_tanh(agent->Lx * 0.1f); // r in [3.81, 3.99] (chaotic regime)
    
    // Iterate map 3 times
    for(int i=0; i<3; i++) x = r * x * (1.0f - x);
    
    // Inject back into Psi
    float chaos_kick = (x - 0.5f) * 2.0f; // map back to (-1,1)
    
    // ===== CONTINUOUS SEED INFLUENCE =====
    float seed_noise = (float)(xorshift32(seed_state) & 0xFFFF) / 65535.0f * 0.1f; // Stronger noise
    
    // ===== PHYSICS EVOLUTION =====
    float phi_avg = 0.0f;
    for (int i = 0; i < PHI_SIZE; i++) {
        phi_avg += agent->Phi[i];
    }
    phi_avg /= PHI_SIZE;
    
    float dI = k * phi_avg + chaos_kick * 0.5f - (U_E / I_CHAR) * agent->I * 0.5f;
    float dR = 0.1f * agent->I * agent->Psi - (2.0f * U_E / R_CHAR) * agent->R * 0.3f;
    float dPsi = ALPHA_PSI * agent->I - BETA_PSI * agent->R - gamma * agent->Psi;
    
    // Update state
    agent->I += dI * DT + seed_noise;
    agent->R += dR * DT;
    agent->Psi += dPsi * DT;
    
    // WRAP Main State (Key to nonlinearity)
    if (fabsf(agent->I) > 5.0f) agent->I = fmodf(agent->I * 1.5f, 5.0f);
    if (fabsf(agent->Psi) > 5.0f) agent->Psi = fmodf(agent->Psi * 1.5f, 5.0f);
    
    // ===== PHI FIELD EVOLUTION =====
    for (int i = 0; i < PHI_SIZE; i++) {
        // Highly nonlinear position factor using sin(Lx * i)
        float position_factor = sinf((float)i * 0.5f + agent->Lx);
        float source = fast_tanh(agent->Psi * 0.5f - agent->Phi[i]);
        agent->Phi[i] += (source * 0.2f + phi_input * 0.1f * position_factor) * DT;
        
        // Wrap field
        if (fabsf(agent->Phi[i]) > 3.0f) agent->Phi[i] = fmodf(agent->Phi[i], 3.0f);
    }
    
    // ===== ENTROPY ACCUMULATION =====
    agent->entropy += mix_nonlinear(agent->Psi, agent->Lx, phi_input);
    agent->entropy = fmodf(agent->entropy, 1000.0f);
}

// Compute authentication response - HARDENED VERSION
AuthResponse auth_compute_response(const float* challenge, int length, const AuthSecret* secret) {
    AgentState agent;
    uint32_t seed_state = secret->seed;  // Mutable copy for continuous use
    
    // Initialize with secret
    init_agent_state(&agent, secret->seed);
    
    // ===== EXTENDED EVOLUTION: 200 steps =====
    int steps_per_challenge = EVOLUTION_STEPS / length;
    if (steps_per_challenge < 1) steps_per_challenge = 1;
    
    int total_step = 0;
    for (int t = 0; t < length; t++) {
        for (int s = 0; s < steps_per_challenge; s++) {
            // Challenge value is mixed with step-dependent variation
            float modified_challenge = challenge[t] * (1.0f + 0.01f * sinf(total_step * 0.1f));
            evolve_step(&agent, secret->k, secret->gamma, modified_challenge, &seed_state, total_step);
            total_step++;
        }
    }
    
    // ===== COMPUTE RESPONSE (8 channels) =====
    float phi_avg = 0.0f;
    for (int i = 0; i < PHI_SIZE; i++) {
        phi_avg += agent.Phi[i];
    }
    phi_avg /= PHI_SIZE;
    
    // Compute entropy hash (nonlinear combination of all state)
    float entropy_hash = mix_nonlinear(agent.Psi, agent.I, agent.R);
    entropy_hash += mix_nonlinear(agent.Lx, agent.Ly, agent.Lz);
    entropy_hash += mix_nonlinear(phi_avg, agent.entropy, secret->k);
    entropy_hash = fast_tanh(entropy_hash);
    
    AuthResponse resp;
    resp.psi = agent.Psi;
    resp.i_val = agent.I;
    resp.r_val = agent.R;
    resp.phi_avg = phi_avg;
    resp.lorenz_x = agent.Lx;
    resp.lorenz_y = agent.Ly;
    resp.lorenz_z = agent.Lz;
    resp.entropy_hash = entropy_hash + agent.entropy * 0.001f;
    
    return resp;
}

// Verify response - ALL 8 CHANNELS must match
int auth_verify(const AuthResponse* received, const AuthResponse* expected, float tolerance) {
    return (fabsf(received->psi - expected->psi) < tolerance) &&
           (fabsf(received->i_val - expected->i_val) < tolerance) &&
           (fabsf(received->r_val - expected->r_val) < tolerance) &&
           (fabsf(received->phi_avg - expected->phi_avg) < tolerance) &&
           (fabsf(received->lorenz_x - expected->lorenz_x) < tolerance) &&
           (fabsf(received->lorenz_y - expected->lorenz_y) < tolerance) &&
           (fabsf(received->lorenz_z - expected->lorenz_z) < tolerance) &&
           (fabsf(received->entropy_hash - expected->entropy_hash) < tolerance);
}
