#ifndef FAKECLOCK_CLOCKSIMULATOR_H
#define FAKECLOCK_CLOCKSIMULATOR_H

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <fakeclock/fakeclock.h>
#include <linux/kcmp.h>
#include <mutex>
#include <queue>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>

class TimerFd
{
    using TimePoint = FakeClock::time_point;
    using Duration = FakeClock::duration;

  public:
    TimerFd()
    {
        client_fd = eventfd(0, 0);
        my_fd = dup(client_fd);
        interval = {};
        next_expiration_time = {};
    }

    ~TimerFd()
    {
        close(my_fd);
        // client_fd is always closed by the user
    }

    void swap(TimerFd &other)
    {
        std::swap(client_fd, other.client_fd);
        std::swap(my_fd, other.my_fd);
        std::swap(next_expiration_time, other.next_expiration_time);
        std::swap(interval, other.interval);
    }
    TimerFd(const TimerFd &other) = delete;
    TimerFd &operator=(const TimerFd &other) = delete;
    TimerFd(TimerFd &&other)
    {
        swap(other);
    }
    TimerFd &operator=(TimerFd &&other)
    {
        swap(other);
        return *this;
    }
    void set_time(TimePoint next_expiration_time, Duration interval = Duration::zero())
    {
        this->next_expiration_time = next_expiration_time;
        this->interval = interval;
    }
    TimePoint get_expiration_time() const
    {
        return next_expiration_time;
    }
    Duration get_interval() const
    {
        return interval;
    }
    void advance_to(TimePoint t)
    {
        if (t >= next_expiration_time && next_expiration_time != TimePoint{})
        {
            if (interval != Duration::zero())
            {
                expire(1 + (t - next_expiration_time) / interval);
            }
            else
            {
                expire();
            }
        }
    }
    void expire(int times = 1)
    {
        uint64_t value = times;
        auto _ = write(client_fd, &value, sizeof(value));
        (void)_;
        assert(next_expiration_time != TimePoint{}); // disarmed clock should not expire
        if (interval != Duration::zero())
        {
            next_expiration_time += interval * times;
        }
        else
        {
            next_expiration_time = {};
        }
    }
    bool client_closed() const
    {
        return syscall(SYS_kcmp, getpid(), getpid(), KCMP_FILE, client_fd, my_fd) != 0;
    }
    int getClientFd() const
    {
        return client_fd;
    }

  private:
    int client_fd;
    int my_fd;
    TimePoint next_expiration_time;
    Duration interval;
};

class ClockSimulator
{
  public:
    using TimePoint = FakeClock::time_point;
    using Duration = FakeClock::duration;
    static constexpr auto DISARM_TIME = TimePoint{};  // TODO: change it to max and use everywhere
    static ClockSimulator &getInstance();

    void addClock();
    void removeClock();
    void handleExpiringFds();
    void cleanupTimerfds();
    void advance(std::chrono::nanoseconds duration);
    void waitUntil(TimePoint tp);
    TimePoint now() const;
    bool isIntercepting() const;
    int timerfdCreate();
    void timerfdSetTime(int fd, TimePoint tp, Duration interval = Duration::zero());
    void timerfdGetTime(int fd, struct itimerspec *curr_value);

  private:
    ClockSimulator() = default;
    void intercept();
    void restore();
    TimerFd &getTimerfd(int fd);

    TimePoint fake_time_ /* zero is used as "no value" */ = TimePoint(std::chrono::seconds{1});
    std::atomic<int> clock_count_ = 0;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool intercepting_ = false;
    std::unordered_map<int, TimerFd> timerfds_;
};

#endif // FAKECLOCK_CLOCKSIMULATOR_H
