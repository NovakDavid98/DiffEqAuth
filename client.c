// client.c
// Authentication client for Raspberry Pi
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include "physics_auth.h"

#define SERVER_URL "http://localhost:5000"
#define SECRET_FILE "/etc/physics_auth/secret.conf"

// Load secret from config file
AuthSecret load_secret(const char* filepath) {
    AuthSecret secret = {2.5f, 0.8f, 12345}; // Default
    
    FILE* f = fopen(filepath, "r");
    if (f) {
        fscanf(f, "k=%f\ngamma=%f\nseed=%u", &secret.k, &secret.gamma, &secret.seed);fclose(f);
    }
    
    return secret;
}

// Parse challenge from server response
int parse_challenge(const char* json, float* challenge, int* length) {
    // Simple JSON parsing (in production, use a proper JSON library)
    const char* ptr = strstr(json, "\"perturbations\":[");
    if (!ptr) return 0;
    
    ptr += 17; // Skip to array start
    *length = 0;
    
    while (*ptr && *length < CHALLENGE_LENGTH) {
        challenge[*length] = atof(ptr);
        (*length)++;
        
        ptr = strchr(ptr, ',');
        if (!ptr) break;
        ptr++;
    }
    
    return 1;
}

// HTTP GET challenge
char* get_challenge(const char* device_id) {
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    char url[256];
    snprintf(url, sizeof(url), "%s/challenge?device=%s", SERVER_URL, device_id);
    
    // In production: implement proper response handling
    // For now: placeholder
    curl_easy_cleanup(curl);
    
    // Mock response
    return strdup("{\"perturbations\":[1.2,0.8,2.1,...]}");
}

// HTTP POST response
int send_response(const char* device_id, const AuthResponse* resp) {
    CURL* curl = curl_easy_init();
    if (!curl) return 0;
    
    char json[512];
    snprintf(json, sizeof(json),
             "{\"device_id\":\"%s\",\"psi\":%.6f,\"i\":%.6f,\"r\":%.6f,\"phi\":%.6f}",
             device_id, resp->psi, resp->i_val, resp->r_val, resp->phi_avg);
    
    // In production: implement POST
    curl_easy_cleanup(curl);
    
    return 1; // Mock success
}

int main(int argc, char** argv) {
    printf("Physics Auth Client v1.0\n");
    
    // Initialize
    auth_init();
    
    // Load secret
    AuthSecret secret = load_secret(SECRET_FILE);
    printf("Loaded secret: k=%.2f, gamma=%.2f, seed=%u\n", 
           secret.k, secret.gamma, secret.seed);
    
    // Device ID (from MAC address or config)
    const char* device_id = "rpi-001";
    
    // Get challenge from server
    printf("Requesting challenge...\n");
    char* challenge_json = get_challenge(device_id);
    
    if (!challenge_json) {
        fprintf(stderr, "Failed to get challenge\n");
        return 1;
    }
    
    // Parse challenge
    float challenge[CHALLENGE_LENGTH];
    int length;
    if (!parse_challenge(challenge_json, challenge, &length)) {
        fprintf(stderr, "Failed to parse challenge\n");
        free(challenge_json);
        return 1;
    }
    free(challenge_json);
    
    printf("Received challenge with %d steps\n", length);
    
    // Compute response
    printf("Computing response...\n");
    clock_t start = clock();
    
    AuthResponse resp = auth_compute_response(challenge, length, &secret);
    
    clock_t end = clock();
    double elapsed_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("Response computed in %.2fms\n", elapsed_ms);
    printf("  Psi=%.6f, I=%.6f, R=%.6f, Phi_avg=%.6f\n",
           resp.psi, resp.i_val, resp.r_val, resp.phi_avg);
    
    // Send to server
    printf("Sending response...\n");
    if (send_response(device_id, &resp)) {
        printf("Authentication successful!\n");
        
        // In production: trigger GPIO for door unlock
        // system("gpio -g write 17 1");
        
        return 0;
    } else {
        fprintf(stderr, "Authentication failed\n");
        return 1;
    }
}
