#include "test_helpers.h"
#include <atomic>
#include <chrono>
#include <fakeclock/fakeclock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std::chrono_literals;

static constexpr auto SOCKET_TIMEOUT = 1s;
static constexpr auto SOCKET_CHECK_INTERVAL = 1ms;

TEST(SocketTimeoutTest, RecvTimeout)
{
    fakeclock::MasterOfTime clock; // Take control of time
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    struct timeval tv = fakeclock::to_timeval(SOCKET_TIMEOUT);
    ASSERT_EQ(setsockopt(sv[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);

    char buf[4];
    std::atomic<ssize_t> result = 0;
    std::atomic<int> err = 0;
    assert_sleeps_for(clock, SOCKET_TIMEOUT, [&] {
        result = recv(sv[0], buf, sizeof(buf), 0);
        err = errno;
    });
    EXPECT_EQ(result, -1);
    EXPECT_EQ(err, EAGAIN);

    close(sv[0]);
    close(sv[1]);
}

TEST(SocketTimeoutTest, RecvWithoutTimeout)
{
    fakeclock::MasterOfTime clock;
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    char buf[4];
    ssize_t result = recv(sv[0], buf, sizeof(buf), 0);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(errno, EAGAIN);

    close(sv[0]);
    close(sv[1]);
}

TEST(SocketTimeoutTest, RecvDataBeforeTimeout)
{
    fakeclock::MasterOfTime clock;
    int sv[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    struct timeval tv = fakeclock::to_timeval(SOCKET_TIMEOUT);
    ASSERT_EQ(setsockopt(sv[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);

    char buf[4] = {};
    std::atomic<ssize_t> result = 0;
    std::atomic<bool> ready = false;
    std::thread receiver([&] {
        ready = true;
        result = recv(sv[0], buf, sizeof(buf), 0);
    });
    ASSERT_TRUE(wait_for([&] { return ready.load(); }));

    clock.advance(SOCKET_TIMEOUT / 2);
    ASSERT_EQ(send(sv[1], "abc", 3, 0), 3);
    clock.advance(SOCKET_CHECK_INTERVAL);
    receiver.join();
    EXPECT_EQ(result, 3);
    EXPECT_EQ(std::string(buf, result), "abc");

    close(sv[0]);
    close(sv[1]);
}

