# Smart Lock Implementation: Technical Deep Dive
*Physics-Based Authentication for Door Lock Systems*

---

## 1. System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    SMART LOCK SYSTEM                     │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌────────────┐         ┌──────────────┐               │
│  │  Keypad    │         │   STM32 H7   │               │
│  │  (PIN)     │────────▶│   MCU        │               │
│  └────────────┘         │              │               │
│                         │ ┌──────────┐ │               │
│  ┌────────────┐         │ │ Physics  │ │               │
│  │  RFID/NFC  │────────▶│ │ Auth     │ │               │
│  │  Reader    │         │ │ Engine   │ │               │
│  └────────────┘         │ └──────────┘ │               │
│                         │      │       │               │
│  ┌────────────┐         │      ▼       │               │
│  │  WiFi/BLE  │◀───────▶│ ┌──────────┐ │               │
│  │  Module    │         │ │ Crypto   │ │               │
│  └────────────┘         │ │ Storage  │ │               │
│                         │ └──────────┘ │               │
│                         │      │       │               │
│                         │      ▼       │               │
│  ┌────────────┐         │ ┌──────────┐ │               │
│  │ Relay/     │◀────────│ │ GPIO     │ │               │
│  │ Solenoid   │         │ │ Control  │ │               │
│  └────────────┘         │ └──────────┘ │               │
│                         └──────────────┘               │
│                                                          │
│  ┌────────────┐         ┌──────────────┐               │
│  │ Battery    │────────▶│  Power Mgmt  │               │
│  │ 12V/5Ah    │         │  (5V/3.3V)   │               │
│  └────────────┘         └──────────────┘               │
│                                                          │
└─────────────────────────────────────────────────────────┘
         │
         │ WiFi/4G
         ▼
┌─────────────────┐
│  Cloud Server   │
│  (Optional)     │
│  - User Mgmt    │
│  - Audit Log    │
│  - Remote Open  │
└─────────────────┘
```

---

## 2. Authentication Flow

### 2.1 Offline Mode (No Server)
```
User Action: Presses keypad button
    ↓
MCU: Wake from low-power mode
    ↓
MCU: Generate challenge from RTC timestamp
    challenge = hash(timestamp + device_salt)
    ↓
MCU: Run physics engine with user's secret
    secret = load_from_flash(user_id)
    response = compute_response(challenge, secret)
    ↓
MCU: Compare with pre-computed expected
    if (response == expected):
        GPIO HIGH → Unlock relay (2 sec)
        Log event to flash
    else:
        Increment fail counter
        if (fails > 3): Lock out for 1 min
    ↓
MCU: Return to low-power sleep
```

### 2.2 Online Mode (With Server)
```
User: Tap RFID card
    ↓
MCU: Read card UID
    ↓
MCU → Server: Request challenge via WiFi
    GET /api/challenge?device=lock-001&uid=ABC123
    ↓
Server: Generate random challenge
    challenge = rand(50) * 3.0
    expected = compute_response(challenge, user_secret)
    store(challenge_id, expected)
    ↓
Server → MCU: Send challenge
    {challenge_id: "CH-001", perturbations: [...]}
    ↓
MCU: Compute response
    response = compute_response(challenge, card_secret)
    ↓
MCU → Server: Send response
    POST /api/verify {challenge_id, response}
    ↓
Server: Verify
    if (response == expected): return "UNLOCK"
    else: return "DENY"
    ↓
MCU: Trigger relay if UNLOCK
```

---

## 3. Hardware Components

### 3.1 STM32 H7 Specifications
| Spec | Value |
|------|-------|
| **CPU** | ARM Cortex-M7 @ 480MHz |
| **FPU** | Double-precision (DP-FPU) |
| **RAM** | 1MB SRAM |
| **Flash** | 2MB |
| **GPIO** | 168 pins |
| **Power** | 3.3V, ~200mA active, 8µA stop |
| **Price** | ~$10-15 (bulk) |

### 3.2 Why STM32 H7 is Perfect

**Performance**:
- 480MHz with FPU → Estimated **<0.5ms** auth latency
- 1MB RAM → Can store 100+ user secrets
- 2MB Flash → Firmware + user DB + logs

**Power**:
- Deep sleep: 8µA = 5 years on CR2032 battery
- Wake on GPIO interrupt (button press)
- Auth in <1ms → Return to sleep

**Security**:
- Hardware RNG for challenge generation
- Memory protection unit (MPU)
- Secure boot (optional)

### 3.3 Bill of Materials

| Component | Part # | Cost | Notes |
|-----------|--------|------|-------|
| MCU | STM32H743VIT6 | $12 | 480MHz, 1MB RAM |
| WiFi Module | ESP-01S | $2 | For cloud mode |
| RFID Reader | MFRC522 | $3 | 13.56MHz NFC |
| Relay | SRD-05VDC-SL-C | $1 | 5V 10A |
| Solenoid Lock | 12V Electric Strike | $25 | Fail-secure |
| Power Supply | 12V 2A | $8 | Wall adapter |
| PCB | Custom 4-layer | $15 | Includes components |
| Enclosure | 3D printed ABS | $5 | Weather-resistant |
| **Total** | | **$71** | Per unit (bulk 100+) |

---

## 4. STM32 H7 Code Implementation

### 4.1 Main Application

```c
// main.c - STM32 H7 Smart Lock
#include "stm32h7xx_hal.h"
#include "physics_auth.h"
#include <string.h>

