# Include subdirectories
include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)

# hello_tests (tests on the 'hello' binary)
## Set the binary directory as a macro
add_compile_definitions(BINDIR="${CMAKE_BINARY_DIR}")
## Add the tests
add_executable(adc_tests.exe adc_test.cc ${CMAKE_SOURCE_DIR}/src/adc.c)
target_link_libraries(adc_tests.exe GTest::gtest_main)
add_test(NAME adc_tests.exe COMMAND adc_tests.exe)