find_package(SoapySDR REQUIRED)

SOAPY_SDR_MODULE_UTIL(
  TARGET pg2sdrSupport
  SOURCES
    soapy-pg2sdr.cc
  LIBRARIES
    pg2sdr)

target_compile_features(pg2sdrSupport PRIVATE cxx_std_17)
target_compile_options(pg2sdrSupport PRIVATE -O3 -Wall -Werror -Wno-psabi)
target_include_directories(pg2sdrSupport PRIVATE ${SoapySDR_INCLUDE_DIRS})
