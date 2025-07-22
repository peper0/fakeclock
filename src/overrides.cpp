#include <atomic>
#include <condition_variable>
#include <cstring>
#include <dlfcn.h>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/common.h>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <queue>
#include <stdexcept>
#include <algorithm>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>

using TimePoint = fakeclock::ClockSimulator::TimePoint;
using Duration = fakeclock::ClockSimulator::Duration;
using fakeclock::FakeClock;
using fakeclock::to_duration;

extern "C"
{
    unsigned int sleep(unsigned int seconds)
    {
        static const auto real_sleep = (decltype(&sleep))dlsym(RTLD_NEXT, "sleep");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_sleep(seconds);
        }
        else
        {
            auto now = simulator.now();
            simulator.waitUntil(now + std::chrono::seconds(seconds));
            return 0;
        }
    }

    int usleep(useconds_t usec)
    {
        static const auto real_usleep = (decltype(&usleep))dlsym(RTLD_NEXT, "usleep");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_usleep(usec);
        }
        else
        {
            auto now = simulator.now();
            simulator.waitUntil(now + std::chrono::microseconds(usec));
            return 0;
        }
    }

    int nanosleep(const struct timespec *req, struct timespec *rem)
    {
        static const auto real_nanosleep = (decltype(&nanosleep))dlsym(RTLD_NEXT, "nanosleep");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_nanosleep(req, rem);
        }
        else
        {
            auto duration = std::chrono::seconds(req->tv_sec) + std::chrono::nanoseconds(req->tv_nsec);
            auto now = simulator.now();
            simulator.waitUntil(now + duration);
            return 0;
        }
    }

    int gettimeofday(struct timeval *tv, void *tz)
    {
        static const auto real_gettimeofday = (decltype(&gettimeofday))dlsym(RTLD_NEXT, "gettimeofday");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_gettimeofday(tv, tz);
        }
        else
        {
            auto duration = simulator.now().time_since_epoch();
            tv->tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            tv->tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;
            return 0;
        }
    }

    int clock_gettime(clockid_t clk_id, struct timespec *ts) noexcept
    {
        static const auto real_clock_gettime = (decltype(&clock_gettime))dlsym(RTLD_NEXT, "clock_gettime");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_clock_gettime(clk_id, ts);
        }
        else
        {
            auto duration = simulator.now().time_since_epoch();
            ts->tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            ts->tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;
            return 0;
        }
    }

    int poll(struct pollfd *fds, nfds_t nfds, int timeout)
    {
        static const auto real_poll = (decltype(&poll))dlsym(RTLD_NEXT, "poll");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting() || timeout <= 0)
        {
            return real_poll(fds, nfds, timeout);
        }
        else
        {
            auto now = simulator.now();
            int fd = simulator.timerfdCreate();
            simulator.timerfdSetTime(fd, now + std::chrono::milliseconds(timeout));
            struct pollfd fake_fd = {fd, POLLIN, 0};
            std::vector<struct pollfd> all_fds(fds, fds + nfds);
            all_fds.push_back(fake_fd);
            int result = real_poll(all_fds.data(), all_fds.size(), -1);
            close(fd);
            return result;
        }
    }

    int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
    {
        static const auto real_epoll_wait = (decltype(&epoll_wait))dlsym(RTLD_NEXT, "epoll_wait");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting() || timeout <= 0)
        {
            return real_epoll_wait(epfd, events, maxevents, timeout);
        }
        else
        {
            auto now = simulator.now();
            int fd = simulator.timerfdCreate();
            simulator.timerfdSetTime(fd, now + std::chrono::milliseconds(timeout));
            struct epoll_event fake_event;
            fake_event.events = EPOLLIN;
            fake_event.data.fd = fd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &fake_event);
            int result = real_epoll_wait(epfd, events, maxevents, -1);
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            return result;
        }
    }

    int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
    {
        static const auto real_select = (decltype(&select))dlsym(RTLD_NEXT, "select");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting() || !timeout)
        {
            return real_select(nfds, readfds, writefds, exceptfds, timeout);
        }
        else
        {
            auto now = simulator.now();
            auto duration = std::chrono::seconds(timeout->tv_sec) + std::chrono::microseconds(timeout->tv_usec);
            int fd = simulator.timerfdCreate();
            simulator.timerfdSetTime(fd, now + duration);
            fd_set fake_readfds = *readfds;
            FD_SET(fd, &fake_readfds);
            int result = real_select(std::max(nfds, fd + 1), &fake_readfds, writefds, exceptfds, nullptr);
            if (result > 0)
            {
                if (FD_ISSET(fd, &fake_readfds))
                {
                    uint64_t buf;
                    auto _ = read(fd, &buf, sizeof(buf));
                    (void)_;
                    result--;
                }
                FD_CLR(fd, &fake_readfds);
                *readfds = fake_readfds;
            }
            close(fd);
            return result;
        }
    }

    int timerfd_create(int clockid, int flags)
    {
        static const auto real_timerfd_create = (decltype(&timerfd_create))dlsym(RTLD_NEXT, "timerfd_create");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timerfd_create(clockid, flags);
        }
        else
        {
            if (flags & TFD_TIMER_CANCEL_ON_SET)
            {
                std::cerr << "TFD_TIMER_CANCEL_ON_SET is not supported" << std::endl;
                errno = EINVAL;
                return -1;
            }
            if (flags & TFD_NONBLOCK)
            {
                std::cerr << "TFD_NONBLOCK is not supported" << std::endl;
                errno = EINVAL;
                return -1;
            }

            return simulator.timerfdCreate();
        }
    }

    int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
    {
        static const auto real_timerfd_settime = (decltype(&timerfd_settime))dlsym(RTLD_NEXT, "timerfd_settime");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timerfd_settime(fd, flags, new_value, old_value);
        }
        else
        {
            try
            {
                if (old_value)
                {
                    // Retrieve the current timer settings
                    simulator.timerfdGetTime(fd, old_value);
                }

                if (new_value)
                {
                    if (new_value->it_value.tv_nsec >= 1000000000 || new_value->it_interval.tv_nsec >= 1000000000)
                    {
                        errno = EINVAL;
                        return -1;
                    }

                    std::chrono::time_point<FakeClock> expiration_time;

                    if (flags & TFD_TIMER_ABSTIME)
                    {
                        expiration_time = TimePoint(to_duration(new_value->it_value));
                    }
                    else
                    {
                        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::seconds(new_value->it_value.tv_sec) +
                            std::chrono::nanoseconds(new_value->it_value.tv_nsec));
                        auto now = simulator.now();
                        expiration_time = now + duration;
                    }

                    auto interval = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::seconds(new_value->it_interval.tv_sec) +
                        std::chrono::nanoseconds(new_value->it_interval.tv_nsec));

                    simulator.timerfdSetTime(fd, expiration_time, interval);
                }
            }
            catch (const std::out_of_range &e)
            {
                errno = EINVAL;
                return -1;
            }
            catch (...)

            {
                errno = EFAULT;
                return -1;
            }

            return 0;
        }
    }

    int timerfd_gettime(int fd, struct itimerspec *curr_value)
    {
        static const auto real_timerfd_gettime = (decltype(&timerfd_gettime))dlsym(RTLD_NEXT, "timerfd_gettime");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timerfd_gettime(fd, curr_value);
        }
        else
        {
            try
            {
                simulator.timerfdGetTime(fd, curr_value);
                return 0;
            }
            catch (const std::out_of_range &e)
            {
                errno = EINVAL;
                return -1;
            }
            catch (...)
            {
                errno = EFAULT;
                return -1;
            }
        }
    }

    int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request, struct timespec *remain)
    {
        static const auto real_clock_nanosleep = (decltype(&clock_nanosleep))dlsym(RTLD_NEXT, "clock_nanosleep");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_clock_nanosleep(clock_id, flags, request, remain);
        }
        else
        {
            if (!request)
            {
                return EFAULT;
            }

            if (request->tv_nsec < 0 || request->tv_nsec >= 1000000000)
            {
                return EINVAL;
            }

            try
            {
                auto now = simulator.now();

                if (flags & TIMER_ABSTIME)
                {
                    // For absolute time, calculate how much time to wait
                    auto target_time = TimePoint(to_duration(*request));
                    if (target_time <= now)
                    {
                        // Target time has already passed
                        return 0;
                    }

                    simulator.waitUntil(target_time);
                }
                else
                {
                    // For relative time, simply wait for the specified duration
                    auto duration = to_duration(*request);
                    simulator.waitUntil(now + duration);
                }

                // In simulated time, there's no real interruption, so we always succeed
                if (remain)
                {
                    remain->tv_sec = 0;
                    remain->tv_nsec = 0;
                }

                return 0;
            }
            catch (const std::exception &e)
            {
                return EFAULT;
            }
        }
    }
}

