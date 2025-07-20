#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/common.h>
#include <mutex>
#include <signal.h>
#include <time.h>
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
