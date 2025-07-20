#ifndef FAKECLOCK_CLOCKSIMULATOR_H
#define FAKECLOCK_CLOCKSIMULATOR_H

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <fakeclock/fakeclock.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>

namespace fakeclock
{

inline bool are_fds_equivalent(int fd1, int fd2)
{
#if 0
    // we cannot SYS_kcmp here because sometimes we get EPERM on it (e.g. in docker)
    long res = syscall(SYS_kcmp, getpid(), getpid(), KCMP_FILE, fd1, fd2);
    return res == 0;
#else
    // compare strings from /proc/self/fdinfo/<fd1> with /proc/self/fdinfo/<fd2>

    std::ifstream fd1_info("/proc/self/fdinfo/" + std::to_string(fd1));
    std::ifstream fd2_info("/proc/self/fdinfo/" + std::to_string(fd2));
    if (!fd1_info.is_open() || !fd2_info.is_open())
    {
        return false;
    }
    std::string info1((std::istreambuf_iterator<char>(fd1_info)), std::istreambuf_iterator<char>());
    std::string info2((std::istreambuf_iterator<char>(fd2_info)), std::istreambuf_iterator<char>());
    return info1 == info2;
#endif
}

class TimerFd
{
    using TimePoint = FakeClock::time_point;
    using Duration = FakeClock::duration;

  public:
    static constexpr auto DISARM_TIME = TimePoint{}; // TODO: change it to max and use everywhere

    TimerFd() = default;

    ~TimerFd()
    {
        if (isValid())
        {
            this->close();
        }
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
    void swap(TimerFd &other)
    {
        std::swap(client_fd, other.client_fd);
        std::swap(my_fd, other.my_fd);
        std::swap(next_expiration_time, other.next_expiration_time);
        std::swap(interval, other.interval);
    }
    void open()
    {
        assert(!*this); // already opened
        client_fd = eventfd(0, 0);
        my_fd = dup(client_fd);
    }
    void close()
    {
        assert(isValid());
        if (!client_closed())
        {
            std::cerr << "fakeclock error: TimerFd " << client_fd << " not closed" << std::endl;
        }
        ::close(my_fd);
        my_fd = -1;
        client_fd = -1;
    }
    void set_time(TimePoint next_expiration_time, Duration interval = Duration::zero())
    {
        assert(isValid());
        this->next_expiration_time = next_expiration_time;
        this->interval = interval;
    }
    TimePoint get_expiration_time() const
    {
        assert(isValid());
        return next_expiration_time;
    }
    Duration get_interval() const
    {
        assert(isValid());
        return interval;
    }
    void advance_to(TimePoint t)
    {
        assert(isValid());
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
        assert(isValid());
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
        assert(isValid());
        return !are_fds_equivalent(client_fd, my_fd);
    }
    bool isValid() const
    {
        return my_fd != -1;
    }
    operator bool() const
    {
        return isValid();
    }
    int getClientFd() const
    {
        return client_fd;
    }

  private:
    int client_fd = -1;
    int my_fd = -1;
    TimePoint next_expiration_time = DISARM_TIME;
    Duration interval = Duration::zero();
};

class ClockSimulator
{
  public:
    using TimePoint = FakeClock::time_point;
    using Duration = FakeClock::duration;
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

} // namespace fakeclock

#endif // FAKECLOCK_CLOCKSIMULATOR_H
