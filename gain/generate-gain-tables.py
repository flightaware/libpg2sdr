#!/usr/bin/env python3

# generate-gain-tables.py - build-time codegen for gain tables
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
import csv
import typing
import argparse

class GainCurveEntry(typing.NamedTuple):
    gain_db: float
    lna: int
    mix: int
    vga: int

def read_gain_table_csv(f):
    results = [None] * 16
    f.readline() # skip header
    for row in csv.reader(f):
        results[int(row[0])] = float(row[1])
    return results

def read_gain_curve_csv(f):
    results = []
    for row in csv.DictReader(f):
        results.append(GainCurveEntry(gain_db = float(row['gain_db']),
                                      lna = int(row['LNA']),
                                      mix = int(row['MIX']),
                                      vga = int(row['VGA'])))
    return results

def make_c_array(iterable, max_per_line, format_fn, *args, **kwargs):
    lines = []
    line = []
    for x in iterable:
        line.append(format_fn(x, *args, **kwargs))
        if len(line) >= max_per_line:
            lines.append('    ' + ', '.join(line) + ',')
            line = []

    if line:
        lines.append('    ' + ', '.join(line) + ',')
    return '\n'.join(lines)

def format_gain_entry(entry):
    return f"{{ {entry.gain_db:5.2f}, {entry.lna:2d}, {entry.mix:2d}, {entry.vga:2d} }}"

def write_c_tables(path, lna_table, mix_table, vga_table, gain_curve):
    # Emit generated code for libpg2sdr with the given tables

    with open(path, 'w') as out:
        print(f"""
/* Generated code, don't edit */

#include "internal/lib.h"

const double pg2sdr__default_lna_table[16] = {{
{make_c_array(lna_table, 4, format, '5.2f')}
}};

const double pg2sdr__default_mix_table[16] = {{
{make_c_array(mix_table, 4, format, '5.2f')}
}};

const double pg2sdr__default_vga_table[16] = {{
{make_c_array(vga_table, 4, format, '5.2f')}
}};

const size_t pg2sdr__default_gain_table_size = {len(gain_curve)};
const pg2sdr_gain_table_t pg2sdr__default_gain_table[] = {{
{make_c_array(gain_curve, 1, format_gain_entry)}
}};
""", file=out)


def main():
    parser = argparse.ArgumentParser(description='Generate C gain tables from CSV files')

    parser.add_argument('--lna-table', help="Input CSV file with LNA gain lookup table", type=str, required=True)
    parser.add_argument('--mix-table', help="Input CSV file with MIX gain lookup table", type=str, required=True)
    parser.add_argument('--vga-table', help="Input CSV file with VGA gain lookup table", type=str, required=True)
    parser.add_argument('--curve',     help="Input CSV file with total gain curve entries", type=str, required=True)

    parser.add_argument('--c-tables', help="Path to C file to generate", type=str, required=True)

    args = parser.parse_args()

    with open(args.lna_table, 'r') as f:
        lna_table = read_gain_table_csv(f)

    with open(args.mix_table, 'r') as f:
        mix_table = read_gain_table_csv(f)

    with open(args.vga_table, 'r') as f:
        vga_table = read_gain_table_csv(f)

    with open(args.curve, 'r') as f:
        curve = read_gain_curve_csv(f)

    write_c_tables(args.c_tables,
                   lna_table,
                   mix_table,
                   vga_table,
                   curve)

if __name__ == '__main__':
    main()
