#include "fakeclock.h"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <dlfcn.h>

class ClockSimulator {
 public:
  using fake_clock = FakeClock;
  using time_point = fake_clock::time_point;
  static ClockSimulator &getInstance() {
    static ClockSimulator instance;
    return instance;
  }

  void addClock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (clock_count_++ == 0) {
      intercept();
    }
  }

  void removeClock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (--clock_count_ == 0) {
      restore();
      cv_.notify_all();  // Release all pending waits
    }
  }

  void advance(std::chrono::nanoseconds duration) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fake_time_ += duration;
    }
    cv_.notify_all();
  }

  void waitUntil(time_point tp) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { 
      if (!isIntercepting()) {
        std::cerr << "Error: FakeClock destroyed during some wait operation" << std::endl;
        return true;
      }
      return fake_time_ >= tp; 
    });
  }

  time_point now() const { return fake_time_; }

  bool isIntercepting() const { return intercepting_; }

 private:

  void intercept() {
    intercepting_ = true;
    // Intercept standard library time functions
    // This is a placeholder, actual implementation will depend on the platform
  }

  void restore() {
    intercepting_ = false;
    // Restore original time functions
    // This is a placeholder, actual implementation will depend on the platform
  }

  time_point fake_time_;
  std::atomic<int> clock_count_=0;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool intercepting_ = false;  // Indicates whether intercepting is active
};

ClockController::ClockController() { ClockSimulator::getInstance().addClock(); }

ClockController::~ClockController() { ClockSimulator::getInstance().removeClock(); }

void ClockController::advance(std::chrono::nanoseconds duration) {
  ClockSimulator::getInstance().advance(duration);
}

FakeClock::time_point FakeClock::now() noexcept {
  return ClockSimulator::getInstance().now();
}

extern "C" {
typedef unsigned int (*sleep_t)(unsigned int);
typedef int (*usleep_t)(useconds_t);
typedef int (*nanosleep_t)(const struct timespec *, struct timespec *);

static sleep_t real_sleep = nullptr;
static usleep_t real_usleep = nullptr;
static nanosleep_t real_nanosleep = nullptr;

struct InitRealFunctions {
  InitRealFunctions() {
    real_sleep = (sleep_t)dlsym(RTLD_NEXT, "sleep");
    real_usleep = (usleep_t)dlsym(RTLD_NEXT, "usleep");
    real_nanosleep = (nanosleep_t)dlsym(RTLD_NEXT, "nanosleep");
  }
};

static InitRealFunctions init_real_functions;

unsigned int sleep(unsigned int seconds) {
  auto &manager = ClockSimulator::getInstance();
  if (!manager.isIntercepting()) {
    return real_sleep(seconds);
  } else {
    auto now = manager.now();
    manager.waitUntil(now + std::chrono::seconds(seconds));
    return 0;
  }
}

int usleep(useconds_t usec) {
  auto &manager = ClockSimulator::getInstance();
  if (!manager.isIntercepting()) {
    return real_usleep(usec);
  } else {
    auto now = manager.now();
    manager.waitUntil(now + std::chrono::microseconds(usec));
    return 0;
  }
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  auto &manager = ClockSimulator::getInstance();
  if (!manager.isIntercepting()) {
    return real_nanosleep(req, rem);
  } else {
    auto duration = std::chrono::seconds(req->tv_sec) +
                    std::chrono::nanoseconds(req->tv_nsec);
    auto now = manager.now();
    manager.waitUntil(now + duration);
    return 0;
  }
}
}
