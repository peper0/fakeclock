#include "test_helpers.h"
#include <atomic>
#include <chrono>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <linux/kcmp.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <unistd.h>

using namespace std::chrono_literals;
using fakeclock::to_timespec;

static constexpr auto LONG_DURATION = 3s;

TEST(TimerFdTest, are_fds_equivalent)
{
    int fd = eventfd(0, 0);
    int fd2 = dup(fd);
    ASSERT_NE(fd2, -1);
    int fd3 = eventfd(0, 0);
    ASSERT_NE(fd3, -1);
    ASSERT_NE(fd, -1);
    ASSERT_NE(fd2, -1);
    EXPECT_TRUE(fakeclock::are_fds_equivalent(fd, fd2));
    EXPECT_FALSE(fakeclock::are_fds_equivalent(fd, fd3));
    int ret = close(fd);
    ASSERT_EQ(ret, 0);
    EXPECT_FALSE(fakeclock::are_fds_equivalent(fd, fd2));
    ret = close(fd2);
    ASSERT_EQ(ret, 0);
}

TEST(TimerFdTest, timerfd_create_and_settime)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    struct itimerspec new_value;
    new_value.it_value = fakeclock::to_timespec(LONG_DURATION);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    int res = timerfd_settime(timer_fd, 0, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    uint64_t expirations;
    assert_sleeps_for(clock, LONG_DURATION, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
    });

    EXPECT_EQ(expirations, 1);
    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

TEST(TimerFdTest, timerfd_create_and_settime_abstime)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    struct timespec now;
    int ret = clock_gettime(CLOCK_MONOTONIC, &now);
    ASSERT_EQ(ret, 0);
    struct itimerspec new_value;
    new_value.it_value = fakeclock::to_timespec(fakeclock::to_duration(now) + LONG_DURATION);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    int res = timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    uint64_t expirations;
    assert_sleeps_for(clock, LONG_DURATION, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
    });

    EXPECT_EQ(expirations, 1);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

TEST(TimerFdTest, timerfd_create_and_settime_abstime_repeated)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    struct timespec now;
    int ret = clock_gettime(CLOCK_MONOTONIC, &now);
    ASSERT_EQ(ret, 0);
    struct itimerspec new_value;
    new_value.it_value = fakeclock::to_timespec(fakeclock::to_duration(now) + LONG_DURATION);
    new_value.it_interval = fakeclock::to_timespec(1s); // Repeated expiration every 1 second

    int res = timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    uint64_t expirations;
    assert_sleeps_for(clock, LONG_DURATION, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
    });
    EXPECT_EQ(expirations, 1);

    assert_sleeps_for(clock, 1s, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
    });
    EXPECT_EQ(expirations, 1);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

TEST(TimerFdTest, timerfd_gettime)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    struct itimerspec new_value;
    new_value.it_value = fakeclock::to_timespec(LONG_DURATION);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    int res = timerfd_settime(timer_fd, 0, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    struct itimerspec curr_value;
    res = timerfd_gettime(timer_fd, &curr_value);
    ASSERT_EQ(res, 0);

    auto remaining =
        std::chrono::seconds(curr_value.it_value.tv_sec) + std::chrono::nanoseconds(curr_value.it_value.tv_nsec);
    ASSERT_EQ(remaining, LONG_DURATION);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

TEST(TimerFdTest, timerfd_repeated_multiple_reads)
{
    fakeclock::MasterOfTime clock;
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    // Repeated every 500ms
    struct itimerspec set_value
    {
    };
    set_value.it_value = to_timespec(500ms);
    set_value.it_interval = to_timespec(500ms);
    int res = timerfd_settime(timer_fd, 0, &set_value, nullptr);
    ASSERT_EQ(res, 0);

    uint64_t expirations = 0;

    // First expiration
    assert_sleeps_for(clock, 500ms, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
        EXPECT_EQ(expirations, 1ul);
    });

    // Second expiration
    expirations = 0;
    assert_sleeps_for(clock, 500ms, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
        EXPECT_EQ(expirations, 1ul);
    });

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

