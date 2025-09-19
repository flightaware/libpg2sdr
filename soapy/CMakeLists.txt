find_package(SoapySDR REQUIRED)

add_library(lpcsdrSupport SHARED soapy-lpcsdr.cc)

# The SoapySDR driver for lpcsdr itself
target_compile_features(lpcsdrSupport PRIVATE cxx_std_17)
target_compile_options(lpcsdrSupport PRIVATE -O3 -Wall -Werror)
target_link_libraries(lpcsdrSupport lpcsdr)

# SoapySDR library dependency
target_include_directories(lpcsdrSupport PRIVATE ${SOAPYSDR_INCLUDE_DIRS})
target_compile_options(lpcsdrSupport PRIVATE ${SOAPYSDR_CFLAGS_OTHERS})
target_link_libraries(lpcsdrSupport ${SOAPYSDR_LIBRARIES})

# liblpcsdr dependency
target_link_libraries(lpcsdrSupport lpcsdr)
