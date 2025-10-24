include(DetectStarchMix.cmake)
include(generated/starch_${USE_STARCH_MIX}.cmake)

add_starch_library(lpcsdr_dsp STATIC src/flavor_support.c src/dsp.c)
target_include_directories(lpcsdr_dsp PUBLIC "include")
target_compile_options(lpcsdr_dsp PRIVATE -O3 -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE)
set_target_properties(lpcsdr_dsp PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

add_starch_benchmark(dsp-benchmark lpcsdr_dsp)
set_target_properties(dsp-benchmark PROPERTIES LINK_FLAGS -static)
