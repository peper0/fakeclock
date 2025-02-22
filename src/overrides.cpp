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
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <unordered_map>

using TimePoint = ClockSimulator::TimePoint;
using Duration = ClockSimulator::Duration;

extern "C"
{
    typedef unsigned int (*sleep_t)(unsigned int);
    typedef int (*usleep_t)(useconds_t);
    typedef int (*nanosleep_t)(const struct timespec *, struct timespec *);
    typedef int (*gettimeofday_t)(struct timeval *, void *);
    typedef int (*clock_gettime_t)(clockid_t, struct timespec *);
    typedef int (*poll_t)(struct pollfd *, nfds_t, int);
    typedef int (*epoll_wait_t)(int, struct epoll_event *, int, int);
    typedef int (*select_t)(int, fd_set *, fd_set *, fd_set *, struct timeval *);
    typedef int (*timerfd_settime_t)(int, int, const struct itimerspec *, struct itimerspec *);
    typedef int (*timerfd_create_t)(int, int);
    typedef int (*timerfd_gettime_t)(int, struct itimerspec *);

    static sleep_t real_sleep = nullptr;
    static usleep_t real_usleep = nullptr;
    static nanosleep_t real_nanosleep = nullptr;
    static gettimeofday_t real_gettimeofday = nullptr;
    static clock_gettime_t real_clock_gettime = nullptr;
    static poll_t real_poll = nullptr;
    static epoll_wait_t real_epoll_wait = nullptr;
    static select_t real_select = nullptr;
    static timerfd_settime_t real_timerfd_settime = nullptr;
    static timerfd_create_t real_timerfd_create = nullptr;
    static timerfd_gettime_t real_timerfd_gettime = nullptr;

    struct InitRealFunctions
    {
        InitRealFunctions()
        {
            real_sleep = (sleep_t)dlsym(RTLD_NEXT, "sleep");
            real_usleep = (usleep_t)dlsym(RTLD_NEXT, "usleep");
            real_nanosleep = (nanosleep_t)dlsym(RTLD_NEXT, "nanosleep");
            real_gettimeofday = (gettimeofday_t)dlsym(RTLD_NEXT, "gettimeofday");
            real_clock_gettime = (clock_gettime_t)dlsym(RTLD_NEXT, "clock_gettime");
            real_poll = (poll_t)dlsym(RTLD_NEXT, "poll");
            real_epoll_wait = (epoll_wait_t)dlsym(RTLD_NEXT, "epoll_wait");
            real_select = (select_t)dlsym(RTLD_NEXT, "select");
            real_timerfd_settime = (timerfd_settime_t)dlsym(RTLD_NEXT, "timerfd_settime");
            real_timerfd_create = (timerfd_create_t)dlsym(RTLD_NEXT, "timerfd_create");
            real_timerfd_gettime = (timerfd_gettime_t)dlsym(RTLD_NEXT, "timerfd_gettime");
        }
    };

    static InitRealFunctions init_real_functions;

    unsigned int sleep(unsigned int seconds)
    {
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
                    read(fd, &buf, sizeof(buf));
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
        auto &simulator = ClockSimulator::getInstance();
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
}

// TODO:
//  timer_create
//  timer_settime
//  timer_gettime
//  clock_settime
//  clock_nanosleep
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
//  timerfd_gettime
//  timerfd_gettime64
//  timerfd_settime64
//  socket operations with timeouts (e.g., connect, recv, send)