#include "test_helpers.h"
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <chrono>
#include <ctime>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <thread>
using namespace std::chrono_literals;

TEST(FakeClockBoostTest, boost_posix_time_local_time)
{
    MasterOfTime clock; // Take control of time
    static constexpr auto STEP_DURATION = 1ms;
    boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
    clock.advance(STEP_DURATION);
    boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();
    auto duration = end - start;
    auto chrono_duration = std::chrono::microseconds(duration.total_microseconds());
    ASSERT_EQ(chrono_duration, STEP_DURATION);
}

TEST(FakeClockBoostTest, asio_steady_timer_wait)
{
    MasterOfTime clock; // Take control of time
    static constexpr auto STEP_DURATION = 5s;
    boost::asio::io_context io_context;
    boost::asio::steady_timer timer(io_context, STEP_DURATION);
    bool timer_expired = false;
    timer.async_wait([&timer_expired](const boost::system::error_code &ec) { //
        timer_expired = true;
    });
    io_context.poll();
    ASSERT_FALSE(timer_expired);
    clock.advance(STEP_DURATION);
    io_context.run_one();
    ASSERT_TRUE(timer_expired);
    timer.cancel();
}

TEST(FakeClockBoostTest, asio_steady_timer_wait_in_background)
{
    MasterOfTime clock; // Take control of time
    static constexpr auto STEP_DURATION = 1ms;
    boost::asio::io_context io_context;
    boost::asio::steady_timer timer(io_context, STEP_DURATION);
    std::atomic<bool> timer_expired = false;
    timer.async_wait([&timer_expired](const boost::system::error_code &ec) { //
        timer_expired = true;
    });
    io_context.poll();
    EXPECT_FALSE(timer_expired);
    assert_sleeps_for(clock, STEP_DURATION, [&] { io_context.run(); });
    EXPECT_TRUE(timer_expired);
}