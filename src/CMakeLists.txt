include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(sdr errors.c boot.c external.c device.c dsp.c control.c adc.c stream.c libusb_interface.c tuner.c)

# Link libusb through dynamic shell
#$(shell pkg-config --libs libusb-1.0)
target_link_libraries(sdr usb-1.0 m)

target_compile_options(sdr PRIVATE -O3 -g -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE -fpic -fno-common)
