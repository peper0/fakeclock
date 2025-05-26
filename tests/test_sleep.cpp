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
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr int SLEEP_DURATION_US = 1;
    assert_sleeps_for(clock, std::chrono::microseconds(SLEEP_DURATION_US), [] {
        int ret = usleep(SLEEP_DURATION_US);
        ASSERT_EQ(ret, 0);
    });
}

TEST(FakeClockSleepTest, sleep)
{
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr int SLEEP_DURATION_S = 1;
    assert_sleeps_for(clock, std::chrono::seconds(SLEEP_DURATION_S), [] {
        int ret = sleep(SLEEP_DURATION_S);
        ASSERT_EQ(ret, 0);
    });
}

TEST(FakeClockSleepTest, nanosleep)
{
    fakeclock::MasterOfTime clock; // Take control of time
    static constexpr int SLEEP_DURATION_NS = 1;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = SLEEP_DURATION_NS;
    assert_sleeps_for(clock, std::chrono::nanoseconds(SLEEP_DURATION_NS), [ts] {
        int ret = nanosleep(&ts, nullptr);
        ASSERT_EQ(ret, 0);
    });
}

TEST(FakeClockSleepTest, this_thread_sleep_for)
{
    fakeclock::MasterOfTime clock; // Take control of time
    assert_sleeps_for(clock, LONG_DURATION, [] { std::this_thread::sleep_for(LONG_DURATION); });
}
