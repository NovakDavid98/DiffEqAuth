# src/physics_auth.py
"""
Physics-Based Authentication Engine
Uses Universe432 dynamics for challenge-response authentication
"""
import numpy as np
import hashlib
import time
from dataclasses import dataclass
from typing import Tuple, Optional
from living_agent import LivingAgent

# Configuration
CHALLENGE_LENGTH = 50  # Number of perturbation steps (reduced for speed)
RESPONSE_PRECISION = 6  # Decimal places for response comparison
DT = 0.1


@dataclass
class AuthSecret:
    """Client's secret key (physics parameters)"""
    k: float      # Sensitivity parameter
    gamma: float  # Decay parameter
    seed: int     # Initial condition seed
    
    def to_hash(self) -> str:
        """Create hash of secret for identification (not security)"""
        data = f"{self.k:.6f}:{self.gamma:.6f}:{self.seed}"
        return hashlib.sha256(data.encode()).hexdigest()[:16]


@dataclass
class Challenge:
    """Server-generated challenge"""
    challenge_id: str
    perturbations: np.ndarray
    timestamp: float
    
    def is_expired(self, timeout_sec: float = 30.0) -> bool:
        return time.time() - self.timestamp > timeout_sec


@dataclass
class Response:
    """Client's response to challenge"""
    challenge_id: str
    psi: float
    i_val: float
    r_val: float
    phi_avg: float
    
    def to_tuple(self) -> Tuple[float, float, float, float]:
        return (
            round(self.psi, RESPONSE_PRECISION),
            round(self.i_val, RESPONSE_PRECISION),
            round(self.r_val, RESPONSE_PRECISION),
            round(self.phi_avg, RESPONSE_PRECISION)
        )


class PhysicsAuthenticator:
    """Core authentication engine using physics dynamics"""
    
    def __init__(self):
        self._challenge_counter = 0
    
    def generate_challenge(self, difficulty: int = CHALLENGE_LENGTH) -> Challenge:
        """Generate a random challenge (server-side)"""
        self._challenge_counter += 1
        challenge_id = f"CH-{self._challenge_counter:08d}-{int(time.time())}"
        
        # Random perturbation sequence
        np.random.seed(int(time.time() * 1000) % 2**31)
        perturbations = np.random.rand(difficulty) * 3.0
        
        return Challenge(
            challenge_id=challenge_id,
            perturbations=perturbations,
            timestamp=time.time()
        )
    
    def compute_response(self, challenge: Challenge, secret: AuthSecret) -> Response:
        """Compute response using secret (client-side)"""
        # Initialize agent with secret parameters
        np.random.seed(secret.seed)
        agent = LivingAgent(DT)
        agent.params_14.k_I_phi = secret.k
        agent.params_14.gamma_psi = secret.gamma
        agent.Phi = np.random.rand(agent.Phi.shape[0]) * 0.1
        agent.Psi = np.random.rand() * 0.1
        
        # Apply challenge perturbations
        for phi in challenge.perturbations:
            agent.update_environment(phi)
            agent.step()
        
        return Response(
            challenge_id=challenge.challenge_id,
            psi=agent.Psi,
            i_val=agent.I,
            r_val=agent.R,
            phi_avg=np.mean(agent.Phi)
        )
    
    def verify(self, response: Response, expected: Response) -> bool:
        """Verify response matches expected (server-side)"""
        return response.to_tuple() == expected.to_tuple()


class AuthServer:
    """Mock authentication server"""
    
    def __init__(self):
        self.authenticator = PhysicsAuthenticator()
        self.registered_devices = {}  # device_id -> AuthSecret
        self.pending_challenges = {}  # challenge_id -> (device_id, expected_response)
        self.used_challenges = set()  # Replay protection
    
    def register_device(self, device_id: str, secret: AuthSecret):
        """Register a device with its secret"""
        self.registered_devices[device_id] = secret
    
    def create_challenge(self, device_id: str) -> Optional[Challenge]:
        """Create a challenge for a device"""
        if device_id not in self.registered_devices:
            return None
        
        challenge = self.authenticator.generate_challenge()
        
        # Pre-compute expected response
        secret = self.registered_devices[device_id]
        expected = self.authenticator.compute_response(challenge, secret)
        
        self.pending_challenges[challenge.challenge_id] = (device_id, expected)
        
        return challenge
    
    def verify_response(self, device_id: str, response: Response) -> Tuple[bool, str]:
        """Verify a client's response"""
        cid = response.challenge_id
        
        # Check replay
        if cid in self.used_challenges:
            return False, "REPLAY_ATTACK_DETECTED"
        
        # Check pending
        if cid not in self.pending_challenges:
            return False, "UNKNOWN_CHALLENGE"
        
        expected_device, expected_response = self.pending_challenges[cid]
        
        # Check device match
        if expected_device != device_id:
            return False, "DEVICE_MISMATCH"
        
        # Mark as used
        self.used_challenges.add(cid)
        del self.pending_challenges[cid]
        
        # Verify physics response
        if self.authenticator.verify(response, expected_response):
            return True, "AUTH_SUCCESS"
        else:
            return False, "INVALID_RESPONSE"


class AuthClient:
    """Mock authentication client"""
    
    def __init__(self, device_id: str, secret: AuthSecret):
        self.device_id = device_id
        self.secret = secret
        self.authenticator = PhysicsAuthenticator()
    
    def respond_to_challenge(self, challenge: Challenge) -> Response:
        """Compute response to server challenge"""
        return self.authenticator.compute_response(challenge, self.secret)


# ===== TEST SUITE =====

