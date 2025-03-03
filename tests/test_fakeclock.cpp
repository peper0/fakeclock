#include "test_helpers.h"
#include <chrono>
#include <ctime>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <thread>
using namespace std::chrono_literals;

static constexpr auto LONG_DURATION = 3s;

TEST(FakeClockTest, select)
{
    MasterOfTime clock; // Take control of time
    std::atomic<bool> operation_finished = false;
    std::atomic<int> select_result = -1;
    assert_sleeps_for(clock, LONG_DURATION, [&] {
        struct timeval tv = to_timeval(LONG_DURATION);
        fd_set readfds;
        FD_ZERO(&readfds);
        int fds[2] = {0, 0};
        assert(pipe(fds) == 0);
        FD_SET(fds[0], &readfds);
        // select_result = select(1, &readfds, nullptr, nullptr, &tv);
        select_result = select(fds[0] + 1, &readfds, nullptr, nullptr, &tv);

        assert(close(fds[0]) == 0);
        assert(close(fds[1]) == 0);

        operation_finished = true;
    });
    ASSERT_EQ(select_result, 0);
}
