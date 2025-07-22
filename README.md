# FakeClock

FakeClock is a simple C++ library for simulating and manipulating time in your unit tests. It replaces select standard C++ time functions with a controllable clock—making your tests both fast and deterministic. Although primarily designed for legacy or external code that cannot easily inject a time source, FakeClock can also benefit new code that isn't architected for dependency injection.

---

## Features

- **Time Simulation:** Programmatically advance time instead of waiting in real-time.
- **Non-Intrusive Integration:** Replaces standard C++ functions like `std::this_thread::sleep_for` and `std::chrono::system_clock::now` without needing invasive code changes.
- **Deterministic Testing:** Ensures that tests produce predictable results by decoupling them from the real system clock.
- **Lightweight and Focused:** Minimalistic design with no external dependencies.
- **Socket Timeouts:** `connect`, `recv`, and `send` respect `SO_RCVTIMEO` and `SO_SNDTIMEO` when the clock is controlled.

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
    fakeclock::MasterOfTime clock;

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

---

## Contributing

Please follow our [CONTRIBUTING.md](CONTRIBUTING.md) guidelines for code style and testing.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---
