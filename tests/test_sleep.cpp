#include "test_helpers.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <thread>
using namespace std::chrono_literals;

static constexpr auto LONG_DURATION = 3s;

TEST(FakeClockSleepTest, usleep)
{
    MasterOfTime clock; // Take control of time
    static constexpr int SLEEP_DURATION_US = 1;
    assert_sleeps_for(clock, std::chrono::microseconds(SLEEP_DURATION_US), [] { usleep(SLEEP_DURATION_US); });
}

TEST(FakeClockSleepTest, sleep)
{
    MasterOfTime clock; // Take control of time
    static constexpr int SLEEP_DURATION_S = 1;
    assert_sleeps_for(clock, std::chrono::seconds(SLEEP_DURATION_S), [] { sleep(SLEEP_DURATION_S); });
}

TEST(FakeClockSleepTest, nanosleep)
{
    MasterOfTime clock; // Take control of time
    static constexpr int SLEEP_DURATION_NS = 1;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = SLEEP_DURATION_NS;
    assert_sleeps_for(clock, std::chrono::nanoseconds(SLEEP_DURATION_NS), [ts] { nanosleep(&ts, nullptr); });
}

TEST(FakeClockSleepTest, this_thread_sleep_for)
{
    MasterOfTime clock; // Take control of time
    assert_sleeps_for(clock, LONG_DURATION, [] { std::this_thread::sleep_for(LONG_DURATION); });
}
