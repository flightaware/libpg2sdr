# Core sub-library used by both libpg2sdr and pg2-firmware

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

add_library(pg2sdr_core OBJECT
  errors.c context.c discovery.c control.c)
target_compile_options(pg2sdr_core PRIVATE -O3 -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE)
target_include_directories(pg2sdr_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_target_properties(pg2sdr_core PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(pg2sdr_core PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_compile_options(pg2sdr_core PUBLIC ${LIBUSB_CFLAGS_OTHERS})
target_link_libraries(pg2sdr_core ${LIBUSB_LIBRARIES})
