// test_physics.c
// Unit tests for physics engine
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "physics_auth.h"

#define TEST_TOLERANCE 0.000001f
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RED "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

int tests_passed = 0;
int tests_failed = 0;

void assert_float_eq(float a, float b, float tolerance, const char* name) {
    if (fabsf(a - b) < tolerance) {
        printf(ANSI_GREEN "✓" ANSI_RESET " %s: %.6f ≈ %.6f\n", name, a, b);
        tests_passed++;
    } else {
        printf(ANSI_RED "✗" ANSI_RESET " %s: %.6f != %.6f (diff: %.6f)\n", 
               name, a, b, fabsf(a - b));
        tests_failed++;
    }
}

void test_determinism() {
    printf("\n=== Test 1: Determinism ===\n");
    
    AuthSecret secret = {2.5f, 0.8f, 12345};
    float challenge[CHALLENGE_LENGTH];
    
    // Generate test challenge
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = (float)i * 0.05f;
    }
    
    // Compute response twice
    AuthResponse resp1 = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
    AuthResponse resp2 = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
    
    assert_float_eq(resp1.psi, resp2.psi, TEST_TOLERANCE, "Psi determinism");
    assert_float_eq(resp1.i_val, resp2.i_val, TEST_TOLERANCE, "I determinism");
    assert_float_eq(resp1.r_val, resp2.r_val, TEST_TOLERANCE, "R determinism");
    assert_float_eq(resp1.phi_avg, resp2.phi_avg, TEST_TOLERANCE, "Phi determinism");
}

void test_uniqueness() {
    printf("\n=== Test 2: Secret Uniqueness ===\n");
    
    float challenge[CHALLENGE_LENGTH];
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = 1.0f + (float)i * 0.02f;
    }
    
    AuthSecret secret1 = {2.5f, 0.8f, 12345};
    AuthSecret secret2 = {2.6f, 0.8f, 12345}; // Different k
    AuthSecret secret3 = {2.5f, 0.9f, 12345}; // Different gamma
    AuthSecret secret4 = {2.5f, 0.8f, 54321}; // Different seed
    
    AuthResponse resp1 = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret1);
    AuthResponse resp2 = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret2);
    AuthResponse resp3 = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret3);
    AuthResponse resp4 = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret4);
    
    float diff_k = fabsf(resp1.psi - resp2.psi);
    float diff_gamma = fabsf(resp1.psi - resp3.psi);
    float diff_seed = fabsf(resp1.psi - resp4.psi);
    
    printf("  Different k: Δ Psi = %.6f %s\n", diff_k, 
           diff_k > 0.01f ? ANSI_GREEN "✓" ANSI_RESET : ANSI_RED "✗" ANSI_RESET);
    printf("  Different γ: Δ Psi = %.6f %s\n", diff_gamma,
           diff_gamma > 0.01f ? ANSI_GREEN "✓" ANSI_RESET : ANSI_RED "✗" ANSI_RESET);
    printf("  Different seed: Δ Psi = %.6f %s\n", diff_seed,
           diff_seed > 0.01f ? ANSI_GREEN "✓" ANSI_RESET : ANSI_RED "✗" ANSI_RESET);
    
    if (diff_k > 0.01f && diff_gamma > 0.01f && diff_seed > 0.01f) {
        tests_passed++;
    } else {
        tests_failed++;
    }
}

void test_verification() {
    printf("\n=== Test 3: Verification ===\n");
    
    AuthSecret secret = {2.5f, 0.8f, 12345};
    float challenge[CHALLENGE_LENGTH];
    
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = 2.0f;
    }
    
    AuthResponse correct = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
    AuthResponse wrong = correct;
    wrong.psi += 0.001f; // Slightly wrong
    
    int match_correct = auth_verify(&correct, &correct, 0.000001f);
    int match_wrong = auth_verify(&wrong, &correct, 0.000001f);
    
    printf("  Correct match: %s\n", match_correct ? 
           ANSI_GREEN "✓ PASS" ANSI_RESET : ANSI_RED "✗ FAIL" ANSI_RESET);
    printf("  Wrong rejected: %s\n", !match_wrong ? 
           ANSI_GREEN "✓ PASS" ANSI_RESET : ANSI_RED "✗ FAIL" ANSI_RESET);
    
    if (match_correct && !match_wrong) {
        tests_passed++;
    } else {
        tests_failed++;
    }
}

void test_performance() {
    printf("\n=== Test 4: Performance ===\n");
    
    AuthSecret secret = {2.5f, 0.8f, 12345};
    float challenge[CHALLENGE_LENGTH];
    
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        challenge[i] = 1.5f;
    }
    
    const int iterations = 100;
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        AuthResponse resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &secret);
        (void)resp; // Suppress unused warning
    }
    
    clock_t end = clock();
    double total_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    double avg_ms = total_ms / iterations;
    
    printf("  Average latency: %.2fms\n", avg_ms);
    printf("  Throughput: %.1f auth/sec\n", 1000.0 / avg_ms);
    printf("  Target: <100ms %s\n", avg_ms < 100.0 ? 
           ANSI_GREEN "✓ PASS" ANSI_RESET : ANSI_RED "✗ FAIL" ANSI_RESET);
    
    if (avg_ms < 100.0) {
        tests_passed++;
    } else {
        tests_failed++;
    }
}

int main() {
    printf("========================================\n");
    printf("Physics Auth C Implementation Tests\n");
    printf("========================================\n");
    
    auth_init();
    
    test_determinism();
    test_uniqueness();
    test_verification();
    test_performance();
    
    printf("\n========================================\n");
    printf("Results: %s%d passed%s, %s%d failed%s\n",
           ANSI_GREEN, tests_passed, ANSI_RESET,
           tests_failed > 0 ? ANSI_RED : "", tests_failed, ANSI_RESET);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
