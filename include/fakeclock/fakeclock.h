#ifndef FAKECLOCK_FAKECLOCK_H
#define FAKECLOCK_FAKECLOCK_H

#include <chrono>

namespace fakeclock
{

class FakeClock
{
  public:
    using rep = std::chrono::nanoseconds::rep;
    using period = std::chrono::nanoseconds::period;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<FakeClock>;
    static const bool is_steady = true;

    static time_point now() noexcept;
};

enum class ClockType
{
    MONOTONIC,
    NON_MONOTONIC
};
class MasterOfTime
{

  public:
    /// Master of time is monotonic when by creating it, you are not going back in time.
    /// Non monotonic master of time will set current time to 1 second after epoch.
    MasterOfTime(ClockType type = ClockType::NON_MONOTONIC);
    ~MasterOfTime();
    MasterOfTime(const MasterOfTime &) = delete;
    void advance(FakeClock::duration duration);
};

} // namespace fakeclock

#endif // FAKECLOCK_FAKECLOCK_H
