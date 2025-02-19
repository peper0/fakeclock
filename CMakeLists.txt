cmake_minimum_required(VERSION 3.10)
project(FakeClock)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Add library
add_library(fakeclock src/fakeclock.cpp)

# Find GTest package
find_package(GTest REQUIRED)

# Add executable for tests
add_executable(test_fakeclock tests/test_fakeclock.cpp)

# Link the library and Google Test to the test executable
target_link_libraries(test_fakeclock fakeclock GTest::GTest pthread)

# Enable testing
enable_testing()

# Add test
add_test(NAME FakeClockTest COMMAND test_fakeclock)
