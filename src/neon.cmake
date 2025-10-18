include(CheckCSourceCompiles)

if(NOT DEFINED HAVE_NEON)
  # Work out if Neon intrinsics are supported
  # (possibly with an extra compiler option)

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

  message(CHECK_START "Detecting ARM Neon support")
  set(CMAKE_REQUIRED_QUIET true)
  set(_neon false)
  set(_neon_flags "")
  foreach(CMAKE_REQUIRED_FLAGS "" "-march=armv7-a+neon-vfpv4")
    set(_testvar NEON_WITH_FLAGS_${CMAKE_REQUIRED_FLAGS})
    check_c_source_compiles("${NEON_TEST}" ${_testvar})
    if(${_testvar})
      set(_neon true)
      set(_neon_flags "${CMAKE_REQUIRED_FLAGS}")
      break()
    endif()
  endforeach()

  set(HAVE_NEON ${_neon} CACHE BOOL "Enable use of ARM Neon intrinsics")
  set(NEON_COMPILE_OPTIONS "${_neon_flags}" CACHE STRING "Extra compiler options needed for ARM Neon support")

  unset(CMAKE_REQUIRED_QUIET)
  unset(CMAKE_REQUIRED_FLAGS)

  if(HAVE_NEON)
    message(CHECK_PASS "enabled, extra compile options: ${NEON_COMPILE_OPTIONS}")
  else()
    message(CHECK_PASS "disabled")
  endif()
endif()
