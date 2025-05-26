#include <cassert>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/common.h>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <string.h>
#include <unistd.h>

// ScopedSigpipeIgnore class definition
class ScopedSigpipeIgnore
{
  public:
    ScopedSigpipeIgnore()
    {
        // Save current signal handler for SIGPIPE
        sigaction(SIGPIPE, nullptr, &old_action_);
        new_action_ = old_action_;
        new_action_.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &new_action_, nullptr);
    }

    ~ScopedSigpipeIgnore()
    {
        // Restore original signal handler for SIGPIPE
        sigaction(SIGPIPE, &old_action_, nullptr);
    }

  private:
    struct sigaction old_action_;
    struct sigaction new_action_;
};

namespace fakeclock
{

ClockSimulator &ClockSimulator::getInstance()
{
    static ClockSimulator instance;
    return instance;
}

void ClockSimulator::addClock()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (clock_count_++ == 0)
    {
        intercept();
    }
}

void ClockSimulator::removeClock()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (--clock_count_ == 0)
    {
        restore();
        cv_.notify_all(); // Release all pending waits
    }
}

void ClockSimulator::handleExpiringFds()
{
    cleanupTimerfds();
    for (auto &[_, timerfd] : timerfds_)
    {
        timerfd.advance_to(fake_time_);
    }
}

void ClockSimulator::cleanupTimerfds()
{
    for (auto it = timerfds_.begin(); it != timerfds_.end();)
    {
        if (it->second.client_closed())
        {
            it = timerfds_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ClockSimulator::advance(std::chrono::nanoseconds duration)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fake_time_ += duration;
        handleExpiringFds();
    }
    cv_.notify_all();
}

void ClockSimulator::waitUntil(TimePoint tp)
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] {
        if (!isIntercepting())
        {
            std::cerr << "fakeclock error: MasterOfTime destroyed during some wait operation" << std::endl;
            return true;
        }
        return fake_time_ >= tp;
    });
}

ClockSimulator::TimePoint ClockSimulator::now() const
{
    return fake_time_;
}

bool ClockSimulator::isIntercepting() const
{
    return intercepting_;
}

int ClockSimulator::timerfdCreate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    TimerFd timer_fd;
    timer_fd.open();
    int client_fd = timer_fd.getClientFd();

    {
        /// if there is already such client_fd it must have been closed already
        auto it = timerfds_.find(client_fd);
        if (it != timerfds_.end())
        {
            assert(it->second.client_closed());
            timerfds_.erase(it);
        }
    }

    timerfds_.emplace(client_fd, std::move(timer_fd));
    return client_fd;
}

TimerFd &ClockSimulator::getTimerfd(int fd)
{
    return timerfds_.at(fd);
}

void ClockSimulator::timerfdSetTime(int fd, TimePoint tp, Duration interval)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto &timer_fd = timerfds_.at(fd);
    timer_fd.set_time(tp, interval);

    handleExpiringFds();
}

void ClockSimulator::timerfdGetTime(int fd, itimerspec *curr_value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto &timerfd = getTimerfd(fd);

    auto expiration_time = timerfd.get_expiration_time();
    if (expiration_time == TimerFd::DISARM_TIME)
    {
        curr_value->it_value = {0, 0}; // Disarmed
    }
    else
    {
        curr_value->it_value = to_timespec(Duration(expiration_time - fake_time_));
    }
    curr_value->it_interval = to_timespec(timerfd.get_interval());
}

void ClockSimulator::intercept()
{
    intercepting_ = true;
}

void ClockSimulator::restore()
{
    intercepting_ = false;
}

} // namespace fakeclock
