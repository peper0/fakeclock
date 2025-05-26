#include "test_helpers.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <fakeclock/fakeclock.h>
#include <fakeclock/common.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <thread>
using namespace std::chrono_literals;

TEST(PosixTimerTest, TimerCreateBasic)
{
    fakeclock::MasterOfTime clock; // Take control of time
    
    timer_t timerid;
    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_NONE; // Don't send any signals
    
    int ret = timer_create(CLOCK_REALTIME, &sev, &timerid);
    ASSERT_EQ(ret, 0) << "timer_create failed";
    
    // Cleanup
    timer_delete(timerid);
}

TEST(PosixTimerTest, TimerSetAndGetTime)
{
    fakeclock::MasterOfTime clock; // Take control of time
    
    timer_t timerid;
    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_NONE; // Don't send any signals
    
    ASSERT_EQ(timer_create(CLOCK_REALTIME, &sev, &timerid), 0) << "timer_create failed";
    
    struct itimerspec its;
    static constexpr auto TIMER_DURATION = 2s;
    
    // Set a one-shot timer for 2 seconds
    its.it_value = fakeclock::to_timespec(TIMER_DURATION);
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    ASSERT_EQ(timer_settime(timerid, 0, &its, nullptr), 0) << "timer_settime failed";
    
    // Get the time and check it
    struct itimerspec current_its;
    ASSERT_EQ(timer_gettime(timerid, &current_its), 0) << "timer_gettime failed";
    
    // The time remaining should be close to 2 seconds
    auto remaining = fakeclock::to_duration(current_its.it_value);
    ASSERT_LE(TIMER_DURATION - remaining, 10ms) << "Timer duration mismatch";
    
    // Cleanup
    timer_delete(timerid);
}

TEST(PosixTimerTest, TimerExpiration)
{
    fakeclock::MasterOfTime clock; // Take control of time
    
    timer_t timerid;
    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_NONE; // Don't send any signals
    
    ASSERT_EQ(timer_create(CLOCK_REALTIME, &sev, &timerid), 0) << "timer_create failed";
    
    struct itimerspec its;
    static constexpr auto TIMER_DURATION = 2s;
    
    // Set a one-shot timer for 2 seconds
    its.it_value = fakeclock::to_timespec(TIMER_DURATION);
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    ASSERT_EQ(timer_settime(timerid, 0, &its, nullptr), 0) << "timer_settime failed";
    
    // Advance time by 2 seconds and check timer state
    clock.advance(TIMER_DURATION);
    
    // Get the time and check it
    struct itimerspec current_its;
    ASSERT_EQ(timer_gettime(timerid, &current_its), 0) << "timer_gettime failed";
    
    // The timer should be expired
    ASSERT_EQ(current_its.it_value.tv_sec, 0);
    ASSERT_EQ(current_its.it_value.tv_nsec, 0);
    
    // Cleanup
    timer_delete(timerid);
}

TEST(PosixTimerTest, PeriodicTimer)
{
    fakeclock::MasterOfTime clock; // Take control of time
    
    timer_t timerid;
    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_NONE; // Don't send any signals
    
    ASSERT_EQ(timer_create(CLOCK_REALTIME, &sev, &timerid), 0) << "timer_create failed";
    
    struct itimerspec its;
    static constexpr auto INITIAL_DELAY = 1s;
    static constexpr auto INTERVAL = 500ms;
    
    // Set a periodic timer with 1s initial delay and 500ms interval
    its.it_value = fakeclock::to_timespec(INITIAL_DELAY);
    its.it_interval = fakeclock::to_timespec(INTERVAL);
    
    ASSERT_EQ(timer_settime(timerid, 0, &its, nullptr), 0) << "timer_settime failed";
    
    // Advance time by 1 second (initial delay)
    clock.advance(INITIAL_DELAY);
    
    // Get the time and check it
    struct itimerspec current_its;
    ASSERT_EQ(timer_gettime(timerid, &current_its), 0) << "timer_gettime failed";
    
    // The timer should be reset to the interval value
    auto remaining = fakeclock::to_duration(current_its.it_value);
    ASSERT_LE(INTERVAL - remaining, 10ms) << "Timer interval mismatch";
    
    // Advance time by 250ms (half the interval)
    clock.advance(INTERVAL / 2);
    
    // Get the time and check it
    ASSERT_EQ(timer_gettime(timerid, &current_its), 0) << "timer_gettime failed";
    
    // The timer should have about half the interval remaining
    remaining = fakeclock::to_duration(current_its.it_value);
    ASSERT_LE(INTERVAL / 2 - remaining, 10ms) << "Timer remaining time mismatch";
    
    // Cleanup
    timer_delete(timerid);
}

TEST(PosixTimerTest, AbsoluteTimer)
{
    fakeclock::MasterOfTime clock; // Take control of time
    
    timer_t timerid;
    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_NONE; // Don't send any signals
    
    ASSERT_EQ(timer_create(CLOCK_REALTIME, &sev, &timerid), 0) << "timer_create failed";
    
    struct itimerspec its;
    static constexpr auto TIMER_OFFSET = 2s;
    
    // Get current time
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    
    // Set an absolute timer for 2 seconds from now
    its.it_value = fakeclock::to_timespec(fakeclock::to_duration(now) + TIMER_OFFSET);
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    ASSERT_EQ(timer_settime(timerid, TIMER_ABSTIME, &its, nullptr), 0) << "timer_settime failed";
    
    // Get the time and check it
    struct itimerspec current_its;
    ASSERT_EQ(timer_gettime(timerid, &current_its), 0) << "timer_gettime failed";
    
    // The time remaining should be close to 2 seconds
    auto remaining = fakeclock::to_duration(current_its.it_value);
    ASSERT_LE(TIMER_OFFSET - remaining, 10ms) << "Timer duration mismatch";
    
    // Cleanup
    timer_delete(timerid);
}