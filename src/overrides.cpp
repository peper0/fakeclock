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
#include <signal.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>

using TimePoint = fakeclock::ClockSimulator::TimePoint;
using Duration = fakeclock::ClockSimulator::Duration;
using fakeclock::FakeClock;
using fakeclock::to_duration;

// Data structure for tracking POSIX timer state
struct PosixTimer {
    clockid_t clockid;
    struct sigevent sevp;
    TimePoint expiration_time;
    Duration interval;
    bool armed;
};

// Map to store POSIX timers - key is the timer ID pointer value
static std::mutex posix_timers_mutex;
static std::unordered_map<timer_t, PosixTimer> posix_timers;

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

    int timer_create(clockid_t clockid, struct sigevent *sevp, timer_t *timerid)
    {
        static const auto real_timer_create = (decltype(&timer_create))dlsym(RTLD_NEXT, "timer_create");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timer_create(clockid, sevp, timerid);
        }
        else
        {
            // Validate parameters
            if (!timerid)
            {
                errno = EFAULT;
                return -1;
            }

            // Create a new timer ID (using a simple pointer cast to ensure uniqueness)
            static std::atomic<uintptr_t> next_timer_id{1};
            uintptr_t id = next_timer_id.fetch_add(1, std::memory_order_relaxed);
            *timerid = reinterpret_cast<timer_t>(id);

            // Set up the timer structure
            PosixTimer timer;
            timer.clockid = clockid;
            timer.armed = false;
            if (sevp)
            {
                timer.sevp = *sevp;
            }
            else
            {
                // Default: no notification
                memset(&timer.sevp, 0, sizeof(struct sigevent));
                timer.sevp.sigev_notify = SIGEV_NONE;
            }

            // Store the timer
            {
                std::lock_guard<std::mutex> lock(posix_timers_mutex);
                posix_timers[*timerid] = timer;
            }

            return 0;
        }
    }

    int timer_delete(timer_t timerid)
    {
        static const auto real_timer_delete = (decltype(&timer_delete))dlsym(RTLD_NEXT, "timer_delete");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timer_delete(timerid);
        }
        else
        {
            std::lock_guard<std::mutex> lock(posix_timers_mutex);
            auto it = posix_timers.find(timerid);
            if (it == posix_timers.end())
            {
                errno = EINVAL;
                return -1;
            }
            posix_timers.erase(it);
            return 0;
        }
    }

    int timer_settime(timer_t timerid, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
    {
        static const auto real_timer_settime = (decltype(&timer_settime))dlsym(RTLD_NEXT, "timer_settime");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timer_settime(timerid, flags, new_value, old_value);
        }
        else
        {
            // Validate parameters
            if (!new_value)
            {
                errno = EFAULT;
                return -1;
            }

            if (new_value->it_value.tv_nsec < 0 || new_value->it_value.tv_nsec >= 1000000000 ||
                new_value->it_interval.tv_nsec < 0 || new_value->it_interval.tv_nsec >= 1000000000)
            {
                errno = EINVAL;
                return -1;
            }

            std::lock_guard<std::mutex> lock(posix_timers_mutex);
            auto it = posix_timers.find(timerid);
            if (it == posix_timers.end())
            {
                errno = EINVAL;
                return -1;
            }

            // If requested, store the old timer value
            if (old_value)
            {
                // If timer is not armed, return zeros
                if (!it->second.armed)
                {
                    memset(old_value, 0, sizeof(struct itimerspec));
                }
                else
                {
                    // Calculate remaining time
                    auto now = simulator.now();
                    auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        it->second.expiration_time - now);
                    
                    if (remaining.count() <= 0)
                    {
                        // Timer has expired
                        old_value->it_value.tv_sec = 0;
                        old_value->it_value.tv_nsec = 0;
                    }
                    else
                    {
                        old_value->it_value = fakeclock::to_timespec(remaining);
                    }
                    
                    // Set interval
                    old_value->it_interval = fakeclock::to_timespec(it->second.interval);
                }
            }

            // Check if timer is being disarmed
            if (new_value->it_value.tv_sec == 0 && new_value->it_value.tv_nsec == 0)
            {
                it->second.armed = false;
                return 0;
            }

            // Set up new timer values
            TimePoint expiration_time;
            if (flags & TIMER_ABSTIME)
            {
                // Absolute time
                expiration_time = TimePoint(to_duration(new_value->it_value));
            }
            else
            {
                // Relative time
                auto now = simulator.now();
                auto duration = to_duration(new_value->it_value);
                expiration_time = now + duration;
            }

            // Set the interval
            Duration interval = to_duration(new_value->it_interval);

            // Update timer state
            it->second.expiration_time = expiration_time;
            it->second.interval = interval;
            it->second.armed = true;

            return 0;
        }
    }

    int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
    {
        static const auto real_timer_gettime = (decltype(&timer_gettime))dlsym(RTLD_NEXT, "timer_gettime");
        auto &simulator = fakeclock::ClockSimulator::getInstance();
        if (!simulator.isIntercepting())
        {
            return real_timer_gettime(timerid, curr_value);
        }
        else
        {
            // Validate parameters
            if (!curr_value)
            {
                errno = EFAULT;
                return -1;
            }

            std::lock_guard<std::mutex> lock(posix_timers_mutex);
            auto it = posix_timers.find(timerid);
            if (it == posix_timers.end())
            {
                errno = EINVAL;
                return -1;
            }

            // If timer is not armed, return zeros
            if (!it->second.armed)
            {
                memset(curr_value, 0, sizeof(struct itimerspec));
            }
            else
            {
                // Calculate remaining time
                auto now = simulator.now();
                auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    it->second.expiration_time - now);
                
                if (remaining.count() <= 0)
                {
                    // Timer has expired
                    curr_value->it_value.tv_sec = 0;
                    curr_value->it_value.tv_nsec = 0;
                    
                    // If there's an interval, the timer rearms itself
                    if (it->second.interval.count() > 0)
                    {
                        // Calculate how many intervals have passed since expiration
                        auto time_since_expiration = now - it->second.expiration_time;
                        auto intervals_elapsed = time_since_expiration / it->second.interval;
                        auto next_expiration = it->second.expiration_time + 
                                              (intervals_elapsed + 1) * it->second.interval;
                        
                        // Calculate time until next expiration
                        auto time_until_next = next_expiration - now;
                        curr_value->it_value = fakeclock::to_timespec(time_until_next);
                        
                        // Update the timer's expiration time
                        it->second.expiration_time = next_expiration;
                    }
                }
                else
                {
                    curr_value->it_value = fakeclock::to_timespec(remaining);
                }
                
                // Set interval
                curr_value->it_interval = fakeclock::to_timespec(it->second.interval);
            }

            return 0;
        }
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
//  timerfd_gettime
//  timerfd_gettime64
//  timerfd_settime64
//  socket operations with timeouts (e.g., connect, recv, send)