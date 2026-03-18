find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

add_executable(pg2-firmware log.c device.c io.c image.c hotplug.c dfu_load.c mem_load.c nanojson.c meta.c load.c write.c device-info.c image-info.c pg2-firmware.c)
target_compile_options(pg2-firmware PRIVATE -O3 -Wall -Wmissing-declarations -Werror)
target_link_libraries(pg2-firmware pg2sdr)

target_include_directories(pg2-firmware PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(pg2-firmware PUBLIC ${LIBUSB_CFLAGS_OTHERS})
target_link_libraries(pg2-firmware ${LIBUSB_LIBRARIES})
