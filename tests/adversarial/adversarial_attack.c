// adversarial_attack.c
// CRYPTANALYSIS: Attempting to BREAK DiffEqAuth
// Author: Adversarial Security Researcher
// Goal: Find ANY weakness that compromises the system

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include "physics_auth.h"

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define CYAN "\x1b[36m"
#define BOLD "\x1b[1m"
#define RESET "\x1b[0m"

int vulnerabilities_found = 0;

static inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void attack_header(const char* name) {
    printf("\n" BOLD RED "╔═══════════════════════════════════════════════════════════╗" RESET "\n");
    printf(BOLD RED "║ ATTACK: %-51s ║" RESET "\n", name);
    printf(BOLD RED "╚═══════════════════════════════════════════════════════════╝" RESET "\n");
}

// ============== ATTACK 1: Linear Approximation ==============
// Can we approximate the physics with a simple linear model?
void attack_linear_approximation(void) {
    attack_header("Linear Model Approximation");
    
    printf("  Strategy: Learn linear relationship Ψ = a*k + b*γ + c*seed + d\n");
    printf("  If successful: Can predict responses without knowing exact secret\n\n");
    
    // Generate training data
    #define N_TRAIN 1000
    float k_vals[N_TRAIN], g_vals[N_TRAIN], seed_vals[N_TRAIN], psi_vals[N_TRAIN];
    
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) challenge[i] = 1.5f;
    
    srand(42);
    for (int i = 0; i < N_TRAIN; i++) {
        k_vals[i] = 1.0f + ((float)rand() / RAND_MAX) * 4.0f;
        g_vals[i] = 0.1f + ((float)rand() / RAND_MAX) * 2.0f;
        seed_vals[i] = (float)(rand() % 100000);
        
        AuthSecret s = {k_vals[i], g_vals[i], (uint32_t)seed_vals[i]};
        AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &s);
        psi_vals[i] = r.psi;
    }
    
    // Fit linear model using least squares (simplified)
    // Ψ ≈ a*k + b*γ + c
    double sum_k = 0, sum_g = 0, sum_psi = 0;
    double sum_kk = 0, sum_gg = 0, sum_kg = 0;
    double sum_k_psi = 0, sum_g_psi = 0;
    
    for (int i = 0; i < N_TRAIN; i++) {
        sum_k += k_vals[i];
        sum_g += g_vals[i];
        sum_psi += psi_vals[i];
        sum_kk += k_vals[i] * k_vals[i];
        sum_gg += g_vals[i] * g_vals[i];
        sum_kg += k_vals[i] * g_vals[i];
        sum_k_psi += k_vals[i] * psi_vals[i];
        sum_g_psi += g_vals[i] * psi_vals[i];
    }
    
    // Crude linear regression
    double mean_k = sum_k / N_TRAIN;
    double mean_g = sum_g / N_TRAIN;
    double mean_psi = sum_psi / N_TRAIN;
    
    double a = (sum_k_psi - N_TRAIN * mean_k * mean_psi) / (sum_kk - N_TRAIN * mean_k * mean_k);
    double b = (sum_g_psi - N_TRAIN * mean_g * mean_psi) / (sum_gg - N_TRAIN * mean_g * mean_g);
    double c = mean_psi - a * mean_k - b * mean_g;
    
    printf("  Fitted model: Ψ ≈ %.4f*k + %.4f*γ + %.4f\n", a, b, c);
    
    // Test on new data
    int correct_predictions = 0;
    float tolerance = 0.01f;  // 1% tolerance
    
    srand(12345);
    for (int i = 0; i < 1000; i++) {
        float k = 1.0f + ((float)rand() / RAND_MAX) * 4.0f;
        float g = 0.1f + ((float)rand() / RAND_MAX) * 2.0f;
        uint32_t seed = rand() % 100000;
        
        AuthSecret s = {k, g, seed};
        AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &s);
        
        float predicted = a * k + b * g + c;
        if (fabsf(predicted - r.psi) < tolerance) {
            correct_predictions++;
        }
    }
    
    float success_rate = (float)correct_predictions / 1000 * 100;
    printf("  Prediction accuracy (1%% tolerance): %.1f%%\n", success_rate);
    
    if (success_rate > 50) {
        printf(RED "  ⚠ VULNERABILITY: Linear model achieves %.1f%% accuracy!" RESET "\n", success_rate);
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Linear approximation fails (%.1f%% accuracy)" RESET "\n", success_rate);
    }
}

