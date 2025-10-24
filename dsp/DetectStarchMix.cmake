include(CheckSymbolExists)

if(NOT DEFINED USE_STARCH_MIX)
  message(CHECK_START "Detecting starch mix to use")

  set(CMAKE_REQUIRED_QUIET true)
  check_symbol_exists(__arm__ "" ARCH_IS_ARM)
  check_symbol_exists(__aarch64__ "" ARCH_IS_AARCH64)
  check_symbol_exists(__x86_64__ "" ARCH_IS_X86_64)
  unset(CMAKE_REQUIRED_QUIET)

  if(ARCH_IS_ARM)
    set(_mix "arm")
  elseif(ARCH_IS_AARCH64)
    set(_mix "aarch64")
  elseif(ARCH_IS_X86_64)
    set(_mix "x86_64")
  else()
    set(_mix "generic")
  endif()

  message(CHECK_PASS "${_mix}")
  set(USE_STARCH_MIX ${_mix} CACHE STRING "Starch mix to use")
endif()
