find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

add_library(lpcsdr STATIC errors.c boot.c context.c device.c dsp.c control.c adc.c stream.c tuner.c config.c gain.c gain-tables.gen.c)
target_compile_options(lpcsdr PRIVATE -O3 -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE)
target_include_directories(lpcsdr PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(lpcsdr PRIVATE ${CMAKE_SOURCE_DIR}/lpcsdr_firmware/include)

target_include_directories(lpcsdr PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(lpcsdr PUBLIC ${LIBUSB_CFLAGS_OTHERS})
target_link_libraries(lpcsdr ${LIBUSB_LIBRARIES})