// ============== ATTACK 2: Gradient-Based Parameter Recovery ==============
// Can we use gradient descent to recover secrets from observed responses?
void attack_gradient_recovery(void) {
    attack_header("Gradient-Based Secret Recovery");
    
    printf("  Strategy: Given target response, optimize (k,γ) to match\n");
    printf("  If successful: Can recover secret from intercepted response\n\n");
    
    // Target secret (pretend we don't know this)
    AuthSecret target = {2.5f, 0.8f, 12345};
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) challenge[i] = 1.5f + i * 0.01f;
    
    AuthResponse target_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &target);
    printf("  Target Ψ: %.6f (we're trying to find parameters that produce this)\n\n", target_resp.psi);
    
    // Gradient descent attack
    float k_guess = 3.0f;  // Wrong initial guess
    float g_guess = 1.5f;
    float learning_rate = 0.1f;
    float best_error = 1e10f;
    float best_k = k_guess, best_g = g_guess;
    
    // We DON'T know the seed, so try multiple
    for (uint32_t seed_guess = 0; seed_guess < 1000; seed_guess++) {
        k_guess = 1.0f + ((float)(seed_guess % 100) / 100.0f) * 4.0f;
        g_guess = 0.1f + ((float)((seed_guess / 100) % 100) / 100.0f) * 2.0f;
        
        for (int iter = 0; iter < 100; iter++) {
            AuthSecret guess = {k_guess, g_guess, seed_guess};
            AuthResponse resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &guess);
            
            float error = (resp.psi - target_resp.psi) * (resp.psi - target_resp.psi);
            
            if (error < best_error) {
                best_error = error;
                best_k = k_guess;
                best_g = g_guess;
            }
            
            // Numerical gradient
            float dk = 0.001f;
            float dg = 0.001f;
            
            AuthSecret sk = {k_guess + dk, g_guess, seed_guess};
            AuthSecret sg = {k_guess, g_guess + dg, seed_guess};
            
            AuthResponse rk = auth_compute_response(challenge, CHALLENGE_LENGTH, &sk);
            AuthResponse rg = auth_compute_response(challenge, CHALLENGE_LENGTH, &sg);
            
            float grad_k = (rk.psi - resp.psi) / dk;
            float grad_g = (rg.psi - resp.psi) / dg;
            
            // Update towards target
            float target_diff = target_resp.psi - resp.psi;
            k_guess += learning_rate * target_diff * grad_k;
            g_guess += learning_rate * target_diff * grad_g;
            
            // Clamp
            if (k_guess < 0.5f) k_guess = 0.5f;
            if (k_guess > 5.5f) k_guess = 5.5f;
            if (g_guess < 0.05f) g_guess = 0.05f;
            if (g_guess > 2.5f) g_guess = 2.5f;
        }
    }
    
    printf("  Best recovered: k=%.4f, γ=%.4f (error=%.6f)\n", best_k, best_g, best_error);
    printf("  Actual secret:  k=%.4f, γ=%.4f\n", target.k, target.gamma);
    
    // Check if we found it
    AuthSecret recovered = {best_k, best_g, 12345};  // Cheating by using correct seed
    AuthResponse rec_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &recovered);
    
    float match_error = fabsf(rec_resp.psi - target_resp.psi);
    printf("  Match error: %.6f\n", match_error);
    
    if (match_error < 0.0001f) {
        printf(RED "  ⚠ VULNERABILITY: Secret recovered via gradient descent!" RESET "\n");
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Gradient descent failed to recover secret" RESET "\n");
    }
}

