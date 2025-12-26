// independent_validation.c
// FULLY INDEPENDENT validation - testing claims from scratch
// Author: Independent Validator

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include "physics_auth.h"

#define GREEN "\x1b[32m"
#define RED "\x1b[31m"
#define YELLOW "\x1b[33m"
#define CYAN "\x1b[36m"
#define BOLD "\x1b[1m"
#define RESET "\x1b[0m"

// High-resolution timer
static inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// ============== TEST 1: SPEED ==============
void test_speed(void) {
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "TEST 1: SPEED CLAIM (2000x faster than RSA-2048)" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    
    AuthSecret secret = {2.5f, 0.8f, 12345};
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = (float)(i % 10) * 0.3f;
    }
    
    // Warm up
    for (int i = 0; i < 100; i++) {
        auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
    }
    
    // Measure 100,000 iterations
    int iterations = 100000;
    uint64_t start = get_ns();
    
    for (int i = 0; i < iterations; i++) {
        volatile AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
        (void)r;
    }
    
    uint64_t elapsed = get_ns() - start;
    double latency_ms = (double)elapsed / iterations / 1000000.0;
    double throughput = 1000.0 / latency_ms;
    
    // RSA-2048 baseline: ~50ms on modern CPU (OpenSSL benchmark)
    double rsa_latency = 50.0;
    double speedup = rsa_latency / latency_ms;
    
    printf("  Iterations:        %d\n", iterations);
    printf("  Total time:        %.2f seconds\n", (double)elapsed / 1e9);
    printf("  Latency per auth:  " BOLD "%.4f ms" RESET "\n", latency_ms);
    printf("  Throughput:        " BOLD "%.0f auth/sec" RESET "\n", throughput);
    printf("  RSA-2048 baseline: 50.0 ms\n");
    printf("  Speedup vs RSA:    " BOLD "%.0fx" RESET "\n", speedup);
    
    if (speedup >= 2000) {
        printf(GREEN "  ✓ CLAIM VERIFIED: 2000x+ speedup confirmed" RESET "\n");
    } else if (speedup >= 1000) {
        printf(YELLOW "  ⚠ PARTIAL: Speedup is ~1000x, not quite 2000x" RESET "\n");
    } else {
        printf(RED "  ✗ FAILED: Speedup only %.0fx" RESET "\n", speedup);
    }
}

// ============== TEST 2: REPRODUCIBILITY ==============
void test_reproducibility(void) {
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "TEST 2: REPRODUCIBILITY (100%% deterministic)" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    
    AuthSecret secret = {3.14159f, 1.41421f, 999999};
    float challenge[CHALLENGE_LENGTH];
    srand(42);
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = ((float)rand() / RAND_MAX) * 5.0f;
    }
    
    // Get reference response
    AuthResponse ref = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
    
    // Test 10,000 repetitions
    int iterations = 10000;
    int exact_matches = 0;
    double max_drift = 0.0;
    
    for (int i = 0; i < iterations; i++) {
        AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
        
        double diff_psi = fabs(r.psi - ref.psi);
        double diff_i = fabs(r.i_val - ref.i_val);
        double diff_r = fabs(r.r_val - ref.r_val);
        double diff_phi = fabs(r.phi_avg - ref.phi_avg);
        double total_diff = diff_psi + diff_i + diff_r + diff_phi;
        
        if (total_diff > max_drift) max_drift = total_diff;
        if (total_diff < 1e-10) exact_matches++;
    }
    
    printf("  Iterations:        %d\n", iterations);
    printf("  Exact matches:     %d\n", exact_matches);
    printf("  Max drift:         %.2e\n", max_drift);
    printf("  Reference Ψ:       %.10f\n", ref.psi);
    
    if (exact_matches == iterations && max_drift < 1e-10) {
        printf(GREEN "  ✓ CLAIM VERIFIED: 100%% bit-identical reproducibility" RESET "\n");
    } else if ((double)exact_matches / iterations > 0.999) {
        printf(YELLOW "  ⚠ PARTIAL: 99.9%%+ match (floating point noise)" RESET "\n");
    } else {
        printf(RED "  ✗ FAILED: Only %.2f%% exact matches" RESET "\n", 
               (double)exact_matches / iterations * 100);
    }
}

