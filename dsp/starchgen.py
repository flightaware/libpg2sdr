#!/usr/bin/env python3

# starchgen.py - starch code generation driver
#
# Copyright (c) 2026 FlightAware All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



import sys
import os
import argparse

from starch.starch import Generator

def main():
    parser = argparse.ArgumentParser(description='Generate DSP support code via starch')

    parser.add_argument('--runtime-dir', help="Base directory for relative paths in generated code", type=str, required=True)
    parser.add_argument('--output-dir', help="Base directory for generated output", type=str, required=True)
    parser.add_argument('--symbol-prefix', help="Prefix for generated symbols", type=str, default="pg2sdr__starch_")

    args = parser.parse_args()

    gen = Generator(runtime_dir = args.runtime_dir,
                    output_dir = args.output_dir,
                    symbol_prefix = args.symbol_prefix)

    gen.add_include('"dsp/dsp.h"')

    gen.add_function(name = 'halfband_decimate_block',
                     argtypes = ['const dsp_halfband_decimate_state_t *', 'const cs16_t *', 'uint32_t', 'cs16_t *'],
                     returntype = 'uint32_t')
    gen.add_function(name = 'fs4_mix',
                     argtypes = ['const int16_t *', 'uint32_t', 'cs16_t *'],
                     returntype = 'uint32_t')
    gen.add_function(name = 'unpack_raw_adc_data',
                     argtypes = ['const uint32_t *', 'uint32_t', 'int16_t *'],
                     returntype = 'void')
    gen.add_function(name = 'unpack_raw_adc_data_invert',
                     argtypes = ['const uint32_t *', 'uint32_t', 'int16_t *'],
                     returntype = 'void')

    gen.add_flavor(name = 'default',
                   description = 'Build with compiler defaults',
                   compile_flags = [])
    armv7a_simd = gen.add_flavor(name = 'armv7a_simd',
                                 description = 'ARMv7-A with Advanced SIMD (Neon)',
                                 compile_flags = ['-march=armv7-a+simd'],
                                 test_function = 'pg2sdr__starch_supports_arm_neon')
    armv8a_simd =  gen.add_flavor(name = 'armv8a_simd',
                                  description = 'ARMv8-A with Advanced SIMD',
                                  compile_flags = [],
                                  test_function = 'pg2sdr__starch_supports_aarch64_asimd')
    gen.add_flavor(name = 'x86_64_avx',
                   description = 'x86-64 with AVX',
                   compile_flags = ['-mavx', '-ffast-math'],
                   test_function = 'pg2sdr__starch_supports_x86_avx')
    gen.add_flavor(name = 'x86_64_avx2',
                   description = 'x86-64 with AVX2',
                   compile_flags = ['-mavx2', '-ffast-math'],
                   test_function = 'pg2sdr__starch_supports_x86_avx2')

    gen.add_mix(name = 'generic',
                description = 'Generic build, Compiler defaults only',
                flavors = ['default'])
    gen.add_mix(name = 'arm',
                description = 'ARM, 32-bit',
                flavors = [armv7a_simd, 'default'],
                wisdom_file='wisdom.arm')
    gen.add_mix(name = 'aarch64',
                description = 'ARM, 64-bit',
                flavors = [armv8a_simd, 'default'],
                wisdom_file='wisdom.aarch64')
    gen.add_mix(name = 'x86_64',
                description = 'x86-64',
                flavors = ['x86_64_avx2', 'x86_64_avx', 'default'],
                wisdom_file='wisdom.x86_64')

    gen.scan_impls('impl/*.generic.c')
    gen.scan_impls('impl/*.neon.c', flavors=[armv7a_simd, armv8a_simd])
    gen.scan_benchmarks('impl/*.benchmark.c')

    gen.generate(cmake=True, makefiles=False)

if __name__ == '__main__':
    main()
