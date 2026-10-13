#include <fakeclock/ClockSimulator.h>
#include <fakeclock/fakeclock.h>

#include <array>
#include <atomic>
#include <barrier>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/syscall.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

int64_t nanoseconds(timespec ts)
{
    return int64_t{ts.tv_sec} * 1'000'000'000 + ts.tv_nsec;
}

// Exercise interposition before main, before any MasterOfTime exists.
const bool before_main = [] {
    timespec before{}, intercepted{}, after{};
    require(syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &before) == 0, "raw clock before main");
    require(clock_gettime(CLOCK_MONOTONIC, &intercepted) == 0, "intercepted clock before main");
    require(syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &after) == 0, "raw clock after read");
    require(nanoseconds(before) <= nanoseconds(intercepted) && nanoseconds(intercepted) <= nanoseconds(after),
            "inactive clock must return real time, including during initialization");
    return true;
}();

void startup()
{
    require(before_main, "pre-main check must run");
    timespec ts{};
    errno = 0;
    require(clock_gettime(-1, &ts) == -1 && errno == EINVAL, "real clock must preserve errors");
    {
        fakeclock::MasterOfTime clock;
        const auto before = std::chrono::steady_clock::now();
        {
            fakeclock::MasterOfTime nested;
            nested.advance(7ms);
        }
        require(std::chrono::steady_clock::now() - before == 7ms, "nested clock must preserve interception");
        const auto fake_before = fakeclock::FakeClock::now();
        const timespec target{12'345'678, 987'654'321};
        require(clock_settime(CLOCK_REALTIME, &target) == 0, "set simulated realtime");
        require(clock_gettime(CLOCK_REALTIME, &ts) == 0 && nanoseconds(ts) == nanoseconds(target),
                "clock_gettime must observe the new offset");
        timeval tv{};
        require(gettimeofday(&tv, nullptr) == 0 && tv.tv_sec == target.tv_sec && tv.tv_usec == 987'654,
                "gettimeofday must observe the new offset");
        require(time(nullptr) == target.tv_sec, "time must observe the new offset");
        require(fakeclock::FakeClock::now() == fake_before, "settime must not change the fake clock");
        require(std::chrono::steady_clock::now() - before == 7ms, "settime must not change other clocks");
        clock.advance(3ms);
        require(clock_gettime(CLOCK_REALTIME, &ts) == 0 && nanoseconds(ts) == nanoseconds(target) + 3'000'000,
                "advancement must retain the new offset");
    }
    timespec before{}, after{};
    require(syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &before) == 0, "raw clock before restoration check");
    require(clock_gettime(CLOCK_MONOTONIC, &ts) == 0, "clock after restoration");
    require(syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &after) == 0, "raw clock after restoration check");
    require(nanoseconds(before) <= nanoseconds(ts) && nanoseconds(ts) <= nanoseconds(after),
            "last MasterOfTime must restore real time");
}

void timers()
{
    fakeclock::MasterOfTime clock;
    const auto before = std::chrono::steady_clock::now();
    // Refill ASan allocation size classes while the simulator holds its mutex:
    // insertion allocates map nodes; advancement opens /proc/self/fdinfo streams.
    for (int i = 0; i < 1024; ++i)
    {
        const int fd = timerfd_create(CLOCK_MONOTONIC, 0);
        require(fd >= 0, "timerfd_create");
        itimerspec expiry{};
        expiry.it_value.tv_nsec = 1'000'000;
        require(timerfd_settime(fd, 0, &expiry, nullptr) == 0, "timerfd_settime");
        clock.advance(1ms);
        uint64_t expirations = 0;
        require(read(fd, &expirations, sizeof(expirations)) == sizeof(expirations) && expirations == 1,
                "timer must expire after advancement");
        require(close(fd) == 0, "close timer");
    }
    require(std::chrono::steady_clock::now() - before == 1024ms, "published time must follow advancement");
}

void fdinfo()
{
    // Isolate descriptor checks from clock interception and simulator locking.
    // GCC ASan on ARM64 must also complete repeated checks with UAR enabled.
    const int fd = eventfd(0, 0);
    require(fd >= 0, "create descriptor for fdinfo checks");
    const int duplicate = dup(fd);
    require(duplicate >= 0, "duplicate descriptor for fdinfo checks");
    for (int i = 0; i < 1024; ++i)
    {
        require(fakeclock::are_fds_equivalent(fd, duplicate), "duplicated descriptors must remain equivalent");
    }
    require(close(fd) == 0, "close fdinfo descriptor");
    require(!fakeclock::are_fds_equivalent(fd, duplicate), "closed descriptors must not be equivalent");
    require(close(duplicate) == 0, "close fdinfo duplicate");
}

void threads()
{
    fakeclock::MasterOfTime clock;
    const auto start = std::chrono::steady_clock::now();
    // Each new thread exercises first use of intercepted functions and TLS.
    for (int i = 0; i < 32; ++i)
    {
        std::thread worker([&] {
            require(std::chrono::steady_clock::now() == start, "first clock read in a new thread");
            const int fd = timerfd_create(CLOCK_MONOTONIC, 0);
            require(fd >= 0, "first timer in a new thread");
            require(close(fd) == 0, "close thread timer");
        });
        worker.join();
    }
}

void concurrent()
{
    fakeclock::MasterOfTime clock;
    constexpr int worker_count = 4;
    constexpr int iterations = 1024;
    std::barrier start(worker_count + 1);
    std::array<std::thread, worker_count> workers;
    const auto initial = fakeclock::FakeClock::now();
    for (auto &worker : workers)
    {
        worker = std::thread([&] {
            start.arrive_and_wait();
            auto previous = std::chrono::steady_clock::now();
            for (int i = 0; i < iterations; ++i)
            {
                std::vector<char> allocation(64 + i, 'x');
                const auto current = std::chrono::steady_clock::now();
                require(current >= previous, "concurrent clock reads must be monotonic");
                require(allocation.back() == 'x', "allocation during clock reads");
                previous = current;
            }
        });
    }
    start.arrive_and_wait();
    for (int i = 0; i < iterations; ++i)
    {
        const int fd = timerfd_create(CLOCK_MONOTONIC, 0);
        require(fd >= 0, "concurrent timer allocation");
        clock.advance(1ns);
        require(close(fd) == 0, "close concurrent timer");
    }
    for (auto &worker : workers)
    {
        worker.join();
    }
    require(fakeclock::FakeClock::now() - initial == iterations * 1ns, "concurrent fake time publication");
}
} // namespace

int main(int argc, char **argv)
{
    require(argc == 2, "expected one runtime test name");
    if (std::strcmp(argv[1], "startup") == 0)
        startup();
    else if (std::strcmp(argv[1], "timers") == 0)
        timers();
    else if (std::strcmp(argv[1], "fdinfo") == 0)
        fdinfo();
    else if (std::strcmp(argv[1], "threads") == 0)
        threads();
    else if (std::strcmp(argv[1], "concurrent") == 0)
        concurrent();
    else
        require(false, "unknown runtime test");
}