extern "C" {

    static constexpr auto CHECK_INTERVAL = std::chrono::milliseconds(1);

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
    {
        static const auto real_setsockopt = (decltype(&setsockopt))dlsym(RTLD_NEXT, "setsockopt");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        int ret = real_setsockopt(sockfd, level, optname, optval, optlen);
        if (!simulator.isIntercepting() || level != SOL_SOCKET || !optval || optlen < sizeof(timeval))
        {
            return ret;
        }

        timeval tv;
        memcpy(&tv, optval, sizeof(tv));
        auto timeout = fakeclock::to_duration(tv);

        auto &mgr = simulator.socketTimeouts();
        if (optname == SO_RCVTIMEO)
        {
            mgr.setRecv(sockfd, timeout);
        }
        else if (optname == SO_SNDTIMEO)
        {
            mgr.setSend(sockfd, timeout);
        }
        return ret;
    }

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
    {
        static const auto real_getsockopt = (decltype(&getsockopt))dlsym(RTLD_NEXT, "getsockopt");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        int ret = real_getsockopt(sockfd, level, optname, optval, optlen);
        if (!simulator.isIntercepting() || level != SOL_SOCKET || !optval || !optlen || *optlen < sizeof(timeval))
        {
            return ret;
        }

        auto &mgr = simulator.socketTimeouts();
        timeval tv{};
        std::chrono::nanoseconds timeout{0};
        if (optname == SO_RCVTIMEO)
        {
            timeout = mgr.getRecv(sockfd);
        }
        else if (optname == SO_SNDTIMEO)
        {
            timeout = mgr.getSend(sockfd);
        }
        tv.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
        tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(timeout).count() % 1000000;
        memcpy(optval, &tv, sizeof(tv));
        *optlen = sizeof(tv);
        return ret;
    }

    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        static const auto real_recv = (decltype(&recv))dlsym(RTLD_NEXT, "recv");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_recv(sockfd, buf, len, flags);
        }

        auto timeout = simulator.socketTimeouts().getRecv(sockfd);
        ssize_t ret = real_recv(sockfd, buf, len, flags | MSG_DONTWAIT);
        if (ret != -1 || errno != EAGAIN || timeout.count() == 0)
        {
            return ret;
        }

        auto start = simulator.now();
        while (simulator.now() - start < timeout)
        {
            auto remaining = timeout - (simulator.now() - start);
            auto step = std::min(remaining, std::chrono::duration_cast<std::chrono::nanoseconds>(CHECK_INTERVAL));
            simulator.waitUntil(simulator.now() + step);
            ret = real_recv(sockfd, buf, len, flags | MSG_DONTWAIT);
            if (ret != -1 || errno != EAGAIN)
            {
                return ret;
            }
        }
        return real_recv(sockfd, buf, len, flags | MSG_DONTWAIT);
    }

    ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    {
        static const auto real_send = (decltype(&send))dlsym(RTLD_NEXT, "send");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_send(sockfd, buf, len, flags);
        }

        auto timeout = simulator.socketTimeouts().getSend(sockfd);
        ssize_t ret = real_send(sockfd, buf, len, flags | MSG_DONTWAIT);
        if (ret != -1 || errno != EAGAIN || timeout.count() == 0)
        {
            return ret;
        }

        auto start = simulator.now();
        while (simulator.now() - start < timeout)
        {
            auto remaining = timeout - (simulator.now() - start);
            auto step = std::min(remaining, std::chrono::duration_cast<std::chrono::nanoseconds>(CHECK_INTERVAL));
            simulator.waitUntil(simulator.now() + step);
            ret = real_send(sockfd, buf, len, flags | MSG_DONTWAIT);
            if (ret != -1 || errno != EAGAIN)
            {
                return ret;
            }
        }
        return real_send(sockfd, buf, len, flags | MSG_DONTWAIT);
    }

    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    {
        static const auto real_connect = (decltype(&connect))dlsym(RTLD_NEXT, "connect");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_connect(sockfd, addr, addrlen);
        }

        auto timeout = simulator.socketTimeouts().getSend(sockfd);
        if (timeout.count() == 0)
        {
            return real_connect(sockfd, addr, addrlen);
        }

        int orig_flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, orig_flags | O_NONBLOCK);
        int ret = real_connect(sockfd, addr, addrlen);
        if (ret == -1 && errno == EINPROGRESS)
        {
            simulator.waitUntil(simulator.now() + timeout);
            ret = real_connect(sockfd, addr, addrlen);
            if (ret == -1 && errno == EINPROGRESS)
            {
                errno = ETIMEDOUT;
            }
            else if (ret == -1 && errno == EISCONN)
            {
                ret = 0;
            }
        }
        fcntl(sockfd, F_SETFL, orig_flags);
        return ret;
    }
}

// TODO:
//  clock_settime
//  clock_adjtime
//  clock_getres
//  clock_getcpuclockid
//  clock_gettime64
//  clock_settime64
//  clock_adjtime64
//  clock_getres64
//  clock_nanosleep64
//  timer_gettime64
//  timer_settime64
//  timerfd_gettime64
//  timerfd_settime64
//  socket operations with timeouts (implemented)
