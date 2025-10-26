
/* starch generated code. Do not edit. */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "starch.h"

/* helper for re-sorting registries */
struct starch_regentry_prefix {
    int rank;
};

static int starch_regentry_rank_compare (const void *l, const void *r)
{
    const struct starch_regentry_prefix *left = l, *right = r;
    return left->rank - right->rank;
}

/* dispatcher / registry for fs4_mix */

lpcsdr__starch_fs4_mix_regentry * lpcsdr__starch_fs4_mix_select() {
    for (lpcsdr__starch_fs4_mix_regentry *entry = lpcsdr__starch_fs4_mix_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static uint32_t lpcsdr__starch_fs4_mix_dispatch ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 ) {
    lpcsdr__starch_fs4_mix_regentry *entry = lpcsdr__starch_fs4_mix_select();
    if (!entry)
        abort();

    lpcsdr__starch_fs4_mix = entry->callable;
    return lpcsdr__starch_fs4_mix ( arg0, arg1, arg2 );
}

lpcsdr__starch_fs4_mix_ptr lpcsdr__starch_fs4_mix = lpcsdr__starch_fs4_mix_dispatch;

void lpcsdr__starch_fs4_mix_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    lpcsdr__starch_fs4_mix_regentry *entry;
    for (entry = lpcsdr__starch_fs4_mix_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - lpcsdr__starch_fs4_mix_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(lpcsdr__starch_fs4_mix_registry, entry - lpcsdr__starch_fs4_mix_registry, sizeof(lpcsdr__starch_fs4_mix_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    lpcsdr__starch_fs4_mix = lpcsdr__starch_fs4_mix_dispatch;
}

lpcsdr__starch_fs4_mix_regentry lpcsdr__starch_fs4_mix_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_intrinsics_armv8a_simd", "armv8a_simd", lpcsdr__starch_fs4_mix_neon_intrinsics_armv8a_simd, lpcsdr__starch_supports_aarch64_asimd },
    { 1, "generic_default", "default", lpcsdr__starch_fs4_mix_generic_default, NULL },
    { 2, "generic_armv8a_simd", "armv8a_simd", lpcsdr__starch_fs4_mix_generic_armv8a_simd, lpcsdr__starch_supports_aarch64_asimd },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_intrinsics_armv7a_simd", "armv7a_simd", lpcsdr__starch_fs4_mix_neon_intrinsics_armv7a_simd, lpcsdr__starch_supports_arm_neon },
    { 1, "generic_default", "default", lpcsdr__starch_fs4_mix_generic_default, NULL },
    { 2, "generic_armv7a_simd", "armv7a_simd", lpcsdr__starch_fs4_mix_generic_armv7a_simd, lpcsdr__starch_supports_arm_neon },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_default", "default", lpcsdr__starch_fs4_mix_generic_default, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86_64
    { 0, "generic_x86_64_avx2", "x86_64_avx2", lpcsdr__starch_fs4_mix_generic_x86_64_avx2, lpcsdr__starch_supports_x86_avx2 },
    { 1, "generic_x86_64_avx", "x86_64_avx", lpcsdr__starch_fs4_mix_generic_x86_64_avx, lpcsdr__starch_supports_x86_avx },
    { 2, "generic_default", "default", lpcsdr__starch_fs4_mix_generic_default, NULL },
#endif /* STARCH_MIX_X86_64 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for halfband_decimate_block */

lpcsdr__starch_halfband_decimate_block_regentry * lpcsdr__starch_halfband_decimate_block_select() {
    for (lpcsdr__starch_halfband_decimate_block_regentry *entry = lpcsdr__starch_halfband_decimate_block_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static uint32_t lpcsdr__starch_halfband_decimate_block_dispatch ( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 ) {
    lpcsdr__starch_halfband_decimate_block_regentry *entry = lpcsdr__starch_halfband_decimate_block_select();
    if (!entry)
        abort();

    lpcsdr__starch_halfband_decimate_block = entry->callable;
    return lpcsdr__starch_halfband_decimate_block ( arg0, arg1, arg2, arg3 );
}

lpcsdr__starch_halfband_decimate_block_ptr lpcsdr__starch_halfband_decimate_block = lpcsdr__starch_halfband_decimate_block_dispatch;

void lpcsdr__starch_halfband_decimate_block_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    lpcsdr__starch_halfband_decimate_block_regentry *entry;
    for (entry = lpcsdr__starch_halfband_decimate_block_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - lpcsdr__starch_halfband_decimate_block_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(lpcsdr__starch_halfband_decimate_block_registry, entry - lpcsdr__starch_halfband_decimate_block_registry, sizeof(lpcsdr__starch_halfband_decimate_block_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    lpcsdr__starch_halfband_decimate_block = lpcsdr__starch_halfband_decimate_block_dispatch;
}

lpcsdr__starch_halfband_decimate_block_regentry lpcsdr__starch_halfband_decimate_block_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_intrinsics_armv8a_simd", "armv8a_simd", lpcsdr__starch_halfband_decimate_block_neon_intrinsics_armv8a_simd, lpcsdr__starch_supports_aarch64_asimd },
    { 1, "generic_default", "default", lpcsdr__starch_halfband_decimate_block_generic_default, NULL },
    { 2, "generic_armv8a_simd", "armv8a_simd", lpcsdr__starch_halfband_decimate_block_generic_armv8a_simd, lpcsdr__starch_supports_aarch64_asimd },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_intrinsics_armv7a_simd", "armv7a_simd", lpcsdr__starch_halfband_decimate_block_neon_intrinsics_armv7a_simd, lpcsdr__starch_supports_arm_neon },
    { 1, "generic_default", "default", lpcsdr__starch_halfband_decimate_block_generic_default, NULL },
    { 2, "generic_armv7a_simd", "armv7a_simd", lpcsdr__starch_halfband_decimate_block_generic_armv7a_simd, lpcsdr__starch_supports_arm_neon },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_default", "default", lpcsdr__starch_halfband_decimate_block_generic_default, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86_64
    { 0, "generic_x86_64_avx2", "x86_64_avx2", lpcsdr__starch_halfband_decimate_block_generic_x86_64_avx2, lpcsdr__starch_supports_x86_avx2 },
    { 1, "generic_x86_64_avx", "x86_64_avx", lpcsdr__starch_halfband_decimate_block_generic_x86_64_avx, lpcsdr__starch_supports_x86_avx },
    { 2, "generic_default", "default", lpcsdr__starch_halfband_decimate_block_generic_default, NULL },
#endif /* STARCH_MIX_X86_64 */
    { 0, NULL, NULL, NULL, NULL }
};


int lpcsdr__starch_read_wisdom (const char * path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    /* reset all ranks to identify entries not listed in the wisdom file; we'll assign ranks at the end to produce a stable sort */
    int rank_fs4_mix = 0;
    for (lpcsdr__starch_fs4_mix_regentry *entry = lpcsdr__starch_fs4_mix_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_halfband_decimate_block = 0;
    for (lpcsdr__starch_halfband_decimate_block_regentry *entry = lpcsdr__starch_halfband_decimate_block_registry; entry->name; ++entry) {
        entry->rank = 0;
    }

    char linebuf[512];
    while (fgets(linebuf, sizeof(linebuf), fp)) {
        /* split name and impl on whitespace, handle comments etc */
        char *name = linebuf;
        while (*name && isspace(*name))
            ++name;

        if (!*name || *name == '#')
            continue;

        char *end = name;
        while (*end && !isspace(*end))
            ++end;

        if (!*end)
            continue;
        *end = 0;

        char *impl = end + 1;
        while (*impl && isspace(*impl))
            ++impl;

        if (!*impl)
           continue;

        end = impl;
        while (*end && !isspace(*end))
            ++end;

        *end = 0;

        /* try to find a matching registry entry */
        if (!strcmp(name, "fs4_mix")) {
            for (lpcsdr__starch_fs4_mix_regentry *entry = lpcsdr__starch_fs4_mix_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_fs4_mix;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "halfband_decimate_block")) {
            for (lpcsdr__starch_halfband_decimate_block_regentry *entry = lpcsdr__starch_halfband_decimate_block_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_halfband_decimate_block;
                    break;
                }
            }
            continue;
        }
    }

    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    /* assign ranks to unmatched items to (stable) sort them last; re-sort everything */
    {
        lpcsdr__starch_fs4_mix_regentry *entry;
        for (entry = lpcsdr__starch_fs4_mix_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_fs4_mix;
        }
        qsort(lpcsdr__starch_fs4_mix_registry, entry - lpcsdr__starch_fs4_mix_registry, sizeof(lpcsdr__starch_fs4_mix_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        lpcsdr__starch_fs4_mix = lpcsdr__starch_fs4_mix_dispatch;
    }
    {
        lpcsdr__starch_halfband_decimate_block_regentry *entry;
        for (entry = lpcsdr__starch_halfband_decimate_block_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_halfband_decimate_block;
        }
        qsort(lpcsdr__starch_halfband_decimate_block_registry, entry - lpcsdr__starch_halfband_decimate_block_registry, sizeof(lpcsdr__starch_halfband_decimate_block_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        lpcsdr__starch_halfband_decimate_block = lpcsdr__starch_halfband_decimate_block_dispatch;
    }

    return 0;
}