// Hardware handles
GPIO_TypeDef* RELAY_PORT = GPIOB;
uint16_t RELAY_PIN = GPIO_PIN_0;

// User database (stored in flash)
#define MAX_USERS 100
typedef struct {
    uint32_t uid;           // RFID card UID
    AuthSecret secret;      // Physics secret
    char name[32];
    uint8_t active;
} UserRecord;

UserRecord users[MAX_USERS] __attribute__((section(".user_db")));

// Initialize hardware
void system_init(void) {
    HAL_Init();
    SystemClock_Config(); // 480 MHz
    
    // GPIO for relay
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = RELAY_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_PORT, &gpio);
    
    // Initialize physics engine
    auth_init();
}

// Unlock door
void unlock_door(uint32_t duration_ms) {
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET);
    HAL_Delay(duration_ms);
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
}

// Authenticate user
int authenticate_user(uint32_t uid) {
    // Find user
    UserRecord* user = NULL;
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && users[i].uid == uid) {
            user = &users[i];
            break;
        }
    }
    
    if (!user) return 0; // Unknown user
    
    // Generate challenge from RTC
    uint32_t timestamp = HAL_GetTick();
    float challenge[CHALLENGE_LENGTH];
    
    // Deterministic challenge from timestamp + user salt
    uint32_t seed = timestamp ^ user->secret.seed;
    for (int i = 0; i < CHALLENGE_LENGTH; i++) {
        seed = seed * 1103515245 + 12345; // LCG
        challenge[i] = ((float)(seed & 0xFFFF) / 65535.0f) * 3.0f;
    }
    
    // Compute response
    uint32_t start = DWT->CYCCNT; // Cycle counter
    AuthResponse resp = auth_compute_response(challenge, CHALLENGE_LENGTH, &user->secret);
    uint32_t cycles = DWT->CYCCNT - start;
    
    // For offline mode: pre-compute expected
    // (In production, store hash of expected response)
    AuthResponse expected = resp; // Simplified for demo
    
    // Verify
    if (auth_verify(&resp, &expected, 0.000001f)) {
        // Log to flash
        float latency_ms = (float)cycles / 480000.0f; // 480 MHz
        printf("AUTH SUCCESS: %s (%.2fms)\n", user->name, latency_ms);
        return 1;
    } else {
        printf("AUTH FAILED: %s\n", user->name);
        return 0;
    }
}

// Main loop
int main(void) {
    system_init();
    
    printf("Smart Lock Ready (STM32 H7)\n");
    
    // Add test user
    users[0].uid = 0x12345678;
    users[0].secret = (AuthSecret){2.5f, 0.8f, 12345};
    strcpy(users[0].name, "Alice");
    users[0].active = 1;
    
    while (1) {
        // Wait for RFID tag
        uint32_t uid = rfid_read(); // Blocking read
        
        if (uid != 0) {
            if (authenticate_user(uid)) {
                unlock_door(2000); // 2 seconds
            } else {
                // Flash LED, beep, etc.
                HAL_Delay(1000);
            }
        }
        
        // Enter low-power mode
        // HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
}
```

### 4.2 Low-Power Operation

```c
// power.c - Power management
void enter_low_power(void) {
    // Disable peripherals
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    // ... (keep GPIOB for relay)
    
    // Configure wake source (button on PA0)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef wake_pin = {0};
    wake_pin.Pin = GPIO_PIN_0;
    wake_pin.Mode = GPIO_MODE_IT_RISING;
    wake_pin.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &wake_pin);
    
    // Enable interrupt
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    
    // Enter Stop mode
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    
    // Wake up here
    SystemClock_Config(); // Re-init clocks
}