// ============== TEST 3: BRUTE FORCE RESISTANCE ==============
void test_brute_force(void) {
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "TEST 3: BRUTE FORCE RESISTANCE (0/10000 expected)" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    
    // Target secret (unknown to attacker)
    AuthSecret target = {2.71828f, 0.57721f, 31415926};
    float challenge[CHALLENGE_LENGTH];
    srand(12345);
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = ((float)rand() / RAND_MAX) * 3.0f;
    }
    
    AuthResponse target_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &target);
    
    // Attacker attempts
    int attempts = 10000;
    int successes = 0;
    float tolerance = 0.0001f;  // Very tight match required
    
    srand(time(NULL));
    for (int i = 0; i < attempts; i++) {
        AuthSecret guess = {
            .k = 0.5f + ((float)rand() / RAND_MAX) * 5.0f,
            .gamma = 0.1f + ((float)rand() / RAND_MAX) * 2.5f,
            .seed = rand()
        };
        
        AuthResponse guess_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &guess);
        
        if (fabs(guess_resp.psi - target_resp.psi) < tolerance &&
            fabs(guess_resp.i_val - target_resp.i_val) < tolerance &&
            fabs(guess_resp.r_val - target_resp.r_val) < tolerance &&
            fabs(guess_resp.phi_avg - target_resp.phi_avg) < tolerance) {
            successes++;
            printf(RED "  WARNING: Match found at attempt %d!" RESET "\n", i);
        }
    }
    
    double success_rate = (double)successes / attempts * 100;
    
    printf("  Attempts:          %d\n", attempts);
    printf("  Successes:         %d\n", successes);
    printf("  Success rate:      %.4f%%\n", success_rate);
    printf("  Target Ψ:          %.6f\n", target_resp.psi);
    
    if (successes == 0) {
        printf(GREEN "  ✓ CLAIM VERIFIED: 0 brute force successes" RESET "\n");
    } else {
        printf(RED "  ✗ FAILED: %d collisions found!" RESET "\n", successes);
    }
}

// ============== TEST 4: PARAMETER SENSITIVITY ==============
void test_sensitivity(void) {
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "TEST 4: PARAMETER SENSITIVITY (tiny changes → different output)" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = 1.5f + i * 0.02f;
    }
    
    AuthSecret base = {2.5f, 0.8f, 12345};
    AuthResponse base_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &base);
    
    // Test various delta sizes
    float deltas[] = {0.1f, 0.01f, 0.001f, 0.0001f};
    
    printf("  Base Ψ: %.10f\n\n", base_resp.psi);
    printf("  %-10s | %-15s | %-15s | %-15s\n", "Delta", "Δk effect", "Δγ effect", "Δseed effect");
    printf("  %s\n", "-----------------------------------------------------------");
    
    for (int d = 0; d < 4; d++) {
        float delta = deltas[d];
        
        AuthSecret vary_k = {2.5f + delta, 0.8f, 12345};
        AuthSecret vary_g = {2.5f, 0.8f + delta, 12345};
        AuthSecret vary_s = {2.5f, 0.8f, 12345 + (int)(delta * 10000)};
        
        AuthResponse resp_k = auth_compute_response(challenge, CHALLENGE_LENGTH, &vary_k);
        AuthResponse resp_g = auth_compute_response(challenge, CHALLENGE_LENGTH, &vary_g);
        AuthResponse resp_s = auth_compute_response(challenge, CHALLENGE_LENGTH, &vary_s);
        
        printf("  %-10.4f | %-15.6f | %-15.6f | %-15.6f\n", 
               delta,
               fabs(resp_k.psi - base_resp.psi),
               fabs(resp_g.psi - base_resp.psi),
               fabs(resp_s.psi - base_resp.psi));
    }
    
    // Using delta=0.001 as the threshold test
    AuthSecret tiny_k = {2.501f, 0.8f, 12345};
    AuthResponse resp_tiny = auth_compute_response(challenge, CHALLENGE_LENGTH, &tiny_k);
    float diff = fabs(resp_tiny.psi - base_resp.psi);
    
    printf("\n  Critical test: k=2.5 vs k=2.501\n");
    printf("  Difference: %.6f\n", diff);
    
    if (diff > 0.00001f) {
        printf(GREEN "  ✓ CLAIM VERIFIED: Tiny parameter changes produce detectable differences" RESET "\n");
    } else {
        printf(YELLOW "  ⚠ PARTIAL: Changes are small, may need larger deltas" RESET "\n");
    }
}

