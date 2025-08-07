# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)

# Move executables to the root of the build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Add target and source files
add_library(sdr errors.c boot.c external.c device.c dsp.c config.c adc.c stream.c)

# $(shell pkg-config --libs libusb-1.0)
target_link_libraries(sdr usb-1.0 m)

target_compile_options(sdr PRIVATE -O3 -g -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE -fpic -fno-common)
