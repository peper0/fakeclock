FakeClock is a simple C++ library designed for simulating and manipulating time in your unit tests. It is ideal for code that does not support the injection of a time source, such as external libraries or legacy code.

## Features

- Decouples your tests from real-time flow
- Simulates time progression
- Completely non-intrusive
- Supports time-related functions, including:
    - Sleep functions (`usleep`, `nanosleep`, `std::this_thread::sleep_for`, etc.)
    - Time getters (`gettimeofday`, `clock_gettime`, `std::chrono::system_clock::now`, etc.)
    - Timeouts in blocking functions (`select`, `poll`, `epoll_wait`)

## Why Use FakeClock?

- To make tests faster
- To make tests deterministic

## Usage

Here is a basic example of how to use FakeClock in unit tests:

```cpp
{
        // Create a MasterOfTime instance, which takes control over time
        MasterOfTime clock;

        auto start = std::chrono::system_clock::now();
        clock.advance(10s);
        auto end = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        assert(duration.count() == 10); // It's exactly 10 seconds
}
```

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contact

For any questions or suggestions, please open an issue on GitHub.