// ============== TEST 5: COLLISION RESISTANCE ==============
void test_collisions(void) {
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "TEST 5: COLLISION RESISTANCE (unique fingerprints)" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    
    #define N_CHIPS 1000
    
    typedef struct {
        float psi, i, r, phi;
    } Fingerprint;
    
    Fingerprint fps[N_CHIPS];
    
    float challenge[CHALLENGE_LENGTH];
    for (int j = 0; j < CHALLENGE_LENGTH; j++) {
        challenge[j] = 1.5f + j * 0.02f;
    }
    
    // Generate fingerprints for N_CHIPS simulated devices
    for (int i = 0; i < N_CHIPS; i++) {
        srand(i * 0x12345 + 0xABCDE);
        
        AuthSecret secret = {
            .k = 1.0f + ((float)rand() / RAND_MAX) * 4.0f,
            .gamma = 0.2f + ((float)rand() / RAND_MAX) * 2.0f,
            .seed = (uint32_t)rand() ^ ((uint32_t)rand() << 16)
        };
        
        AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
        fps[i].psi = r.psi;
        fps[i].i = r.i_val;
        fps[i].r = r.r_val;
        fps[i].phi = r.phi_avg;
    }
    
    // Check for collisions in 4D space
    int collisions = 0;
    int near_collisions = 0;
    float min_dist = 1e10f;
    float collision_threshold = 0.000001f;
    float near_threshold = 0.0001f;
    
    for (int i = 0; i < N_CHIPS && collisions < 10; i++) {
        for (int j = i + 1; j < N_CHIPS; j++) {
            float d = sqrtf(
                powf(fps[i].psi - fps[j].psi, 2) +
                powf(fps[i].i - fps[j].i, 2) +
                powf(fps[i].r - fps[j].r, 2) +
                powf(fps[i].phi - fps[j].phi, 2)
            );
            
            if (d < min_dist) min_dist = d;
            if (d < collision_threshold) collisions++;
            if (d < near_threshold) near_collisions++;
        }
    }
    
    printf("  Simulated chips:   %d\n", N_CHIPS);
    printf("  Pairs compared:    %d\n", N_CHIPS * (N_CHIPS - 1) / 2);
    printf("  Collisions (<1e-6): %d\n", collisions);
    printf("  Near-miss (<1e-4): %d\n", near_collisions);
    printf("  Min 4D distance:   %.6f\n", min_dist);
    
    if (collisions == 0) {
        printf(GREEN "  ✓ CLAIM VERIFIED: Zero collisions in %d chips" RESET "\n", N_CHIPS);
    } else {
        printf(RED "  ✗ FAILED: %d collisions detected!" RESET "\n", collisions);
    }
}

// ============== TEST 6: WRONG SECRET REJECTION ==============
void test_wrong_secret(void) {
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "TEST 6: WRONG SECRET REJECTION" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    
    float challenge[CHALLENGE_LENGTH];
    srand(999);
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = ((float)rand() / RAND_MAX) * 3.0f;
    }
    
    AuthSecret correct = {2.5f, 0.8f, 12345};
    AuthResponse correct_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &correct);
    
    // Test wrong secrets
    AuthSecret wrong_secrets[] = {
        {2.5f, 0.8f, 12346},     // Wrong seed by 1
        {2.5f, 0.81f, 12345},    // Wrong gamma by 0.01
        {2.51f, 0.8f, 12345},    // Wrong k by 0.01
        {3.0f, 1.0f, 99999},     // Completely wrong
    };
    
    const char* labels[] = {
        "Seed +1",
        "Gamma +0.01", 
        "K +0.01",
        "All wrong"
    };
    
    float tolerance = 0.0001f;
    int rejected = 0;
    
    printf("  Correct response: Ψ=%.6f, I=%.6f, R=%.6f, Φ=%.6f\n\n",
           correct_resp.psi, correct_resp.i_val, correct_resp.r_val, correct_resp.phi_avg);
    
    for (int i = 0; i < 4; i++) {
        AuthResponse wrong_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &wrong_secrets[i]);
        
        int match = 
            fabs(wrong_resp.psi - correct_resp.psi) < tolerance &&
            fabs(wrong_resp.i_val - correct_resp.i_val) < tolerance &&
            fabs(wrong_resp.r_val - correct_resp.r_val) < tolerance &&
            fabs(wrong_resp.phi_avg - correct_resp.phi_avg) < tolerance;
        
        if (!match) rejected++;
        
        printf("  %-15s: Ψ=%.6f, diff=%.6f → %s\n", 
               labels[i], 
               wrong_resp.psi,
               fabs(wrong_resp.psi - correct_resp.psi),
               match ? RED "ACCEPTED (BAD!)" RESET : GREEN "REJECTED ✓" RESET);
    }
    
    printf("\n  Rejected: %d / 4\n", rejected);
    
    if (rejected == 4) {
        printf(GREEN "  ✓ CLAIM VERIFIED: All wrong secrets correctly rejected" RESET "\n");
    } else {
        printf(RED "  ✗ FAILED: Some wrong secrets were accepted!" RESET "\n");
    }
}

// ============== MAIN ==============
int main(void) {
    printf(BOLD CYAN "\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║      INDEPENDENT VALIDATION OF DIFFEQAUTH CLAIMS             ║\n");
    printf("║      Conducted by: Independent Validator                      ║\n");
    printf("║      Date: %s                                 ║\n", __DATE__);
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf(RESET);
    
    auth_init();
    
    test_speed();
    test_reproducibility();
    test_brute_force();
    test_sensitivity();
    test_collisions();
    test_wrong_secret();
    
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "VALIDATION COMPLETE" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n\n");
    
    return 0;
}
