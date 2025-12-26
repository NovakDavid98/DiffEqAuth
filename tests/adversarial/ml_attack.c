// ml_attack.c
// Machine Learning Attack: Train a model to predict responses WITHOUT knowing the secret
// This exploits the correlation vulnerability discovered in adversarial_attack.c

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

#define N_TRAIN 500
#define N_TEST 100
#define N_FEATURES 10  // Summarized challenge features

// Simple neural network weights (trained via gradient descent)
typedef struct {
    float w1[N_FEATURES][20];  // Input -> Hidden
    float b1[20];
    float w2[20][4];           // Hidden -> Output (Ψ, I, R, Φ)  
    float b2[4];
} NeuralNet;

// Extract features from challenge
void extract_features(float* challenge, float* features) {
    // Feature 1-5: First 5 challenge values
    for (int i = 0; i < 5; i++) {
        features[i] = challenge[i];
    }
    
    // Feature 6: Mean
    float sum = 0;
    for (int i = 0; i < CHALLENGE_LENGTH; i++) sum += challenge[i];
    features[5] = sum / CHALLENGE_LENGTH;
    
    // Feature 7: Variance
    float var = 0;
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        var += (challenge[i] - features[5]) * (challenge[i] - features[5]);
    }
    features[6] = var / CHALLENGE_LENGTH;
    
    // Feature 8: Max
    float max = challenge[0];
    for (int i = 1; i < CHALLENGE_LENGTH; i++) {
        if (challenge[i] > max) max = challenge[i];
    }
    features[7] = max;
    
    // Feature 9: Min
    float min = challenge[0];
    for (int i = 1; i < CHALLENGE_LENGTH; i++) {
        if (challenge[i] < min) min = challenge[i];
    }
    features[8] = min;
    
    // Feature 10: Sum of last 5
    float last_sum = 0;
    for (int i = CHALLENGE_LENGTH - 5; i < CHALLENGE_LENGTH; i++) {
        last_sum += challenge[i];
    }
    features[9] = last_sum / 5;
}

float relu(float x) {
    return x > 0 ? x : 0;
}

void nn_forward(NeuralNet* nn, float* features, float* output) {
    float hidden[20];
    
    // Hidden layer
    for (int j = 0; j < 20; j++) {
        hidden[j] = nn->b1[j];
        for (int i = 0; i < N_FEATURES; i++) {
            hidden[j] += features[i] * nn->w1[i][j];
        }
        hidden[j] = relu(hidden[j]);
    }
    
    // Output layer
    for (int j = 0; j < 4; j++) {
        output[j] = nn->b2[j];
        for (int i = 0; i < 20; i++) {
            output[j] += hidden[i] * nn->w2[i][j];
        }
    }
}

void nn_train(NeuralNet* nn, float challenges[N_TRAIN][CHALLENGE_LENGTH], 
              float responses[N_TRAIN][4], int epochs) {
    
    float lr = 0.001f;
    
    for (int epoch = 0; epoch < epochs; epoch++) {
        float total_loss = 0;
        
        for (int sample = 0; sample < N_TRAIN; sample++) {
            float features[N_FEATURES];
            extract_features(challenges[sample], features);
            
            // Forward pass
            float hidden[20], output[4];
            float hidden_pre[20];  // Pre-activation
            
            for (int j = 0; j < 20; j++) {
                hidden_pre[j] = nn->b1[j];
                for (int i = 0; i < N_FEATURES; i++) {
                    hidden_pre[j] += features[i] * nn->w1[i][j];
                }
                hidden[j] = relu(hidden_pre[j]);
            }
            
            for (int j = 0; j < 4; j++) {
                output[j] = nn->b2[j];
                for (int i = 0; i < 20; i++) {
                    output[j] += hidden[i] * nn->w2[i][j];
                }
            }
            
            // Compute loss
            float loss = 0;
            float grad_output[4];
            for (int j = 0; j < 4; j++) {
                float diff = output[j] - responses[sample][j];
                loss += diff * diff;
                grad_output[j] = 2 * diff;
            }
            total_loss += loss;
            
            // Backprop to hidden
            float grad_hidden[20] = {0};
            for (int j = 0; j < 4; j++) {
                for (int i = 0; i < 20; i++) {
                    grad_hidden[i] += grad_output[j] * nn->w2[i][j];
                }
            }
            
            // ReLU gradient
            for (int i = 0; i < 20; i++) {
                if (hidden_pre[i] <= 0) grad_hidden[i] = 0;
            }
            
            // Update weights
            for (int j = 0; j < 4; j++) {
                for (int i = 0; i < 20; i++) {
                    nn->w2[i][j] -= lr * grad_output[j] * hidden[i];
                }
                nn->b2[j] -= lr * grad_output[j];
            }
            
            for (int j = 0; j < 20; j++) {
                for (int i = 0; i < N_FEATURES; i++) {
                    nn->w1[i][j] -= lr * grad_hidden[j] * features[i];
                }
                nn->b1[j] -= lr * grad_hidden[j];
            }
        }
        
        if ((epoch + 1) % 100 == 0) {
            printf("  Epoch %4d: Loss = %.6f\n", epoch + 1, total_loss / N_TRAIN);
        }
    }
}

