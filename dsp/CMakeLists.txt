include(DetectStarchMix.cmake)
include(generated/starch_${USE_STARCH_MIX}.cmake)

add_starch_library(pg2sdr_dsp OBJECT src/flavor_support.c src/dsp.c)
target_include_directories(pg2sdr_dsp PUBLIC "include")
target_compile_options(pg2sdr_dsp PRIVATE -O3 -Wall -Wmissing-declarations -Werror -D_DEFAULT_SOURCE)
set_target_properties(pg2sdr_dsp PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

add_starch_benchmark(dsp-benchmark pg2sdr_dsp)