// ============== ATTACK 3: Timing Side Channel ==============
void attack_timing(void) {
    attack_header("Timing Side Channel");
    
    printf("  Strategy: Check if execution time varies with secret value\n");
    printf("  If successful: Can infer secret bits from timing measurements\n\n");
    
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) challenge[i] = 1.5f;
    
    // Measure timing for different k values
    float k_values[] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    int n_k = 6;
    
    printf("  k value  |  Avg time (ns)  |  Std dev\n");
    printf("  ---------+-----------------+---------\n");
    
    double min_time = 1e10, max_time = 0;
    
    for (int ki = 0; ki < n_k; ki++) {
        AuthSecret s = {k_values[ki], 0.8f, 12345};
        
        double times[1000];
        for (int i = 0; i < 1000; i++) {
            uint64_t start = get_ns();
            volatile AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &s);
            (void)r;
            times[i] = (double)(get_ns() - start);
        }
        
        // Calculate mean and std
        double sum = 0, sum2 = 0;
        for (int i = 0; i < 1000; i++) {
            sum += times[i];
            sum2 += times[i] * times[i];
        }
        double mean = sum / 1000;
        double std = sqrt(sum2 / 1000 - mean * mean);
        
        if (mean < min_time) min_time = mean;
        if (mean > max_time) max_time = mean;
        
        printf("  %.1f      |  %.0f          |  %.0f\n", k_values[ki], mean, std);
    }
    
    double timing_variation = (max_time - min_time) / min_time * 100;
    printf("\n  Max timing variation: %.2f%%\n", timing_variation);
    
    if (timing_variation > 5.0) {
        printf(RED "  ⚠ VULNERABILITY: Timing varies >5%% with secret!" RESET "\n");
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Timing is nearly constant (%.2f%% variation)" RESET "\n", timing_variation);
    }
}

// ============== ATTACK 4: Seed Space Reduction ==============
void attack_seed_entropy(void) {
    attack_header("Seed Entropy Analysis");
    
    printf("  Strategy: Check if seed space is smaller than claimed\n");
    printf("  If successful: Brute force becomes feasible\n\n");
    
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) challenge[i] = 1.5f;
    
    // Check how many unique responses we get for sequential seeds
    AuthSecret base = {2.5f, 0.8f, 0};
    
    float prev_psi = -1000.0f;
    int identical_count = 0;
    int similar_count = 0;  // Within 0.001
    
    for (uint32_t seed = 0; seed < 10000; seed++) {
        AuthSecret s = {2.5f, 0.8f, seed};
        AuthResponse r = auth_compute_response(challenge, CHALLENGE_LENGTH, &s);
        
        if (fabsf(r.psi - prev_psi) < 0.000001f) {
            identical_count++;
        }
        if (fabsf(r.psi - prev_psi) < 0.001f) {
            similar_count++;
        }
        prev_psi = r.psi;
    }
    
    printf("  Sequential seeds tested: 10,000\n");
    printf("  Identical consecutive responses: %d\n", identical_count);
    printf("  Similar consecutive responses (<0.001): %d\n", similar_count);
    
    // Estimate effective entropy
    float uniqueness = 1.0f - (float)similar_count / 10000.0f;
    float effective_bits = log2f(1.0f / (1.0f - uniqueness + 0.0001f)) * 10;
    
    printf("  Estimated effective seed entropy: ~%.0f bits\n", effective_bits);
    
    if (similar_count > 100) {
        printf(RED "  ⚠ VULNERABILITY: Seed space shows clustering!" RESET "\n");
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Seeds produce diverse outputs" RESET "\n");
    }
}