// 2. Disarming the timer: setting the timeout to 0 should stop the timer.
TEST(TimerFdTest, timerfd_disarm)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    struct itimerspec new_value;
    new_value.it_value = to_timespec(LONG_DURATION);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    int res = timerfd_settime(timer_fd, 0, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    // Disarm the timer
    struct itimerspec disarm_value = {};
    res = timerfd_settime(timer_fd, 0, &disarm_value, nullptr);
    ASSERT_EQ(res, 0);

    struct itimerspec curr_value;
    res = timerfd_gettime(timer_fd, &curr_value);
    ASSERT_EQ(res, 0);
    EXPECT_EQ(curr_value.it_value.tv_sec, 0);
    EXPECT_EQ(curr_value.it_value.tv_nsec, 0);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

// 3. Retrieving the old value via timerfd_settime.
TEST(TimerFdTest, timerfd_settime_old_value)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    // Set an initial timer for 2 seconds.
    struct itimerspec initial_value;
    initial_value.it_value = to_timespec(2s);
    initial_value.it_interval = to_timespec({});
    int res = timerfd_settime(timer_fd, 0, &initial_value, nullptr);
    ASSERT_EQ(res, 0);

    // Update timer to 5 seconds and get the old value.
    struct itimerspec new_value;
    new_value.it_value = to_timespec(5s);
    new_value.it_interval = to_timespec({});
    struct itimerspec old_value;
    res = timerfd_settime(timer_fd, 0, &new_value, &old_value);
    ASSERT_EQ(res, 0);

    auto remaining_old = fakeclock::to_duration(old_value.it_value);
    EXPECT_EQ(remaining_old, 2s);
    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

// 4. Accumulated expirations: ensuring that multiple expirations are counted.
TEST(TimerFdTest, timerfd_repeated_expirations_accumulate)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    // Start a periodic timer: first expiration after 1s, then every 1s.
    struct itimerspec new_value;
    new_value.it_value = to_timespec(1s);
    new_value.it_interval = to_timespec(1s);
    int res = timerfd_settime(timer_fd, 0, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    uint64_t expirations = 0;
    // Sleep for 3 seconds (skipping over some expirations), then read.
    assert_sleeps_for(clock, 3s, [&] {
        auto res = read(timer_fd, &expirations, sizeof(expirations));
        ASSERT_EQ(res, sizeof(expirations));
    });
    // Expect at least 3 expirations to have accumulated.
    EXPECT_GE(expirations, 3);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

// 5. Invalid nanosecond value: timerfd_settime should fail when tv_nsec is out of range.
TEST(TimerFdTest, timerfd_settime_invalid_nsec)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    struct itimerspec new_value;
    new_value.it_value.tv_sec = 1;
    new_value.it_value.tv_nsec = 1000000000; // Invalid: must be < 10^9
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    int res = timerfd_settime(timer_fd, 0, &new_value, nullptr);
    EXPECT_EQ(res, -1);
    EXPECT_EQ(errno, EINVAL);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}

// 6. Checking interval via timerfd_gettime for a periodic timer.
TEST(TimerFdTest, timerfd_gettime_interval)
{
    fakeclock::MasterOfTime clock; // Take control of time

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    ASSERT_NE(timer_fd, -1);

    // Set a periodic timer: start after 2s, then every 1s.
    struct itimerspec new_value;
    new_value.it_value = to_timespec(2s);
    new_value.it_interval = to_timespec(1s);
    int res = timerfd_settime(timer_fd, 0, &new_value, nullptr);
    ASSERT_EQ(res, 0);

    struct itimerspec curr_value;
    res = timerfd_gettime(timer_fd, &curr_value);
    ASSERT_EQ(res, 0);

    auto interval = fakeclock::to_duration(curr_value.it_interval);
    EXPECT_EQ(interval, 1s);

    auto remaining = fakeclock::to_duration(curr_value.it_value);
    EXPECT_EQ(remaining, 2s);

    res = close(timer_fd);
    ASSERT_EQ(res, 0);
}