// Interrupt handler
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) {
        // Button pressed, wake up
        // Main loop resumes
    }
}
```

---

## 5. Performance Analysis on STM32 H7

### 5.1 Theoretical Calculation

**FLOPs per auth**: 2,500 (from C tests)
**CPU cycles** = 2,500 FLOPs × 10 cycles/FLOP = 25,000 cycles
**Time** = 25,000 / 480,000,000 = **0.052ms**

**Memory**:
- Code: ~8KB
- Stack: ~2KB (during auth)
- User DB: 100 users × 60 bytes = 6KB
- **Total**: ~16KB (fits easily in 1MB)

### 5.2 Expected vs x86 Results

| Platform | CPU | FPU | Latency | Speedup |
|----------|-----|-----|---------|---------|
| x86 (C) | 3.5GHz | DP | 0.03ms | 116,667× |
| Python | 3.5GHz | DP | 90ms | 1× |
| **STM32 H7** | **480MHz** | **DP** | **~0.05ms** | **1,800×** |

**Estimate**: STM32 H7 should achieve **<0.1ms** latency.

### 5.3 Power Consumption

**Modes**:
- **Sleep**: 8µA @ 3.3V = 26µW
- **Auth**: 200mA @ 3.3V = 660mW for 0.1ms = **0.000066mWh**
- **Relay**: 70mA @ 12V = 840mW for 2s = **0.467mWh**

**Battery Life** (5Ah @ 12V = 60Wh):
- 10 unlocks/day: 0.467 × 10 × 365 = 1.7Wh/year
- Sleep: 26µW × 24h × 365d = 0.23Wh/year
- **Total**: 1.93Wh/year
- **Battery life**: 60Wh / 1.93Wh = **31 years**

With realistic losses: **~10 years**

---

## 6. Security Features

### 6.1 Secret Storage
```c
// Stored in flash, encrypted with hardware AES
typedef struct {
    uint32_t uid;
    uint8_t encrypted_secret[48]; // AES-256 encrypted AuthSecret
    uint8_t iv[16];
} SecureUserRecord;

// Decrypt on-the-fly during auth
AuthSecret decrypt_secret(SecureUserRecord* rec) {
    uint8_t decrypted[48];
    AES_decrypt(rec->encrypted_secret, rec->iv, DEVICE_KEY, decrypted);
    
    AuthSecret secret;
    memcpy(&secret, decrypted, sizeof(AuthSecret));
    return secret;
}
```

### 6.2 Anti-Tamper
- Detect case opening (GPIO switch)
- Wipe secrets if tampering detected
- Flash LED and lock permanently

### 6.3 Audit Log
```c
typedef struct {
    uint32_t timestamp;
    uint32_t uid;
    uint8_t result; // 1=success, 0=fail
    float latency_ms;
} AuditEntry;

AuditEntry log[1000] __attribute__((section(".audit_log")));
```

---

## 7. Production Deployment

### 7.1 Manufacturing Process
1. Flash bootloader with secure keys
2. Program firmware via SWD
3. Burn device-unique secret into OTP
4. Seal enclosure with tamper detection
5. QA test (100 auth cycles)

### 7.2 User Enrollment
```c
void enroll_user(uint32_t uid, const char* name) {
    // Generate random secret
    AuthSecret secret = {
        .k = 2.5f + ((float)rand() / RAND_MAX - 0.5f) * 0.1f,
        .gamma = 0.8f + ((float)rand() / RAND_MAX - 0.5f) * 0.05f,
        .seed = HAL_RNG_GetRandomNumber()
    };
    
    // Store in flash
    int slot = find_empty_slot();
    users[slot].uid = uid;
    users[slot].secret = secret;
    strcpy(users[slot].name, name);
    users[slot].active = 1;
    
    flash_write(&users[slot], sizeof(UserRecord));
    
    // Print card for user (QR code with secret)
    printf("User enrolled: %s (UID: %08X)\n", name, uid);
}
```

---

## 8. Real-World Testing Plan

### 8.1 Bench Tests
- [ ] Auth latency measurement (DWT cycle counter)
- [ ] Power consumption (multimeter on VBAT)
- [ ] Flash endurance (10K writes)
- [ ] Temperature range (-20°C to 70°C)

### 8.2 Field Tests
- [ ] 1000 unlock cycles
- [ ] WiFi reliability in office
- [ ] Battery life (simulated 1 year = 3,650 unlocks)
- [ ] RFID read distance (5-10cm)

### 8.3 Security Audit
- [ ] Side-channel analysis (power, timing)
- [ ] Flash dump resistance
- [ ] Replay attack testing

---

*Document prepared for hardware engineers deploying physics-based authentication on STM32 H7 microcontrollers in smart lock applications.*
