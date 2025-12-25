# Contributing to PhysicsAuth

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/physicsauth.git`
3. Create a branch: `git checkout -b feature/your-feature`

## Running Tests

Before submitting a PR, run all tests:

```bash
# Python tests
cd src
python3 physics_auth.py

# C tests
cd c_implementation
make test_physics puf_test critical_validation
./test_physics
./puf_test
./critical_validation
```

All tests must pass for a PR to be accepted.

## Code Style

### Python
- Use type hints
- Follow PEP 8
- Add docstrings to functions

### C
- Use snake_case for functions
- Document all public functions
- Keep memory usage minimal (embedded target)

## What We're Looking For

### High Priority
- [ ] Real hardware PUF validation (STM32, ESP32)
- [ ] Formal security analysis
- [ ] Side-channel attack testing
- [ ] Additional microcontroller ports

### Medium Priority
- [ ] Performance optimizations
- [ ] Additional language bindings (Rust, Go)
- [ ] Web demo (WASM)

### Low Priority
- [ ] GUI tools
- [ ] Cloud integration examples

## Pull Request Process

1. Ensure all tests pass
2. Update documentation if needed
3. Add test cases for new features
4. Write clear commit messages
5. Request review

## Questions?

Open an issue with the "question" label.