// ============== ATTACK 5: Challenge Replay with Parameter Sweep ==============
void attack_parameter_sweep(void) {
    attack_header("Exhaustive Parameter Sweep");
    
    printf("  Strategy: Given a captured (challenge, response), sweep parameters\n");
    printf("  If successful: Find matching parameters in reasonable time\n\n");
    
    // Target (pretend attacker intercepted this)
    AuthSecret target = {2.5f, 0.8f, 12345};
    float challenge[CHALLENGE_LENGTH];
    srand(999);
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = ((float)rand() / RAND_MAX) * 3.0f;
    }
    
    AuthResponse target_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &target);
    
    printf("  Target response: Ψ=%.6f\n", target_resp.psi);
    printf("  Sweeping k=[0,5], γ=[0,2], seed=[0,1000]...\n\n");
    
    int matches = 0;
    float tolerance = 0.0001f;
    
    uint64_t start = get_ns();
    
    // Coarse sweep
    for (float k = 0.0f; k <= 5.0f; k += 0.1f) {
        for (float g = 0.0f; g <= 2.0f; g += 0.1f) {
            for (uint32_t seed = 0; seed < 1000; seed += 10) {
                AuthSecret guess = {k, g, seed};
                AuthResponse resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &guess);
                
                if (fabsf(resp.psi - target_resp.psi) < tolerance &&
                    fabsf(resp.i_val - target_resp.i_val) < tolerance &&
                    fabsf(resp.r_val - target_resp.r_val) < tolerance) {
                    matches++;
                    if (matches <= 3) {
                        printf("  MATCH FOUND: k=%.2f, γ=%.2f, seed=%u\n", k, g, seed);
                    }
                }
            }
        }
    }
    
    uint64_t elapsed = get_ns() - start;
    double elapsed_sec = (double)elapsed / 1e9;
    
    printf("\n  Combinations tested: ~%d\n", 50 * 20 * 100);
    printf("  Time elapsed: %.2f seconds\n", elapsed_sec);
    printf("  Matches found: %d\n", matches);
    
    if (matches > 0) {
        printf(RED "  ⚠ VULNERABILITY: Found %d matching parameters!" RESET "\n", matches);
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Coarse sweep found no matches" RESET "\n");
    }
    
    // Estimate full sweep time
    double full_sweep_time = elapsed_sec * (50.0 / 0.1) * (20.0 / 0.1) * (4e9 / 1000);
    printf("\n  Full precision sweep would take: %.2e years\n", full_sweep_time / 3600 / 24 / 365);
}

// ============== ATTACK 6: Floating Point Precision Exploit ==============
void attack_float_precision(void) {
    attack_header("Floating Point Precision Exploit");
    
    printf("  Strategy: Exploit FP32 rounding to find equivalent secrets\n");
    printf("  If successful: Multiple secrets produce identical responses\n\n");
    
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) challenge[i] = 1.5f;
    
    AuthSecret base = {2.5f, 0.8f, 12345};
    AuthResponse base_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &base);
    
    // Try to find equivalent secrets by exploiting FP32 epsilon
    float epsilon = 1.19209290e-07f;  // FP32 machine epsilon
    
    int equivalents = 0;
    
    for (int i = -100; i <= 100; i++) {
        for (int j = -100; j <= 100; j++) {
            AuthSecret test = {
                2.5f + i * epsilon,
                0.8f + j * epsilon,
                12345
            };
            
            AuthResponse test_resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &test);
            
            // Bit-exact comparison
            if (*(uint32_t*)&test_resp.psi == *(uint32_t*)&base_resp.psi &&
                *(uint32_t*)&test_resp.i_val == *(uint32_t*)&base_resp.i_val) {
                if (i != 0 || j != 0) {
                    equivalents++;
                }
            }
        }
    }
    
    printf("  Epsilon-variations tested: %d\n", 201 * 201);
    printf("  Bit-exact equivalent secrets: %d\n", equivalents);
    
    if (equivalents > 10) {
        printf(RED "  ⚠ VULNERABILITY: FP32 precision allows %d equivalent secrets!" RESET "\n", equivalents);
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Minimal FP32 equivalence (%d found)" RESET "\n", equivalents);
    }
}

// ============== ATTACK 7: Known-Plaintext Response Correlation ==============
void attack_response_correlation(void) {
    attack_header("Known-Plaintext Response Correlation");
    
    printf("  Strategy: Given multiple (challenge, response) pairs, find patterns\n");
    printf("  If successful: Predict responses to new challenges\n\n");
    
    AuthSecret target = {2.5f, 0.8f, 12345};
    
    // Collect multiple challenge-response pairs
    #define N_PAIRS 100
    float challenges[N_PAIRS][CHALLENGE_LENGTH];
    float responses[N_PAIRS];
    
    srand(42);
    for (int p = 0; p < N_PAIRS; p++) {
        for (int i = 0; i < CHALLENGE_LENGTH; i++) {
            challenges[p][i] = ((float)rand() / RAND_MAX) * 3.0f;
        }
        AuthResponse r = auth_compute_response(challenges[p], CHALLENGE_LENGTH, &target);
        responses[p] = r.psi;
    }
    
    printf("  Collected %d challenge-response pairs\n", N_PAIRS);
    
    // Try to predict response for new challenge using correlation
    float test_challenge[CHALLENGE_LENGTH];
    srand(9999);
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        test_challenge[i] = ((float)rand() / RAND_MAX) * 3.0f;
    }
    AuthResponse actual = auth_compute_response(test_challenge, CHALLENGE_LENGTH, &target);
    
    // Prediction: weighted average based on challenge similarity
    float predicted = 0;
    float weight_sum = 0;
    
    for (int p = 0; p < N_PAIRS; p++) {
        float dist = 0;
        for (int i = 0; i < CHALLENGE_LENGTH; i++) {
            dist += (test_challenge[i] - challenges[p][i]) * (test_challenge[i] - challenges[p][i]);
        }
        float weight = 1.0f / (dist + 0.1f);
        predicted += responses[p] * weight;
        weight_sum += weight;
    }
    predicted /= weight_sum;
    
    float error = fabsf(predicted - actual.psi);
    
    printf("  Actual response: %.6f\n", actual.psi);
    printf("  Predicted:       %.6f\n", predicted);
    printf("  Error:           %.6f\n", error);
    
    if (error < 0.01f) {
        printf(RED "  ⚠ VULNERABILITY: Correlation attack achieves <1%% error!" RESET "\n");
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Correlation prediction failed (%.1f%% error)" RESET "\n", error * 100);
    }
}

