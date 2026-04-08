find_package(Python3 REQUIRED)

# Full library with ADC/tuner/DSP etc

# Generate gain-tables.gen.c from csv files
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gain-tables.gen.c
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gain/generate-gain-tables.py
    --lna-table ${CMAKE_SOURCE_DIR}/gain/lna.csv
    --mix-table ${CMAKE_SOURCE_DIR}/gain/mix.csv
    --vga-table ${CMAKE_SOURCE_DIR}/gain/vga.csv
    --curve ${CMAKE_SOURCE_DIR}/gain/sensitivity-curve.csv
    --c-tables ${CMAKE_CURRENT_BINARY_DIR}/gain-tables.gen.c
  DEPENDS
    ${CMAKE_SOURCE_DIR}/gain/generate-gain-tables.py
    ${CMAKE_SOURCE_DIR}/gain/lna.csv
    ${CMAKE_SOURCE_DIR}/gain/mix.csv
    ${CMAKE_SOURCE_DIR}/gain/vga.csv
    ${CMAKE_SOURCE_DIR}/gain/sensitivity-curve.csv)

# Generate bandpass-table.gen.c from csv file
set(BANDPASS_METRICS ${CMAKE_SOURCE_DIR}/bandpass/metrics-381A2B63986061DC.csv.gz
    CACHE FILEPATH "CSV file containing tuner bandpass filter metrics")
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bandpass-table.gen.c
  COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/bandpass/generate-bandpass-table.py
    --metrics ${BANDPASS_METRICS}
    --c-table ${CMAKE_CURRENT_BINARY_DIR}/bandpass-table.gen.c
  DEPENDS
    ${CMAKE_SOURCE_DIR}/bandpass/generate-bandpass-table.py
    ${BANDPASS_METRICS})

add_library(pg2sdr SHARED
  device.c adc.c stream.c tuner.c config.c gain.c bandpass.c
  ${CMAKE_CURRENT_BINARY_DIR}/gain-tables.gen.c
  ${CMAKE_CURRENT_BINARY_DIR}/bandpass-table.gen.c)

target_compile_options(pg2sdr PRIVATE -O3 -Wall -Wmissing-declarations -Werror)
target_compile_definitions(pg2sdr PRIVATE _DEFAULT_SOURCE $<$<CONFIG:DEBUG>:ENABLE_INSTRUMENTATION>)
target_include_directories(pg2sdr PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(pg2sdr pg2sdr_core pg2sdr_dsp m)
set_target_properties(pg2sdr PROPERTIES
  PUBLIC_HEADER "../include/pg2sdr.h"
  VERSION 1.0
  SOVERSION 1)

target_include_directories(pg2sdr PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(pg2sdr PUBLIC ${LIBUSB_CFLAGS_OTHERS})
target_link_libraries(pg2sdr ${LIBUSB_LIBRARIES})

include(GNUInstallDirs)
install(
  TARGETS pg2sdr
  LIBRARY
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT Libraries
    NAMELINK_COMPONENT Development
  PUBLIC_HEADER
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Development
)
