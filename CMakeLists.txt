# Minimum version of CMake required to build this project
cmake_minimum_required(VERSION 3.15)

# Name of the project
project(liblpcsdr LANGUAGES C CXX)

# Set the C standard to C11
set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_STANDARD 17)

# Fetch GoogleTest framework and build it
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        DOWNLOAD_NO_PROGRESS TRUE
      )
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Tests
include(CTest)
include(GoogleTest)

enable_testing()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)