def run_functional_tests():
    """Test basic auth functionality"""
    print("\n" + "="*60)
    print("FUNCTIONAL TESTS")
    print("="*60)
    
    server = AuthServer()
    
    # Register a device
    secret = AuthSecret(k=2.5, gamma=0.8, seed=12345)
    server.register_device("device-001", secret)
    
    # Create legitimate client
    client = AuthClient("device-001", secret)
    
    # Test 1: Correct secret should pass
    print("\n[Test 1] Correct secret...")
    challenge = server.create_challenge("device-001")
    response = client.respond_to_challenge(challenge)
    success, msg = server.verify_response("device-001", response)
    print(f"  Result: {msg} → {'✅ PASS' if success else '❌ FAIL'}")
    
    # Test 2: Wrong secret should fail
    print("\n[Test 2] Wrong secret...")
    wrong_secret = AuthSecret(k=3.0, gamma=0.5, seed=99999)
    wrong_client = AuthClient("device-001", wrong_secret)
    
    challenge2 = server.create_challenge("device-001")
    wrong_response = wrong_client.respond_to_challenge(challenge2)
    success2, msg2 = server.verify_response("device-001", wrong_response)
    print(f"  Result: {msg2} → {'✅ PASS (rejected)' if not success2 else '❌ FAIL (should reject)'}")
    
    # Test 3: Same challenge + same secret = same response
    print("\n[Test 3] Determinism...")
    auth = PhysicsAuthenticator()
    ch = auth.generate_challenge()
    r1 = auth.compute_response(ch, secret)
    r2 = auth.compute_response(ch, secret)
    deterministic = r1.to_tuple() == r2.to_tuple()
    print(f"  Result: {'Same response' if deterministic else 'Different response'} → {'✅ PASS' if deterministic else '❌ FAIL'}")
    
    return success and (not success2) and deterministic


def run_security_tests():
    """Test security properties"""
    print("\n" + "="*60)
    print("SECURITY TESTS")
    print("="*60)
    
    server = AuthServer()
    secret = AuthSecret(k=2.5, gamma=0.8, seed=12345)
    server.register_device("device-001", secret)
    client = AuthClient("device-001", secret)
    
    # Test 4: Replay attack
    print("\n[Test 4] Replay attack resistance...")
    challenge = server.create_challenge("device-001")
    response = client.respond_to_challenge(challenge)
    
    # First use
    s1, m1 = server.verify_response("device-001", response)
    # Replay
    s2, m2 = server.verify_response("device-001", response)
    
    replay_blocked = s1 and (not s2) and m2 == "REPLAY_ATTACK_DETECTED"
    print(f"  First attempt: {m1}")
    print(f"  Replay attempt: {m2}")
    print(f"  → {'✅ PASS' if replay_blocked else '❌ FAIL'}")
    
    # Test 5: Brute force resistance
    print("\n[Test 5] Brute force resistance (100 random secrets)...")
    challenge = server.create_challenge("device-001")
    correct_response = client.respond_to_challenge(challenge)
    
    # Consume the challenge properly first
    server.verify_response("device-001", correct_response)
    
    # Now try brute force on new challenge
    challenge2 = server.create_challenge("device-001")
    collisions = 0
    for i in range(100):
        fake_secret = AuthSecret(k=np.random.rand()*5, gamma=np.random.rand()*2, seed=np.random.randint(0, 100000))
        fake_client = AuthClient("device-001", fake_secret)
        fake_response = fake_client.respond_to_challenge(challenge2)
        s, m = server.verify_response("device-001", fake_response)
        if s:
            collisions += 1
            break
        # Re-add challenge for next attempt (simulating fresh challenge)
        expected = client.authenticator.compute_response(challenge2, secret)
        server.pending_challenges[challenge2.challenge_id] = ("device-001", expected)
    
    brute_force_safe = collisions == 0
    print(f"  Collisions found: {collisions}")
    print(f"  → {'✅ PASS' if brute_force_safe else '❌ FAIL'}")
    
    return replay_blocked and brute_force_safe


def run_performance_tests():
    """Test performance"""
    print("\n" + "="*60)
    print("PERFORMANCE TESTS")
    print("="*60)
    
    server = AuthServer()
    secret = AuthSecret(k=2.5, gamma=0.8, seed=12345)
    server.register_device("device-001", secret)
    client = AuthClient("device-001", secret)
    
    # Test 6: Latency
    print("\n[Test 6] Authentication latency...")
    iterations = 50
    start = time.time()
    for _ in range(iterations):
        challenge = server.create_challenge("device-001")
        response = client.respond_to_challenge(challenge)
        server.verify_response("device-001", response)
    elapsed = time.time() - start
    
    latency_ms = (elapsed / iterations) * 1000
    throughput = iterations / elapsed
    
    print(f"  Latency: {latency_ms:.2f}ms per auth")
    print(f"  Throughput: {throughput:.1f} auth/sec")
    latency_pass = latency_ms < 100
    print(f"  → {'✅ PASS' if latency_pass else '❌ FAIL'} (target: <100ms)")
    
    return latency_pass


if __name__ == "__main__":
    print("="*60)
    print("PHYSICS-BASED AUTHENTICATION PoC")
    print("="*60)
    
    func_pass = run_functional_tests()
    sec_pass = run_security_tests()
    perf_pass = run_performance_tests()
    
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    print(f"Functional Tests: {'✅ PASS' if func_pass else '❌ FAIL'}")
    print(f"Security Tests:   {'✅ PASS' if sec_pass else '❌ FAIL'}")
    print(f"Performance Tests: {'✅ PASS' if perf_pass else '❌ FAIL'}")
    print(f"\nOverall: {'✅ ALL TESTS PASSED' if all([func_pass, sec_pass, perf_pass]) else '❌ SOME TESTS FAILED'}")
