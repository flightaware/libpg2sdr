find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

find_package(Python3 REQUIRED)

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

add_library(pg2sdr STATIC
  errors.c context.c device.c control.c adc.c stream.c tuner.c config.c gain.c bandpass.c
  ${CMAKE_CURRENT_BINARY_DIR}/gain-tables.gen.c
  ${CMAKE_CURRENT_BINARY_DIR}/bandpass-table.gen.c)

target_compile_options(pg2sdr PRIVATE -O3 -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE)
target_include_directories(pg2sdr PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(pg2sdr pg2sdr_dsp m)
set_target_properties(pg2sdr PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


target_include_directories(pg2sdr PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(pg2sdr PUBLIC ${LIBUSB_CFLAGS_OTHERS})
target_link_libraries(pg2sdr ${LIBUSB_LIBRARIES})
