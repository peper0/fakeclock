#pragma once

#include <atomic>
#include <chrono>
#include <fakeclock/common.h>
#include <fakeclock/fakeclock.h>
#include <functional>
#include <gtest/gtest.h>
#include <thread>

// inline void run_in_background(std::function<void()> f)
// {
//     // Create a new thread and run the function
//     std::thread t(f);
//     t.detach();
// }

inline bool wait_for(std::function<bool()> condition, int retries = 10000)
{
    while (!condition() && retries > 0)
    {
        std::this_thread::yield();
        retries--;
    }
    return condition();
}

inline void assert_sleeps_for(MasterOfTime &cc, FakeClock::duration duration, std::function<void()> sleep_fn)
{
    std::atomic<bool> sleep_finished = false;
    std::mutex m;

    std::thread t([&sleep_finished, &sleep_fn] {
        sleep_fn();
        sleep_finished = true;
    });
    ASSERT_FALSE(wait_for([&] -> bool { return sleep_finished; }));
    cc.advance(duration);
    ASSERT_TRUE(wait_for([&] -> bool { return sleep_finished; }));
    t.join();
}
