#ifndef FAKECLOCK_FAKECLOCK_H
#define FAKECLOCK_FAKECLOCK_H

#include <chrono>

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
    MasterOfTime();
    ~MasterOfTime();
    void advance(FakeClock::duration duration);
};

#endif // FAKECLOCK_FAKECLOCK_H
