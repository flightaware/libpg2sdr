find_package(SoapySDR REQUIRED)

add_library(lpcsdrSupport SHARED soapy-lpcsdr.cc)

# The SoapySDR driver for lpcsdr itself
target_compile_features(lpcsdrSupport PRIVATE cxx_std_17)
target_compile_options(lpcsdrSupport PRIVATE -O3 -Wall -Werror)
target_link_libraries(lpcsdrSupport lpcsdr)

# SoapySDR library dependency
target_include_directories(lpcsdrSupport PRIVATE ${SoapySDR_INCLUDE_DIRS})
target_link_libraries(lpcsdrSupport ${SoapySDR_LIBRARIES})

# liblpcsdr dependency
target_link_libraries(lpcsdrSupport lpcsdr)
