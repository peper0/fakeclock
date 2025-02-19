#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <thread>

#include "fakeclock.h"
using namespace std::chrono_literals;

void run_in_background(std::function<void()> f)
{
    // Create a new thread and run the function
    std::thread t(f);
    t.detach();
}

bool wait_for(std::function<bool()> condition, int retries = 10000)
{
    while (!condition() && retries > 0)
    {
        std::this_thread::yield();
        retries--;
    }
    return condition();
}

TEST(FakeClockTest, usleep)
{
    ClockController clock; // Take control of time
    static constexpr int SLEEP_DURATION_US = 1;
    std::atomic<bool> sleep_finished = false;
    run_in_background([&sleep_finished] {
        usleep(SLEEP_DURATION_US);
        sleep_finished = true;
    });
    ASSERT_FALSE(wait_for([&] -> bool { return sleep_finished; }));

    clock.advance(std::chrono::microseconds(SLEEP_DURATION_US));
    ASSERT_TRUE(wait_for([&] -> bool { return sleep_finished; }));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
