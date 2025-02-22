cmake_minimum_required(VERSION 3.10)
project(FakeClock)

# Set to C++20 (no gnu)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add library
add_library(fakeclock SHARED src/fakeclock.cpp src/ClockSimulator.cpp src/overrides.cpp)

target_include_directories(fakeclock PUBLIC include)

# Find GTest package
find_package(GTest REQUIRED)

# Add executable for tests
add_executable(test_fakeclock 
tests/test_fakeclock.cpp
tests/test_boost.cpp
tests/test_ClockSimulator.cpp 
tests/test_timerfd.cpp
tests/test_sleep.cpp
tests/test_gettime.cpp
)
# Link the library and Google Test to the test executable
target_link_libraries(test_fakeclock fakeclock GTest::gtest GTest::gtest_main pthread)

# Enable testing
enable_testing()

# Add test
add_test(NAME FakeClockTest COMMAND test_fakeclock)

