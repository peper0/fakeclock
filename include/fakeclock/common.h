#pragma once
#include <chrono>
#include <sys/time.h>

namespace fakeclock
{

inline timeval to_timeval(std::chrono::nanoseconds us)
{
    timeval tv;
    tv.tv_sec = us.count() / 1000000000;
    tv.tv_usec = (us.count() % 1000000000) / 1000;
    return tv;
}

inline timespec to_timespec(std::chrono::nanoseconds ns)
{
    timespec ts;
    ts.tv_sec = ns.count() / 1000000000;
    ts.tv_nsec = ns.count() % 1000000000;
    return ts;
}

inline std::chrono::nanoseconds to_duration(const timespec &ts)
{
    return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
}

inline std::chrono::nanoseconds to_duration(const timeval &tv)
{
    return std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec);
}

} // namespace fakeclock
