find_package(SoapySDR REQUIRED)

SOAPY_SDR_MODULE_UTIL(
  TARGET lpcsdrSupport
  SOURCES
    soapy-lpcsdr.cc
  LIBRARIES
    lpcsdr)

target_compile_features(lpcsdrSupport PRIVATE cxx_std_17)
target_compile_options(lpcsdrSupport PRIVATE -O3 -Wall -Werror -Wno-psabi)
target_include_directories(lpcsdrSupport PRIVATE ${SoapySDR_INCLUDE_DIRS})
