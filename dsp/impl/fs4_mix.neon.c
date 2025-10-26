#include "dsp.h"

#include <assert.h>

#ifndef __ARM_NEON
#  error Neon intrinsics are not available
#endif
#include <arm_neon.h>

uint32_t STARCH_IMPL(fs4_mix, neon_intrinsics) (const int16_t * restrict in, uint32_t in_length, cs16_t * restrict out)
{
    assert (in_length % 4 == 0);

    const int16x4_t i_multiplier = vcreate_s16(0xFFFF0001FFFF0001); /* { -1, +1, -1, +1 } */
    const int16x4_t q_multiplier = vneg_s16(i_multiplier);          /* { +1, -1, +1, -1 } */
    const int16x4_t zero = vdup_n_s16(0);

    /* process 8 samples at a time */
    uint32_t i;
    for (i = 0; i + 8 <= in_length; i += 8, in += 8, out += 8) {
        int16x4x2_t in_samples = vld2_s16(in);                /* samples.val[0] = { in[6], in[4], in[2], in[0] }
                                                                 samples.val[1] = { in[7], in[5], in[3], in[1] } */

        int16x4_t out_i = vmul_s16(in_samples.val[0], i_multiplier);  /* out_i = { in[6]*-1, in[4]*1, in[2]*-1, in[0]*1 */
        int16x4_t out_q = vmul_s16(in_samples.val[1], q_multiplier);  /* out_q = { in[7]*1, in[5]*-1, in[3]*1, in[1]*-1 */

        int16x4x2_t zip_i = vzip_s16(out_i, zero);            /* zip_i.val[0] = { 0, in[2]*-1, 0, in[0]*1 }
                                                               * zip_q.val[1] = { 0, in[6]*-1, 0, in[4]*1 } */
        int16x4x2_t zip_q = vzip_s16(zero, out_q);            /* zip_i.val[0] = { in[3]*1, 0, in[1]*-1, 0 }
                                                               * zip_q.val[1] = { in[7]*1, 0, in[5]*-1, 0 } */

        int16x8x2_t out_iq;
        out_iq.val[0] = vcombine_s16(zip_i.val[0], zip_i.val[1]); /* { 0, in[6]*-1, 0, in[4]*1, 0, in[2]*-1, 0, in[0]*1 } */
        out_iq.val[1] = vcombine_s16(zip_q.val[0], zip_q.val[1]); /* { in[7]*1, 0, in[5]*-1, 0, in[3]*1, 0, in[1]*-1, 0 } */

        vst2q_s16((int16_t*) out, out_iq); /* store { in[0]*1, 0, 0, in[1]*-1, in[2]*-1, 0, 0, in[3]*1, .. etc .. */
    }

    /* handle trailing data if not a multiple of 8 */
    if (i < in_length) {
        out[0].i = in[0];
        out[0].q = 0;
        out[1].i = 0;
        out[1].q = -in[1];
        out[2].i = -in[2];
        out[2].q = 0;
        out[3].i = 0;
        out[3].q = in[3];
    }

    return in_length;
}