int main(void) {
    printf(BOLD RED "\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           MACHINE LEARNING ATTACK ON DIFFEQAUTH              ║\n");
    printf("║     Training neural network to predict responses             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf(RESET);
    
    auth_init();
    
    // SECRET that attacker doesn't know
    AuthSecret target = {2.5f, 0.8f, 12345};
    
    printf("\n" CYAN "Phase 1: Collecting Training Data" RESET "\n");
    printf("  (Attacker observes %d challenge-response pairs)\n\n", N_TRAIN);
    
    // Collect training data
    float train_challenges[N_TRAIN][CHALLENGE_LENGTH];
    float train_responses[N_TRAIN][4];
    
    srand(42);
    for (int i = 0; i < N_TRAIN; i++) {
        for (int j = 0; j < CHALLENGE_LENGTH; j++) {
            train_challenges[i][j] = ((float)rand() / RAND_MAX) * 3.0f;
        }
        AuthResponse r = auth_compute_response(train_challenges[i], CHALLENGE_LENGTH, &target);
        train_responses[i][0] = r.psi;
        train_responses[i][1] = r.i_val;
        train_responses[i][2] = r.r_val;
        train_responses[i][3] = r.phi_avg;
    }
    
    printf(CYAN "Phase 2: Training Neural Network" RESET "\n\n");
    
    // Initialize NN with small random weights
    NeuralNet nn;
    srand(12345);
    for (int i = 0; i < N_FEATURES; i++) {
        for (int j = 0; j < 20; j++) {
            nn.w1[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }
    for (int i = 0; i < 20; i++) {
        nn.b1[i] = 0;
        for (int j = 0; j < 4; j++) {
            nn.w2[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }
    for (int j = 0; j < 4; j++) {
        nn.b2[j] = 0;
    }
    
    nn_train(&nn, train_challenges, train_responses, 500);
    
    printf("\n" CYAN "Phase 3: Testing Attack on New Challenges" RESET "\n\n");
    
    // Test on completely new challenges
    srand(9999);  // Different seed for test data
    
    int auth_bypassed = 0;
    float tolerance = 0.01f;  // 1% tolerance
    
    printf("  Testing %d new challenges...\n\n", N_TEST);
    
    float total_psi_error = 0;
    float total_i_error = 0;
    float total_r_error = 0;
    float total_phi_error = 0;
    
    for (int i = 0; i < N_TEST; i++) {
        float test_challenge[CHALLENGE_LENGTH];
        for (int j = 0; j < CHALLENGE_LENGTH; j++) {
            test_challenge[j] = ((float)rand() / RAND_MAX) * 3.0f;
        }
        
        // True response (what server expects)
        AuthResponse true_resp = auth_compute_response(test_challenge, CHALLENGE_LENGTH, &target);
        
        // Attacker's prediction
        float features[N_FEATURES];
        extract_features(test_challenge, features);
        
        float predicted[4];
        nn_forward(&nn, features, predicted);
        
        // Check if prediction is close enough to bypass
        float psi_err = fabsf(predicted[0] - true_resp.psi) / fabsf(true_resp.psi);
        float i_err = fabsf(predicted[1] - true_resp.i_val) / fabsf(true_resp.i_val);
        float r_err = fabsf(predicted[2] - true_resp.r_val) / fabsf(true_resp.r_val);
        float phi_err = fabsf(predicted[3] - true_resp.phi_avg) / fabsf(true_resp.phi_avg);
        
        total_psi_error += psi_err;
        total_i_error += i_err;
        total_r_error += r_err;
        total_phi_error += phi_err;
        
        if (psi_err < tolerance && i_err < tolerance && 
            r_err < tolerance && phi_err < tolerance) {
            auth_bypassed++;
            if (auth_bypassed <= 5) {
                printf(RED "  BYPASSED #%d:" RESET " True Ψ=%.4f, Predicted=%.4f (%.2f%% error)\n",
                       auth_bypassed, true_resp.psi, predicted[0], psi_err * 100);
            }
        }
    }
    
    printf("\n" BOLD CYAN "═══════════════════════════════════════════════════════════" RESET "\n");
    printf(BOLD "MACHINE LEARNING ATTACK RESULTS" RESET "\n");
    printf(CYAN "═══════════════════════════════════════════════════════════" RESET "\n\n");
    
    printf("  Training samples:    %d\n", N_TRAIN);
    printf("  Test samples:        %d\n", N_TEST);
    printf("  Tolerance:           %.0f%%\n\n", tolerance * 100);
    
    printf("  Average Prediction Errors:\n");
    printf("    Ψ (psi):    %.2f%%\n", total_psi_error / N_TEST * 100);
    printf("    I:          %.2f%%\n", total_i_error / N_TEST * 100);
    printf("    R:          %.2f%%\n", total_r_error / N_TEST * 100);
    printf("    Φ (phi):    %.2f%%\n", total_phi_error / N_TEST * 100);
    
    printf("\n  Authentication Bypassed: " BOLD "%d / %d (%.1f%%)" RESET "\n", 
           auth_bypassed, N_TEST, (float)auth_bypassed / N_TEST * 100);
    
    if (auth_bypassed > 0) {
        printf(RED "\n  ⚠ CRITICAL VULNERABILITY: ML attack bypassed %d authentications!" RESET "\n", auth_bypassed);
        printf(RED "  An attacker with ~500 observed challenge-response pairs can" RESET "\n");
        printf(RED "  predict responses and bypass authentication!" RESET "\n");
    } else if (total_psi_error / N_TEST < 0.05) {
        printf(YELLOW "\n  ⚠ WARNING: Prediction error <5%%, close to exploitable" RESET "\n");
    } else {
        printf(GREEN "\n  ✓ SECURE: ML attack failed to predict responses accurately" RESET "\n");
    }
    
    printf("\n");
    return auth_bypassed;
}
