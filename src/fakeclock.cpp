#include <atomic>
#include <condition_variable>
#include <cstring>
#include <dlfcn.h>
#include <fakeclock/ClockSimulator.h>
#include <fakeclock/fakeclock.h>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <queue>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <unordered_map>

MasterOfTime::MasterOfTime()
{
    ClockSimulator::getInstance().addClock();
}

MasterOfTime::~MasterOfTime()
{
    ClockSimulator::getInstance().removeClock();
}

void MasterOfTime::advance(FakeClock::duration duration)
{
    ClockSimulator::getInstance().advance(duration);
}

FakeClock::time_point FakeClock::now() noexcept
{
    return ClockSimulator::getInstance().now();
}
