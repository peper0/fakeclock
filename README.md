# FakeClock

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/yourusername/fakeclock)  
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)  
[![Version: 0.1](https://img.shields.io/badge/Version-0.1-orange.svg)](https://github.com/yourusername/fakeclock/releases)

FakeClock is a simple C++ library for simulating and manipulating time in your unit tests. It replaces select standard C++ time functions with a controllable clock—making your tests both fast and deterministic. Although primarily designed for legacy or external code that cannot easily inject a time source, FakeClock can also benefit new code that isn’t architected for dependency injection.

---

## Features

- **Time Simulation:** Programmatically advance time instead of waiting in real-time.
- **Non-Intrusive Integration:** Replaces standard C++ functions like `std::this_thread::sleep_for` and `std::chrono::system_clock::now` without needing invasive code changes.
- **Deterministic Testing:** Ensures that tests produce predictable results by decoupling them from the real system clock.
- **Lightweight and Focused:** Minimalistic design with no external dependencies.

---

## Installation

FakeClock is built as a static or dynamic library (not header-only) and currently supports Linux only.

### Building with CMake

1. **Clone the repository:**

   ```bash
   git clone https://github.com/yourusername/fakeclock.git
   cd fakeclock
   ```

2. **Create a build directory and run CMake:**

   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build .
   ```

3. **Link FakeClock into your project:**

   Add the resulting library (e.g., `libfakeclock.a` or `libfakeclock.so`) to your linker settings and include the FakeClock headers in your project.

---

## Usage Example

Below is a basic example showing how FakeClock can be used in a unit test:

```cpp
#include <cassert>
#include <chrono>
#include "fakeclock/fakeclock.h"

using namespace std::chrono_literals;

int main() {
    // Create a MasterOfTime instance to take control of the clock.
    FakeClock::MasterOfTime clock;

    // Capture the simulated current time.
    auto start = std::chrono::system_clock::now();

    // Simulate advancing time by 10 seconds.
    clock.advance(10s);

    // Capture the new simulated time.
    auto end = std::chrono::system_clock::now();

    // Validate that the time advanced exactly 10 seconds.
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    assert(duration == 10s);

    return 0;
}
```

*Note:* This example assumes you include the appropriate header files from FakeClock.

---

## Supported Platforms

- **Linux:** Current support.
- **Other Platforms:** Work is in progress. See the TODO list below for plans to extend support to Windows and macOS.

---

## Documentation & API Reference

For a detailed API reference and advanced usage scenarios, please refer to the [FakeClock Documentation](https://github.com/yourusername/fakeclock/wiki) (coming soon).

---

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/my-new-feature`).
3. Commit your changes and push your branch.
4. Submit a pull request.

Please follow our [CONTRIBUTING.md](CONTRIBUTING.md) guidelines for code style and testing.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

## Contact

For questions, suggestions, or issues, please open an issue on our [GitHub repository](https://github.com/yourusername/fakeclock/issues) or contact the maintainers via email at [your.email@example.com](mailto:your.email@example.com).

---

## Future Work / TODO List

- **Platform Extensions:**
  - [ ] Extend support to **Windows** and **macOS**.
- **Build System:**
  - [ ] Evaluate if sticking with CMake is optimal long-term or if an alternative build system would be beneficial.
- **Documentation Enhancements:**
  - [ ] Create a comprehensive API reference on the project wiki.
  - [ ] Add advanced usage examples and integration guides.
- **Testing Improvements:**
  - [ ] Increase unit test coverage, including edge cases for legacy and external code.
  - [ ] Develop integration tests for scenarios where the clock is not dependency-injected.
- **Additional Features:**
  - [ ] Explore options for an optional header-only mode (if feasible in the future).
  - [ ] Add more simulation functions (e.g., for high-resolution timers or additional C++ time functions).
- **Community & Contribution:**
  - [ ] Set up a discussion forum or GitHub Discussions page for user support and ideas.
  - [ ] Improve contribution guidelines with detailed examples.

---

By keeping this README updated and using the TODO list to guide future improvements, FakeClock can become an even more approachable and robust tool for developers looking to create deterministic, efficient tests.

If you have any additional details or questions, let me know!