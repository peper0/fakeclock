#include "test_helpers.h"
#include <atomic>
#include <chrono>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std::chrono_literals;

TEST(SocketTimeoutTest, RecvTimeout)
{
    fakeclock::MasterOfTime clock; // Take control of time
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    struct timeval tv = fakeclock::to_timeval(1ms);
    ASSERT_EQ(setsockopt(sv[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);

    char buf[4];
    std::atomic<ssize_t> result = 0;
    std::atomic<int> err = 0;
    assert_sleeps_for(clock, 1ms, [&] {
        result = recv(sv[0], buf, sizeof(buf), 0);
        err = errno;
    });
    EXPECT_EQ(result, -1);
    EXPECT_EQ(err, EAGAIN);

    close(sv[0]);
    close(sv[1]);
}

