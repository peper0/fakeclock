#include "test_helpers.h"
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <time.h>
#include <chrono>
using namespace std::chrono_literals;

TEST(FakeClockSetTimeTest, Settimeofday)
{
    fakeclock::MasterOfTime clock; // Take control of time
    struct timeval tv;
    ASSERT_EQ(gettimeofday(&tv, nullptr), 0);
    struct timeval new_tv = tv;
    new_tv.tv_sec += 5;
    ASSERT_EQ(settimeofday(&new_tv, nullptr), 0);
    struct timeval tv_after;
    ASSERT_EQ(gettimeofday(&tv_after, nullptr), 0);
    EXPECT_EQ(tv_after.tv_sec, new_tv.tv_sec);
    EXPECT_EQ(tv_after.tv_usec, new_tv.tv_usec);
}

TEST(FakeClockSetTimeTest, ClockSettimeRealtime)
{
    fakeclock::MasterOfTime clock; // Take control of time
    struct timespec ts;
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &ts), 0);
    struct timespec new_ts = ts;
    new_ts.tv_sec += 7;
    ASSERT_EQ(clock_settime(CLOCK_REALTIME, &new_ts), 0);
    struct timespec ts_after;
    ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &ts_after), 0);
    EXPECT_EQ(ts_after.tv_sec, new_ts.tv_sec);
    EXPECT_EQ(ts_after.tv_nsec, new_ts.tv_nsec);
}

TEST(FakeClockSetTimeTest, TimeFunction)
{
    fakeclock::MasterOfTime clock; // Take control of time
    time_t t1 = time(nullptr);
    clock.advance(3s);
    time_t t2 = time(nullptr);
    EXPECT_EQ(t2 - t1, 3);
}

