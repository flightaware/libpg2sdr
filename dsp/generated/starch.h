
/* starch generated code. Do not edit. */

#include "dsp.h"

/* mixes */

/* ARM, 64-bit */
#ifdef STARCH_MIX_AARCH64
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "aarch64"
#define STARCH_FLAVOR_ARMV8A_SIMD
#define STARCH_FLAVOR_DEFAULT
#endif /* STARCH_MIX_AARCH64 */

/* ARM, 32-bit */
#ifdef STARCH_MIX_ARM
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "arm"
#define STARCH_FLAVOR_ARMV7A_SIMD
#define STARCH_FLAVOR_DEFAULT
#endif /* STARCH_MIX_ARM */

/* Generic build, Compiler defaults only */
#ifdef STARCH_MIX_GENERIC
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "generic"
#define STARCH_FLAVOR_DEFAULT
#endif /* STARCH_MIX_GENERIC */

/* x86-64 */
#ifdef STARCH_MIX_X86_64
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "x86_64"
#define STARCH_FLAVOR_X86_64_AVX2
#define STARCH_FLAVOR_X86_64_AVX
#define STARCH_FLAVOR_DEFAULT
#endif /* STARCH_MIX_X86_64 */


/* entry points and registries */

typedef uint32_t (* pg2sdr__starch_halfband_decimate_block_ptr) ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
extern pg2sdr__starch_halfband_decimate_block_ptr pg2sdr__starch_halfband_decimate_block;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    pg2sdr__starch_halfband_decimate_block_ptr callable;
    int (*flavor_supported)();
} pg2sdr__starch_halfband_decimate_block_regentry;

extern pg2sdr__starch_halfband_decimate_block_regentry pg2sdr__starch_halfband_decimate_block_registry[];
pg2sdr__starch_halfband_decimate_block_regentry * pg2sdr__starch_halfband_decimate_block_select();
void pg2sdr__starch_halfband_decimate_block_set_wisdom( const char * const * received_wisdom );

typedef uint32_t (* pg2sdr__starch_fs4_mix_ptr) ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
extern pg2sdr__starch_fs4_mix_ptr pg2sdr__starch_fs4_mix;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    pg2sdr__starch_fs4_mix_ptr callable;
    int (*flavor_supported)();
} pg2sdr__starch_fs4_mix_regentry;

extern pg2sdr__starch_fs4_mix_regentry pg2sdr__starch_fs4_mix_registry[];
pg2sdr__starch_fs4_mix_regentry * pg2sdr__starch_fs4_mix_select();
void pg2sdr__starch_fs4_mix_set_wisdom( const char * const * received_wisdom );

typedef void (* pg2sdr__starch_unpack_raw_adc_data_ptr) ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
extern pg2sdr__starch_unpack_raw_adc_data_ptr pg2sdr__starch_unpack_raw_adc_data;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    pg2sdr__starch_unpack_raw_adc_data_ptr callable;
    int (*flavor_supported)();
} pg2sdr__starch_unpack_raw_adc_data_regentry;

extern pg2sdr__starch_unpack_raw_adc_data_regentry pg2sdr__starch_unpack_raw_adc_data_registry[];
pg2sdr__starch_unpack_raw_adc_data_regentry * pg2sdr__starch_unpack_raw_adc_data_select();
void pg2sdr__starch_unpack_raw_adc_data_set_wisdom( const char * const * received_wisdom );

typedef void (* pg2sdr__starch_unpack_raw_adc_data_invert_ptr) ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
extern pg2sdr__starch_unpack_raw_adc_data_invert_ptr pg2sdr__starch_unpack_raw_adc_data_invert;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    pg2sdr__starch_unpack_raw_adc_data_invert_ptr callable;
    int (*flavor_supported)();
} pg2sdr__starch_unpack_raw_adc_data_invert_regentry;

extern pg2sdr__starch_unpack_raw_adc_data_invert_regentry pg2sdr__starch_unpack_raw_adc_data_invert_registry[];
pg2sdr__starch_unpack_raw_adc_data_invert_regentry * pg2sdr__starch_unpack_raw_adc_data_invert_select();
void pg2sdr__starch_unpack_raw_adc_data_invert_set_wisdom( const char * const * received_wisdom );


/* flavors and prototypes */

#ifdef STARCH_FLAVOR_ARMV7A_SIMD
int pg2sdr__starch_supports_arm_neon (void);
uint32_t pg2sdr__starch_fs4_mix_generic_armv7a_simd ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_generic_armv7a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_generic_armv7a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
uint32_t pg2sdr__starch_halfband_decimate_block_generic_armv7a_simd ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
uint32_t pg2sdr__starch_halfband_decimate_block_neon_intrinsics_armv7a_simd ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
uint32_t pg2sdr__starch_fs4_mix_neon_intrinsics_armv7a_simd ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_neon_intrinsics_armv7a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_neon_intrinsics_armv7a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
#endif /* STARCH_FLAVOR_ARMV7A_SIMD */

int pg2sdr__starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_ARMV8A_SIMD
int pg2sdr__starch_supports_aarch64_asimd (void);
uint32_t pg2sdr__starch_fs4_mix_generic_armv8a_simd ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_generic_armv8a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_generic_armv8a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
uint32_t pg2sdr__starch_halfband_decimate_block_generic_armv8a_simd ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
uint32_t pg2sdr__starch_halfband_decimate_block_neon_intrinsics_armv8a_simd ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
uint32_t pg2sdr__starch_fs4_mix_neon_intrinsics_armv8a_simd ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_neon_intrinsics_armv8a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_neon_intrinsics_armv8a_simd ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
#endif /* STARCH_FLAVOR_ARMV8A_SIMD */

int pg2sdr__starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_DEFAULT
uint32_t pg2sdr__starch_fs4_mix_generic_default ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_generic_default ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_generic_default ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
uint32_t pg2sdr__starch_halfband_decimate_block_generic_default ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
#endif /* STARCH_FLAVOR_DEFAULT */

int pg2sdr__starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_X86_64_AVX
int pg2sdr__starch_supports_x86_avx (void);
uint32_t pg2sdr__starch_fs4_mix_generic_x86_64_avx ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_generic_x86_64_avx ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_generic_x86_64_avx ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
uint32_t pg2sdr__starch_halfband_decimate_block_generic_x86_64_avx ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
#endif /* STARCH_FLAVOR_X86_64_AVX */

int pg2sdr__starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_X86_64_AVX2
int pg2sdr__starch_supports_x86_avx2 (void);
uint32_t pg2sdr__starch_fs4_mix_generic_x86_64_avx2 ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_generic_x86_64_avx2 ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
void pg2sdr__starch_unpack_raw_adc_data_invert_generic_x86_64_avx2 ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );
uint32_t pg2sdr__starch_halfband_decimate_block_generic_x86_64_avx2 ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 );
#endif /* STARCH_FLAVOR_X86_64_AVX2 */

int pg2sdr__starch_read_wisdom (const char * path);

