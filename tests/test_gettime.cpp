#include "test_helpers.h"
#include <chrono>
#include <ctime>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <thread>
using namespace std::chrono_literals;

static constexpr auto LONG_DURATION = 3s;

TEST(FakeClockGetTest, gettimeofday)
{
    fakeclock::MasterOfTime clock; // Take control of time
    struct timeval tv;
    int ret = gettimeofday(&tv, nullptr);
    ASSERT_EQ(ret, 0);
    auto start = std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec);
    clock.advance(LONG_DURATION);
    ret = gettimeofday(&tv, nullptr);
    ASSERT_EQ(ret, 0);
    auto end = std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec);
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(FakeClockGetTest, clock_gettime)
{
    fakeclock::MasterOfTime clock; // Take control of time
    struct timespec ts;
    int ret = clock_gettime(CLOCK_REALTIME, &ts);
    ASSERT_EQ(ret, 0);
    auto start = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    clock.advance(LONG_DURATION);
    ret = clock_gettime(CLOCK_REALTIME, &ts);
    ASSERT_EQ(ret, 0);
    auto end = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(FakeClockGetTest, clock_gettime_monotonic)
{
    fakeclock::MasterOfTime clock; // Take control of time
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    ASSERT_EQ(ret, 0);
    auto start = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    clock.advance(LONG_DURATION);
    ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    ASSERT_EQ(ret, 0);
    auto end = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(FakeClockGetTest, clock_gettime_monotonic_raw)
{
    fakeclock::MasterOfTime clock; // Take control of time
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    ASSERT_EQ(ret, 0);
    auto start = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    clock.advance(LONG_DURATION);
    ret = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    ASSERT_EQ(ret, 0);
    auto end = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(FakeClockGetTest, steady_clock_now)
{
    fakeclock::MasterOfTime clock; // Take control of time
    auto start = std::chrono::steady_clock::now();
    clock.advance(LONG_DURATION);
    auto end = std::chrono::steady_clock::now();
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(FakeClockGetTest, system_clock_now)
{
    fakeclock::MasterOfTime clock; // Take control of time
    auto start = std::chrono::system_clock::now();
    clock.advance(LONG_DURATION);
    auto end = std::chrono::system_clock::now();
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(FakeClockGetTest, high_resolution_clock_now)
{
    fakeclock::MasterOfTime clock; // Take control of time
    auto start = std::chrono::high_resolution_clock::now();
    clock.advance(LONG_DURATION);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}
