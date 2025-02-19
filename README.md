FakeClock is a simple C++ library for simulating and manipulating time in your unit tests. It intercepts some time-related standard library functions and redirects them to a fake clock, which is fully controlled by the programmer and detached from real time. This allows you to test time-related code in a fast and deterministic manner.

## Features

- Simulate time progression
- Control time speed (faster, slower, or paused)
- Set specific dates and times
- Easy integration with existing code

## Installation

To install FakeClock, clone the repository and include the header files in your project:

```sh
git clone https://github.com/yourusername/fakeclock.git
```

## Usage

Here is a basic example of how to use FakeClock in unit tests:

Ble

```cpp
#include "fakeclock.h"
#include <iostream>
#include <ctime>
#include <cassert>
#include <chrono>

void test_time_travel() {
    // Create a FakeClock instance, it takes control over
    FakeClock clock;

    // Advance time by 1 hour
    clock.advance(3600); // 3600 seconds = 1 hour

    // Get the current time using standard library function
    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);

    // Check the current time
    assert(std::string(buffer) == "2023-01-01 13:00:00");

    // Restore original time functions
    clock.restore();
}

int main() {
    test_time_travel();
    std::cout << "Test passed!" << std::endl;
    return 0;
}
```

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contact

For any questions or suggestions, please open an issue on GitHub.
