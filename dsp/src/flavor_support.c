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
