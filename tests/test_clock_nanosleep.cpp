#include "test_helpers.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <fakeclock/fakeclock.h>
#include <fakeclock/common.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <time.h>
#include <thread>
using namespace std::chrono_literals;

TEST(FakeClockNanoSleepTest, ClockNanoSleepRelative)
{
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr auto SLEEP_DURATION = 1ms; // 1 millisecond
    
    struct timespec ts = fakeclock::to_timespec(SLEEP_DURATION);
    
    assert_sleeps_for(clock, SLEEP_DURATION, [ts] {
        int ret = clock_nanosleep(CLOCK_REALTIME, 0, &ts, nullptr);
        ASSERT_EQ(ret, 0);
    });
}

TEST(FakeClockNanoSleepTest, ClockNanoSleepAbsolute)
{
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr auto SLEEP_DURATION = 1ms; // 1 millisecond
    
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    
    auto target_duration = fakeclock::to_duration(current_time) + SLEEP_DURATION;
    struct timespec target_time = fakeclock::to_timespec(target_duration);
    
    assert_sleeps_for(clock, SLEEP_DURATION, [target_time] {
        int ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target_time, nullptr);
        ASSERT_EQ(ret, 0);
    });
}

TEST(FakeClockNanoSleepTest, ClockNanoSleepInterrupted)
{
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr auto SLEEP_DURATION = 2s;
    
    struct timespec ts = fakeclock::to_timespec(SLEEP_DURATION);
    struct timespec rem;
    
    // In simulated time, there shouldn't be interruptions, so the remainder should be untouched
    assert_sleeps_for(clock, SLEEP_DURATION, [&ts, &rem] {
        int ret = clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem);
        ASSERT_EQ(ret, 0);
    });
}

TEST(FakeClockNanoSleepTest, ClockNanoSleepMonotonicClock)
{
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr auto SLEEP_DURATION = 5ms; // 5 milliseconds
    
    struct timespec ts = fakeclock::to_timespec(SLEEP_DURATION);
    
    assert_sleeps_for(clock, SLEEP_DURATION, [ts] {
        int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
        ASSERT_EQ(ret, 0);
    });
}