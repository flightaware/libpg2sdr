/*
 *  flavor_support.c - flavor/CPU detection support for starch
 *
 *  Copyright (c) 2026 FlightAware All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __arm__

#include <asm/hwcap.h>
#include <sys/auxv.h>

int pg2sdr__starch_supports_arm_neon(void);
int pg2sdr__starch_supports_arm_neon(void)
{
    long hwcaps = getauxval(AT_HWCAP);
    return (hwcaps & HWCAP_ARM_NEON);
}

#endif /* __arm__ */

#ifdef __aarch64__

#include <asm/hwcap.h>
#include <sys/auxv.h>

int pg2sdr__starch_supports_aarch64_asimd(void);
int pg2sdr__starch_supports_aarch64_asimd(void)
{
    long hwcaps = getauxval(AT_HWCAP);
    return (hwcaps & HWCAP_ASIMD);
}

#endif

#ifdef __x86_64__

#include <cpuid.h>
#include <stdio.h>

int pg2sdr__starch_supports_x86_avx(void);
int pg2sdr__starch_supports_x86_avx(void)
{
    unsigned int maxlevel = __get_cpuid_max (0, 0);
    if (maxlevel < 1)
        return 0;

    unsigned eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    if (!(ecx & bit_AVX))
        return 0;

    return 1;
}

int pg2sdr__starch_supports_x86_avx2(void);
int pg2sdr__starch_supports_x86_avx2(void)
{
    unsigned int maxlevel = __get_cpuid_max (0, 0);
    if (maxlevel < 7)
        return 0;

    unsigned eax, ebx, ecx, edx;
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    if (!(ebx & bit_AVX2))
        return 0;

    return 1;
}

#endif
