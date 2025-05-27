cmake_minimum_required(VERSION 3.10)
project(fakeclock)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()


# Add library
add_library(fakeclock SHARED 
    src/fakeclock.cpp 
    src/ClockSimulator.cpp 
    src/overrides.cpp
    src/posix_timers.cpp
)

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
    tests/test_clock_nanosleep.cpp
    tests/test_posix_timer.cpp
)
# Link the library and Google Test to the test executable
target_link_libraries(test_fakeclock fakeclock GTest::gtest GTest::gtest_main pthread)

# Enable testing
enable_testing()

# Add test
add_test(NAME FakeClockTest COMMAND test_fakeclock)

# Option to build examples
option(BUILD_EXAMPLES "Build example programs" ON)

# Build examples if enabled
if(BUILD_EXAMPLES)
    # Create directory for example
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples)
    
    # Extract example from README
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/examples/readme_example.cpp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/examples
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_readme_example.py
                ${CMAKE_CURRENT_SOURCE_DIR}/README.md
                ${CMAKE_CURRENT_BINARY_DIR}/examples/readme_example.cpp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_readme_example.py
        COMMENT "Extracting example from README.md"
    )
    
    # Add example executable (using the extracted code)
    add_executable(readme_example ${CMAKE_CURRENT_BINARY_DIR}/examples/readme_example.cpp)
    target_link_libraries(readme_example fakeclock)
    
    # Make sure the example is extracted before building
    add_custom_target(extract_example DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/examples/readme_example.cpp)
    add_dependencies(readme_example extract_example)
endif()

# Add install target
install(TARGETS fakeclock
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

