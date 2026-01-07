#include <cassert>
#include <chrono>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/common.h>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <sys/timerfd.h>

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

void ClockSimulator::setTime(TimePoint tp, ClockId clk_id)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        setOffset(clk_id, tp - fake_time_);
        handleExpiringFds();
    }
    cv_.notify_all();
}

ClockSimulator::TimePoint ClockSimulator::now() const
{
    return fake_time_;
}

ClockSimulator::TimePoint ClockSimulator::getTime(ClockId clk_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fake_time_ + getOffset(clk_id);
}

bool ClockSimulator::isIntercepting() const
{
    return intercepting_;
}

int ClockSimulator::timerfdCreate(ClockId clock_id, int flags)
{
    std::lock_guard<std::mutex> lock(mutex_);
    TimerFd timer_fd;

    bool success = timer_fd.open(clock_id, flags);
    if (!success)
    {
        errno = EINVAL;
        return -1;
    }
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

ClockSimulator::ClockId ClockSimulator::timerfdGetClockId(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return timerfds_.at(fd).get_clock_id();
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

ClockSimulator::TimePoint ClockSimulator::toFakeTime(ClockId clk_id, timespec ts) const
{
    std::scoped_lock lock(mutex_);
    return TimePoint(to_duration(ts) - getOffset(clk_id));
}

timespec ClockSimulator::toTimespec(ClockId clk_id, TimePoint tp) const
{
    std::scoped_lock lock(mutex_);
    return fakeclock::to_timespec(tp.time_since_epoch() + getOffset(clk_id));
}

void ClockSimulator::intercept()
{
    intercepting_ = true;
}
void ClockSimulator::restore()
{
    intercepting_ = false;
}

ClockSimulator::Duration ClockSimulator::getOffset(ClockId clk_id) const
{
    if (clk_id >= 0 && clk_id < MAX_CLK_ID)
    {
        return clock_offsets_[clk_id];
    }
    return Duration::zero();
}

void ClockSimulator::setOffset(ClockId clk_id, Duration offset)
{
    if (clk_id >= 0 && clk_id < MAX_CLK_ID)
    {
        clock_offsets_[clk_id] = offset;
    }
    // In a real application, you might want to handle the case where the clock id is not found,
    // e.g., by adding it or logging an error. For now, we assume it's in the list.
}

} // namespace fakeclock
