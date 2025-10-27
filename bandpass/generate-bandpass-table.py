#!/usr/bin/env python3

import sys
import csv
import typing
import argparse
import gzip

class BandpassEntry(typing.NamedTuple):
    hpf_corner: int
    lpf_narrow: int
    lpf_coarse: int
    lpf_fine: int
    lpf_q: int

    cutoff_low_freq: float
    cutoff_high_freq: float
    center_freq: float
    bandwidth: float
    passband_db: float
    passband_ripple_min_db: float
    passband_ripple_max_db: float
    passband_ripple_db: float
    stopband_low_freq: float
    stopband_high_freq: float
    low_rolloff: float
    high_rolloff: float
    low_corner_freq: float
    high_corner_freq: float

def read_metrics_csv(f):
    results = []
    int_columns = set(['hpf_corner', 'lpf_narrow', 'lpf_coarse', 'lpf_fine', 'lpf_q'])
    float_columns = set(BandpassEntry._fields) - int_columns
    for row in csv.DictReader(f):
        ints = {k:int(v) for k,v in row.items() if k in int_columns}
        floats = {k:float(v) for k,v in row.items() if k in float_columns}
        results.append(BandpassEntry(**(ints | floats)))
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

def format_bandpass_entry(entry):
    return f"{{ {entry.low_corner_freq:.0f}, {entry.high_corner_freq:.0f}, {entry.passband_ripple_db:.1f}, {entry.hpf_corner}, {entry.lpf_narrow}, {entry.lpf_coarse}, {entry.lpf_fine}, {entry.lpf_q} }}"

def write_c_table(path, entries):
    # Emit generated code for liblpcsdr with the given table

    with open(path, 'w') as out:
        print(f"""
/* Generated code, don't edit */

#include "internal.h"

const size_t pg2sdr__default_bandpass_table_size = {len(entries)};
const lpcsdr_bandpass_table_t pg2sdr__default_bandpass_table[] = {{
{make_c_array(entries, 1, format_bandpass_entry)}
}};
""", file=out)


def gzopen(path, mode):
    if path.endswith('.gz'):
        return gzip.open(path, mode)
    else:
        return open(path, mode)


def main():
    parser = argparse.ArgumentParser(description='Generate C bandpass table from CSV file')

    parser.add_argument('--metrics', help="Input CSV file with filter metrics", type=str, required=True)

    parser.add_argument('--min-lpf-rolloff', help="Set minimum acceptable LPF rolloff (dB/decade)", type=float, default=-200.0)
    parser.add_argument('--min-hpf-rolloff', help="Set minimum acceptable HPF rolloff (dB/decade)", type=float, default=-50.0)
    parser.add_argument('--max-ripple', help="Set maximum acceptable passband ripple (dB)", type=float, default=6.0)
    parser.add_argument('--min-bandwidth', help="Set minimum acceptable passband width (Hz)", type=float, default=500e3)

    parser.add_argument('--c-table', help="Path to C file to generate", type=str, required=True)

    args = parser.parse_args()

    with gzopen(args.metrics, 'rt') as f:
        metrics = read_metrics_csv(f)

    print(f"Read {len(metrics)} filter metrics from {args.metrics}", file=sys.stderr)

    metrics = [m for m in metrics if abs(m.low_rolloff) >= abs(args.min_hpf_rolloff)]
    metrics = [m for m in metrics if abs(m.high_rolloff) >= abs(args.min_lpf_rolloff)]
    metrics = [m for m in metrics if m.passband_ripple_db < args.max_ripple]
    metrics = [m for m in metrics if m.bandwidth >= args.min_bandwidth]

    metrics.sort(key=lambda x: (-x.hpf_corner, -x.lpf_narrow, -x.lpf_coarse, -x.lpf_fine, x.lpf_q))

    print(f"Writing {len(metrics)} entries to {args.c_table}")
    write_c_table(args.c_table, metrics)

if __name__ == '__main__':
    main()
