# Minimum version of CMake required to build this project
cmake_minimum_required(VERSION 3.15)

# Name of the project
project(libpg2sdr LANGUAGES C CXX)

# Set the C standard to C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(ENABLE_TESTING "Build the test code" OFF)
option(ENABLE_CLI "Build the CLI utilities" ON)
option(ENABLE_SOAPYSDR "Build the SoapySDR driver module" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Add subdirectories
add_subdirectory(corelib)
add_subdirectory(src)
add_subdirectory(dsp)

if(ENABLE_SOAPYSDR)
  add_subdirectory(soapy)
endif()

if(ENABLE_CLI)
  add_subdirectory(cli/pg2-util)
endif()

if(ENABLE_TESTING)
  # Fetch GoogleTest framework and build it
  include(FetchContent)
  FetchContent_Declare(
          googletest
            URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
          URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
          PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
          DOWNLOAD_NO_PROGRESS TRUE
        )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  # Tests
  include(CTest)
  include(GoogleTest)

  enable_testing()
  add_subdirectory(tests)
endif()

