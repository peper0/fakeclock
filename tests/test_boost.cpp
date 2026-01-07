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

// Prefix namespace for MasterOfTime
using fakeclock::MasterOfTime;

TEST(FakeClockBoostTest, boost_posix_time_local_time)
{
    fakeclock::MasterOfTime clock; // Take control of time
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
    timer.async_wait([&timer_expired](const boost::system::error_code &ec) {
        ASSERT_FALSE(ec);
        timer_expired = true;
    });
    io_context.poll();
    io_context.reset();
    ASSERT_FALSE(timer_expired);
    clock.advance(STEP_DURATION);
    io_context.run_one();
    ASSERT_TRUE(timer_expired);
    auto cancelled = timer.cancel();
    ASSERT_EQ(cancelled, 0);
}

TEST(FakeClockBoostTest, asio_deadline_timer_wait)
{
    MasterOfTime clock; // Take control of time
    static constexpr auto STEP_DURATION = 2s;
    boost::asio::io_context io_context;
    boost::asio::deadline_timer timer(io_context, boost::posix_time::seconds(STEP_DURATION.count()));
    bool timer_expired = false;
    timer.async_wait([&timer_expired](const boost::system::error_code &ec) {
        ASSERT_FALSE(ec);
        timer_expired = true;
    });
    io_context.poll();
    io_context.reset();
    ASSERT_FALSE(timer_expired);
    clock.advance(STEP_DURATION);
    io_context.run_one();
    ASSERT_TRUE(timer_expired);
    auto cancelled = timer.cancel();
    ASSERT_EQ(cancelled, 0);
}

TEST(FakeClockBoostTest, asio_steady_timer_wait_multiple_times)
{
    MasterOfTime clock; // Take control of time
    boost::asio::io_context io_context;
    boost::asio::steady_timer timer1(io_context);
    boost::asio::steady_timer timer2(io_context);
    boost::asio::steady_timer timer3(io_context);
    timer1.expires_from_now(2s);
    timer2.expires_at(std::chrono::steady_clock::now() + 5s);

    bool timer1_expired = false;
    bool timer2_expired = false;
    bool timer3_expired = false;
    // t=0s
    timer1.async_wait([&timer1_expired](const boost::system::error_code &ec) {
        ASSERT_FALSE(ec);
        timer1_expired = true;
    });
    timer2.async_wait([&timer2_expired](const boost::system::error_code &ec) {
        ASSERT_FALSE(ec);
        timer2_expired = true;
    });
    clock.advance(1s); // t=1s
    io_context.poll();
    io_context.reset();
    ASSERT_FALSE(timer1_expired);
    ASSERT_FALSE(timer2_expired);

    clock.advance(1s); // t=2s
    io_context.poll();
    io_context.reset();
    ASSERT_TRUE(timer1_expired);
    ASSERT_FALSE(timer2_expired);

    timer3.expires_from_now(2s); // expires at t=4s
    timer3.async_wait([&timer3_expired](const boost::system::error_code &ec) {
        ASSERT_FALSE(ec);
        timer3_expired = true;
    });

    clock.advance(1s); // t=3s
    io_context.poll();
    io_context.reset();
    ASSERT_FALSE(timer2_expired);
    ASSERT_FALSE(timer3_expired);

    clock.advance(1s); // t=4s
    io_context.poll();
    io_context.reset();
    ASSERT_FALSE(timer2_expired);
    ASSERT_TRUE(timer3_expired);

    clock.advance(1s); // t=5s
    io_context.poll();
    io_context.reset();
    ASSERT_TRUE(timer2_expired);
}

TEST(FakeClockBoostTest, asio_steady_timer_wait_in_background)
{
    MasterOfTime clock; // Take control of time
    static constexpr auto STEP_DURATION = 1ms;
    boost::asio::io_context io_context;
    boost::asio::steady_timer timer(io_context, STEP_DURATION);
    std::atomic<bool> timer_expired = false;
    timer.async_wait([&timer_expired](const boost::system::error_code &ec) { //
        ASSERT_FALSE(ec);
        timer_expired = true;
    });
    io_context.poll();
    EXPECT_FALSE(timer_expired);
    assert_sleeps_for(clock, STEP_DURATION, [&] { io_context.run(); });
    EXPECT_TRUE(timer_expired);
}
