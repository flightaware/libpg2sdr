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

include (neon.cmake)

add_library(lpcsdr STATIC
  errors.c boot.c context.c device.c dsp.c control.c adc.c stream.c tuner.c config.c gain.c bandpass.c
  ${CMAKE_CURRENT_BINARY_DIR}/gain-tables.gen.c
  ${CMAKE_CURRENT_BINARY_DIR}/bandpass-table.gen.c)

target_compile_options(lpcsdr PRIVATE -O3 -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE)
target_include_directories(lpcsdr PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(lpcsdr PRIVATE ${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)
set_target_properties(lpcsdr PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(HAVE_NEON)
  set_source_files_properties(dsp.c PROPERTIES COMPILE_OPTIONS "-DHAVE_NEON;${NEON_COMPILE_OPTIONS}")
endif()

target_include_directories(lpcsdr PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(lpcsdr PUBLIC ${LIBUSB_CFLAGS_OTHERS})
target_link_libraries(lpcsdr ${LIBUSB_LIBRARIES})
