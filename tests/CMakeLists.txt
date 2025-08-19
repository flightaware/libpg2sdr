# Include subdirectories
include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)

## Set the binary directory as a macro
add_compile_definitions(BINDIR="${CMAKE_BINARY_DIR}")
## Add the tests
add_executable(adc_tests.exe adc_test.cc)
target_link_libraries(adc_tests.exe GTest::gtest_main ${CMAKE_SOURCE_DIR}/build/src/libsdr.a usb-1.0 m)
add_test(NAME adc_tests.exe COMMAND adc_tests.exe)
set_tests_properties(adc_tests.exe PROPERTIES LABELS "Unit")

add_executable(dsp_tests.exe dsp_test.cc)
target_link_libraries(dsp_tests.exe GTest::gtest_main ${CMAKE_SOURCE_DIR}/build/src/libsdr.a usb-1.0 m)
add_test(NAME dsp_tests.exe COMMAND dsp_tests.exe)
set_tests_properties(dsp_tests.exe PROPERTIES LABELS "Unit")

add_executable(tuner_tests.exe tuner_test.cc util.cc)
target_link_libraries(tuner_tests.exe GTest::gtest_main ${CMAKE_SOURCE_DIR}/build/src/libsdr.a usb-1.0 m)
add_test(NAME tuner_tests.exe COMMAND tuner_tests.exe)
set_tests_properties(tuner_tests.exe PROPERTIES LABELS "Unit")

add_executable(stream_tests.exe stream_test.cc util.cc)
target_link_libraries(stream_tests.exe GTest::gtest_main ${CMAKE_SOURCE_DIR}/build/src/libsdr.a usb-1.0 m)
add_test(NAME stream_tests.exe COMMAND stream_tests.exe)
set_tests_properties(stream_tests.exe PROPERTIES LABELS "Integration")