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

# override the install rule from SOAPY_SDR_MODULE_UTIL to specify a component
include(GNUInstallDirs)
install(
  TARGETS pg2sdrSupport
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/SoapySDR/modules${SOAPY_SDR_ABI_VERSION}/"
    COMPONENT SoapySDR
)
