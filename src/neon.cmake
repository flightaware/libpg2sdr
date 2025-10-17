set(NEON_TEST "
#ifndef __ARM_NEON
#  error No NEON support
#endif
#include <arm_neon.h>
int main(int argc, char **argv) {
  int16x4_t t = vdup_n_s16(0);
  return 0;
}
")

if((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch.*") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*"))
  # Work out if we need a flag to enable NEON
  check_c_source_compiles("${NEON_TEST}" HAVE_NEON_DEFAULT)
  if(HAVE_NEON_DEFAULT)
    message("-- NEON: enabled, no additional compiler flags needed")
  else()
    set(CMAKE_REQUIRED_FLAGS "-march=armv7-a+neon")
    check_c_source_compiles("${NEON_TEST}" HAVE_NEON_WITH_MARCH)
    if (HAVE_NEON_WITH_MARCH)
      message("-- NEON: enabled, with -march=armv7-a+neon")
      set(NEON_EXTRA_FLAGS "-march=armv7-a+neon")
    else()
      message("-- NEON: disabled, can't find suitable compiler flags")
    endif()
  endif()
else()
  message("-- NEON: disabled, not building for ARM")
endif()
