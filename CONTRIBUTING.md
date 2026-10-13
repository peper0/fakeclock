# Contributing to FakeClock

Thank you for your interest in contributing to FakeClock! This document provides guidelines and instructions for contributing to this project.

## Code of Conduct

Please be respectful and considerate of others when contributing to this project. We expect all contributors to follow basic principles of open-source collaboration.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/peper0/fakeclock.git`
3. Create a new branch for your feature or bugfix: `git checkout -b your-feature-name`
4. Make your changes
5. Build and test your changes

## Development Environment Setup

To set up your development environment:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Coding Standards

When contributing code, please follow these guidelines:

- Use consistent formatting with the rest of the codebase
- Follow C++17 standards or later
- Write clear, descriptive commit messages
- Add appropriate unit tests for new functionality
- Document public APIs using comments

## Pull Request Process

1. Ensure your code builds and passes all tests
2. Update documentation if necessary
3. Submit a pull request to the main repository
4. Address any feedback from code reviews

## Testing

All new features should include unit tests. Run the test suite before submitting your PR:

```bash
cd build
ctest
```

Run the sanitizer regression tests with:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON
cmake --build build-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:detect_stack_use_after_return=1 \
    ctest --test-dir build-asan --output-on-failure
```

`ClockRuntime.*` runs startup, timer allocation, new-thread initialization,
concurrent allocation/clock reads, and repeated descriptor checks in separate
processes. CTest enforces a
20-second timeout on each process so a regression cannot hang the test runner.
These tests and the existing suite run in GitHub Actions with GCC and Clang,
with ASan both enabled and disabled, on x86-64 and ARM64.

When adding an intercepted libc function, resolve its real implementation in
the function body using the private `src/real_function.h` helper:

```cpp
const auto real_sleep = fakeclock::detail::realFunction<&sleep>(__func__);
```

The helper infers the exact function pointer type and caches the lookup per
function. There is no registry or table to update. Add the simulation behavior
and its tests alongside the existing overrides. POSIX timer definitions also
declare their glibc compatibility aliases next to the function, using
`FAKECLOCK_TIMER_ABI`.

`clock_gettime` is the bootstrap exception: it must never call this resolver,
construct the simulator, allocate memory, or wait for a writer. ASan may call it
from inside an allocation while the simulator's mutex is held. Its real-time
path uses a syscall, and its simulated path reads constant-initialized,
lock-free atomic values. A spinlock or a seqlock retry would also deadlock on
reentry from a suspended writer.

## Reporting Issues

If you find a bug or have a feature request:

1. Check if the issue already exists in the GitHub issue tracker
2. If not, create a new issue with a clear description
3. Include steps to reproduce bugs or clear descriptions of proposed features
4. Add relevant labels

## License

By contributing to FakeClock, you agree that your contributions will be licensed under the project's MIT License.