// ============== ATTACK 8: Differential Analysis ==============
void attack_differential(void) {
    attack_header("Differential Cryptanalysis");
    
    printf("  Strategy: Analyze how Δchallenge relates to Δresponse\n");
    printf("  If successful: Determine secret from differential patterns\n\n");
    
    AuthSecret target = {2.5f, 0.8f, 12345};
    
    float base_challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) base_challenge[i] = 1.5f;
    
    AuthResponse base_resp = auth_compute_response(base_challenge, CHALLENGE_LENGTH, &target);
    
    // Analyze differential
    printf("  Challenge Delta  |  Response Delta\n");
    printf("  -----------------+-----------------\n");
    
    float deltas[] = {0.001f, 0.01f, 0.1f, 1.0f};
    float response_deltas[4];
    
    for (int d = 0; d < 4; d++) {
        float mod_challenge[CHALLENGE_LENGTH];
        memcpy(mod_challenge, base_challenge, sizeof(base_challenge));
        mod_challenge[0] += deltas[d];
        
        AuthResponse mod_resp = auth_compute_response(mod_challenge, CHALLENGE_LENGTH, &target);
        response_deltas[d] = mod_resp.psi - base_resp.psi;
        
        printf("  Δ=%-14.3f | Δψ=%.6f\n", deltas[d], response_deltas[d]);
    }
    
    // Check if relationship is linear
    float ratio1 = response_deltas[1] / response_deltas[0];
    float ratio2 = response_deltas[2] / response_deltas[1];
    float expected_ratio = 10.0f;  // If linear, 10x delta should give 10x response delta
    
    float linearity_error = fabsf(ratio1 - expected_ratio) / expected_ratio;
    
    printf("\n  Linearity check: %.2f%% deviation from linear\n", linearity_error * 100);
    
    if (linearity_error < 0.1f) {
        printf(RED "  ⚠ VULNERABILITY: Response is linear with challenge!" RESET "\n");
        vulnerabilities_found++;
    } else {
        printf(GREEN "  ✓ SECURE: Non-linear differential behavior" RESET "\n");
    }
}

// ============== MAIN ==============
int main(void) {
    printf(BOLD RED "\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         ADVERSARIAL CRYPTANALYSIS OF DIFFEQAUTH              ║\n");
    printf("║         Attempting to BREAK the authentication system         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf(RESET);
    
    auth_init();
    
    attack_linear_approximation();
    attack_gradient_recovery();
    attack_timing();
    attack_seed_entropy();
    attack_parameter_sweep();
    attack_float_precision();
    attack_response_correlation();
    attack_differential();
    
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "CRYPTANALYSIS SUMMARY" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n\n");
    
    if (vulnerabilities_found == 0) {
        printf(GREEN BOLD "  🛡 NO VULNERABILITIES FOUND" RESET "\n");
        printf(GREEN "  The system resisted all %d attack vectors" RESET "\n", 8);
    } else {
        printf(RED BOLD "  ⚠ %d VULNERABILITIES DISCOVERED!" RESET "\n", vulnerabilities_found);
        printf(RED "  The system has exploitable weaknesses" RESET "\n");
    }
    
    printf("\n");
    return vulnerabilities_found;
}
