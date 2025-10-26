
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_ARMV8A_SIMD

#include "starch.h"

#define STARCH_SYMBOL(_name) lpcsdr__starch_ ## _name ## _ ## armv8a_simd
#define STARCH_IMPL(_function,_impl) lpcsdr__starch_ ## _function ## _ ## _impl ## _ ## armv8a_simd

#include "../impl/fs4_mix.generic.c"
#include "../impl/fs4_mix.neon.c"
#include "../impl/halfband_decimate.generic.c"
#include "../impl/halfband_decimate.neon.c"
