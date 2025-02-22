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
    std::atomic<bool> sleep_finished = false;
    assert_sleeps_for(clock, LONG_DURATION, [&sleep_finished] {
        ClockSimulator::getInstance().waitUntil(ClockSimulator::getInstance().now() + LONG_DURATION);
        sleep_finished = true;
    });
}

TEST(ClockSimulatorTest, timerfd)
{
    MasterOfTime clock; // Take control of time
    int fd = ClockSimulator::getInstance().timerfdCreate();
    ClockSimulator::getInstance().timerfdSetTime(fd, ClockSimulator::getInstance().now() + LONG_DURATION);
    std::atomic<bool> sleep_finished = false;
    assert_sleeps_for(clock, LONG_DURATION, [&sleep_finished, fd] {
        uint64_t buf;
        read(fd, &buf, sizeof(buf));
        sleep_finished = true;
    });
}