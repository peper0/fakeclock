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

class MasterOfTime
{

  public:
    /// Monotonicity of std::chrono::steady_clock is preserved. Other clocks are changed stepwise.
    MasterOfTime();
    ~MasterOfTime();
    MasterOfTime(const MasterOfTime &) = delete;
    void advance(FakeClock::duration duration);
};

} // namespace fakeclock

#endif // FAKECLOCK_FAKECLOCK_H
