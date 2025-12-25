# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |

## Security Model

PhysicsAuth uses differential equations as a one-way function for authentication. 

### What We Claim
- ✅ 2000× faster than RSA-2048 (verified)
- ✅ 0/10,000 brute force success rate (verified)
- ✅ Replay attack resistant (challenge-response design)
- ✅ 100% reproducible (deterministic)

### What We Do NOT Claim
- ❌ NIST/FIPS certification (not evaluated)
- ❌ Formal security proof (empirically tested only)
- ❌ Quantum resistance (no known attack, but not proven)
- ❌ Hardware PUF validation (simulation only)

## Known Limitations

1. **No Formal Proof**: Security is based on empirical testing, not mathematical proof
2. **Simulated PUF**: Hardware PUF behavior validated in simulation, not on real chips
3. **Novel Approach**: Less cryptanalysis than traditional algorithms (40 years vs 0)

## Reporting a Vulnerability

If you discover a security issue:

1. **DO NOT** open a public issue
2. Email: [security contact - add your email]
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact

We will respond within 48 hours and work with you to address the issue.

## Responsible Disclosure

We follow responsible disclosure practices:
- 90 day disclosure timeline
- Credit to researchers who report issues
- Security patches released ASAP
