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
    fakeclock::MasterOfTime clock; // Take control of time
    auto start = fakeclock::ClockSimulator::getInstance().now();
    fakeclock::ClockSimulator::getInstance().advance(LONG_DURATION);
    auto end = fakeclock::ClockSimulator::getInstance().now();
    auto duration = end - start;
    ASSERT_EQ(duration, LONG_DURATION);
}

TEST(ClockSimulatorTest, waitUntil)
{
    fakeclock::MasterOfTime clock; // Take control of time
    assert_sleeps_for(clock, LONG_DURATION, [] {
        fakeclock::ClockSimulator::getInstance().waitUntil(fakeclock::ClockSimulator::getInstance().now() +
                                                           LONG_DURATION);
    });
}

TEST(ClockSimulatorTest, timerfd)
{
    fakeclock::MasterOfTime clock; // Take control of time
    int fd = fakeclock::ClockSimulator::getInstance().timerfdCreate();
    fakeclock::ClockSimulator::getInstance().timerfdSetTime(fd, fakeclock::ClockSimulator::getInstance().now() +
                                                                    LONG_DURATION);
    assert_sleeps_for(clock, LONG_DURATION, [fd] {
        uint64_t buf;
        auto res = read(fd, &buf, sizeof(buf));
        EXPECT_EQ(res, sizeof(buf));
        EXPECT_EQ(buf, 1);
    });
    ::close(fd);
}