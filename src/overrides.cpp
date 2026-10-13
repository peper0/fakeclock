#include "real_function.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/common.h>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <queue>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
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
        const auto real_sleep = fakeclock::detail::realFunction<&sleep>(__func__);
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
        const auto real_usleep = fakeclock::detail::realFunction<&usleep>(__func__);
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
        const auto real_nanosleep = fakeclock::detail::realFunction<&nanosleep>(__func__);
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
        const auto real_gettimeofday = fakeclock::detail::realFunction<&gettimeofday>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_gettimeofday(tv, tz);
        }
        else
        {
            // TODO: handle tz
            auto duration = simulator.getTime(CLOCK_REALTIME).time_since_epoch();
            tv->tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            tv->tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;
            return 0;
        }
    }

    int clock_gettime(clockid_t clk_id, struct timespec *ts) noexcept
    {
        if (fakeclock::ClockSimulator::tryGetTime(clk_id, *ts))
        {
            return 0;
        }
        // dlsym may allocate and ASan's allocator itself calls clock_gettime.
        // The inactive path must also work before any dynamic initialization.
        return static_cast<int>(syscall(SYS_clock_gettime, clk_id, ts));
    }

    int settimeofday(const struct timeval *tv, const struct timezone *tz)
    {
        const auto real_settimeofday = fakeclock::detail::realFunction<&settimeofday>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_settimeofday(tv, tz);
        }
        else
        {
            if (!tv)
            {
                errno = EFAULT;
                return -1;
            }
            if (tv->tv_usec < 0 || tv->tv_usec >= 1000000)
            {
                errno = EINVAL;
                return -1;
            }

            auto duration = std::chrono::seconds(tv->tv_sec) + std::chrono::microseconds(tv->tv_usec);
            // TODO: handle tz
            simulator.setTime(TimePoint(duration), CLOCK_REALTIME);
            (void)tz;
            return 0;
        }
    }

    int clock_settime(clockid_t clk_id, const struct timespec *ts)
    {
        const auto real_clock_settime = fakeclock::detail::realFunction<&clock_settime>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_clock_settime(clk_id, ts);
        }
        else
        {
            if (ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000)
            {
                errno = EINVAL;
                return -1;
            }
            auto duration = std::chrono::seconds(ts->tv_sec) + std::chrono::nanoseconds(ts->tv_nsec);
            simulator.setTime(TimePoint(duration), clk_id);
            return 0;
        }
    }

    time_t time(time_t *t)
    {
        const auto real_time = fakeclock::detail::realFunction<&time>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_time(t);
        }
        else
        {
            time_t result =
                std::chrono::duration_cast<std::chrono::seconds>(simulator.getTime(CLOCK_REALTIME).time_since_epoch())
                    .count();
            if (t)
            {
                *t = result;
            }
            return result;
        }
    }

    int poll(struct pollfd *fds, nfds_t nfds, int timeout)
    {
        const auto real_poll = fakeclock::detail::realFunction<&poll>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting() || timeout <= 0)
        {
            return real_poll(fds, nfds, timeout);
        }
        else
        {
            auto now = simulator.now();
            int fd = simulator.timerfdCreate(CLOCK_MONOTONIC, 0);
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
        const auto real_epoll_wait = fakeclock::detail::realFunction<&epoll_wait>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting() || timeout <= 0)
        {
            return real_epoll_wait(epfd, events, maxevents, timeout);
        }
        else
        {
            auto now = simulator.now();
            int fd = simulator.timerfdCreate(CLOCK_MONOTONIC, 0);
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
        const auto real_select = fakeclock::detail::realFunction<&select>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting() || !timeout)
        {
            return real_select(nfds, readfds, writefds, exceptfds, timeout);
        }
        else
        {
            auto now = simulator.now();
            auto duration = std::chrono::seconds(timeout->tv_sec) + std::chrono::microseconds(timeout->tv_usec);
            int fd = simulator.timerfdCreate(CLOCK_MONOTONIC, 0);
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
        const auto real_timerfd_create = fakeclock::detail::realFunction<&timerfd_create>(__func__);
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timerfd_create(clockid, flags);
        }
        else
        {
            return simulator.timerfdCreate(clockid, flags);
        }
    }

    int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
    {
        const auto real_timerfd_settime = fakeclock::detail::realFunction<&timerfd_settime>(__func__);
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
                        auto clock_id = simulator.timerfdGetClockId(fd);
                        expiration_time =
                            simulator.toFakeTime(clock_id, {new_value->it_value.tv_sec, new_value->it_value.tv_nsec});
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
        const auto real_timerfd_gettime = fakeclock::detail::realFunction<&timerfd_gettime>(__func__);
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
        const auto real_clock_nanosleep = fakeclock::detail::realFunction<&clock_nanosleep>(__func__);
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
                    auto target_time = simulator.toFakeTime(clock_id, *request);
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
                    simulator.waitUntil(simulator.now() + duration);
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
