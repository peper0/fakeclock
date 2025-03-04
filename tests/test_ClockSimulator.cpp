#include "test_helpers.h"
#include <chrono>
#include <ctime>
#include <fakeclock/ClockSimulator.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <thread>
using namespace std::chrono_literals;

static constexpr auto LONG_DURATION = 3s;

TEST(ClockSimulatorTest, now)
{
    MasterOfTime clock; // Take control of time
    auto start = ClockSimulator::getInstance().now();
    ClockSimulator::getInstance().advance(LONG_DURATION);
    auto end = ClockSimulator::getInstance().now();
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(ClockSimulatorTest, waitUntil)
{
    MasterOfTime clock; // Take control of time
    assert_sleeps_for(clock, LONG_DURATION, [] {
        ClockSimulator::getInstance().waitUntil(ClockSimulator::getInstance().now() + LONG_DURATION);
    });
}

TEST(ClockSimulatorTest, timerfd)
{
    MasterOfTime clock; // Take control of time
    int fd = ClockSimulator::getInstance().timerfdCreate();
    ClockSimulator::getInstance().timerfdSetTime(fd, ClockSimulator::getInstance().now() + LONG_DURATION);
    assert_sleeps_for(clock, LONG_DURATION, [fd] {
        uint64_t buf;
        auto res = read(fd, &buf, sizeof(buf));
        EXPECT_EQ(res, sizeof(buf));
        EXPECT_EQ(buf, 1);

    });
    ::close(fd);
}