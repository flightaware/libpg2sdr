# Include subdirectories
include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)
add_compile_options(-Wno-psabi)

## Set the binary directory as a macro
add_compile_definitions(BINDIR="${CMAKE_BINARY_DIR}")
## Add the tests
add_executable(adc_tests adc_test.cc)
target_link_libraries(adc_tests GTest::gtest_main lpcsdr)
add_test(NAME adc_tests COMMAND adc_tests)
set_tests_properties(adc_tests PROPERTIES LABELS "Unit")

add_executable(dsp_tests dsp_test.cc)
target_link_libraries(dsp_tests GTest::gtest_main lpcsdr_dsp)
add_test(NAME dsp_tests COMMAND dsp_tests)
set_tests_properties(dsp_tests PROPERTIES LABELS "Unit")

add_executable(tuner_tests tuner_test.cc)
target_link_libraries(tuner_tests GTest::gtest_main lpcsdr)
add_test(NAME tuner_tests COMMAND tuner_tests)
set_tests_properties(tuner_tests PROPERTIES LABELS "Unit")

add_executable(stream_tests stream_test.cc)
target_link_libraries(stream_tests GTest::gtest_main lpcsdr)
add_test(NAME stream_tests COMMAND stream_tests)
set_tests_properties(stream_tests PROPERTIES LABELS "Integration")
