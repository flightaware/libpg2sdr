#!/bin/python3

import numpy as np
import scipy.signal
import SoapySDR
from scipy.signal import *
import csv
import os
import sys

root = os.path.dirname(os.path.dirname(os.getcwd()))

sys.path.insert(0, f"{root}/lpcsdr_firmware/python")
import lpcsdr.bulk

def read_from_tsv(file_path, chunk_size = 1310720, remove_header = True):
    csv.field_size_limit(sys.maxsize)
    results = []
    with open(file_path, 'r', newline='') as tsvfile:
        tsv_reader = csv.reader(tsvfile, delimiter='\t')
        
        if remove_header:
            next(tsv_reader)
        for index, row in enumerate(tsv_reader):
            val = row[1]
            val = val.split(",")
            int_vals = [int(x) for x in val]
            print(len(int_vals))
            if len(int_vals) != chunk_size:
                print(f"Ummmm {index}")

            results.append(int_vals)

    return results

def read_from_single_col_file(file_path, chunk_size = 1310720, remove_header = True):
    results = []
    with open(file_path, 'r', newline='\n') as file:
        result = []
        for line in file:
            # print(line)
            result.append(line)

            if len(result) == chunk_size:
                results.append(result)
                result = []

    results.append(result)
    print(len(results))
    return results

def capture(required_samples=960000, spb = 6808, chunk_size = 1310720):

    adc_samples = required_samples * 2 + 200  # ask for a few extra samples to help with FIR zero padding
    raw_blocks = read_from_single_col_file("../lpcsdr_firmware/python/adc_capture_script_raw_bytes.txt")
    print(len(raw_blocks[0]), len(raw_blocks[1]), len(raw_blocks[2]))


    fs4_mixer = np.array([1.0 + 0.0j,
                            0.0 + 1.0j,
                            -1.0 + 0.0j,
                            0.0 - 1.0j], dtype=np.complex64)
    assert (spb % len(fs4_mixer) == 0)
    tiled_fs4_mixer = np.tile(fs4_mixer, spb // len(fs4_mixer))

    mixed = np.empty(adc_samples + 9 * spb, dtype=np.complex64)
    mixed_offset = 0
    index = 0
    # print(f"len of mixed {len(mixed)}")

    with open("b4-mixed.tsv", "w", newline='\n') as output:
        tsv_output = csv.writer(output, delimiter='\t')
        
        tsv_output.writerow("index\tvalue")

        ######
        for b_index, adc_block in enumerate(lpcsdr.bulk.unpack_blocks(raw_blocks)):
            
            if b_index < 8:
                continue

            for b in adc_block.samples:
                tsv_output.writerow([index, b])
                index += 1

            assert len(adc_block.samples) == spb
            
            mixed[mixed_offset:mixed_offset+spb] = adc_block.samples * tiled_fs4_mixer
            mixed_offset += spb




    with open("after-mixed.tsv", "w", newline='\n') as output:
        tsv_output = csv.writer(output, delimiter='\t')
        
        tsv_output.writerow("index\tvalue")

        for index, r in enumerate(mixed):
            tsv_output.writerow([index,r])

    print(f"mixed offset {mixed_offset}, required_samples {required_samples}")
    # assert mixed_offset >= required_samples * 2

    q2 = 1
    n2 = 40

    # Copied from decimate scipy version 1.11.4.
    # https://github.com/scipy/scipy/blob/5e4a5e3785f79dd4e8930eed883da89958860db2/scipy/signal/_signaltools.py#L4458
    # Running this methods output (with hard_coded_coefficients is None) will give us same result as decimate from above
    def our_impl_of_decimate(mixed, mixed_offset, n2, q2, hard_coded_coefficients=None):
        axis = -1
        x = mixed[:mixed_offset]
        result_type = x.dtype
        if not np.issubdtype(result_type, np.inexact) \
            or result_type.type == np.float16:
        # upcast integers and float16 to float64
            result_type = np.float64

        if hard_coded_coefficients:
            print("Using hardcoded coef")
            b, a = hard_coded_coefficients, 1.
        else:
            print("Using firwin")
            b, a = scipy.signal.firwin(n2+1, 1. / q2, window='hamming'), 1.

        b = np.asarray(b, dtype=result_type)
        a = np.asarray(a, dtype=result_type)

        sl = [slice(None)] * x.ndim
        b = b / a
        # n_out = x.shape[axis] // q2 + bool(x.shape[axis] % q2)
        y = upfirdn(b, x, up=1, down=q2, axis=axis)
        y = y[40:len(y) - 40:]
        y = y[::2]
        with open("python-dec-our-impl.tsv", "w", newline='\n') as output:
            tsv_output = csv.writer(output, delimiter=',')
            

            for index, r in enumerate(y):
                tsv_output.writerow([int(r.real) >> 15, int(r.imag) >> 15])
                # tsv_output.writerow([round(r.real, 6), round(r.imag, 6)])
        return y
        # sl[axis] = slice(None, n_out, None)
        # return y[tuple(sl)]

    scaled_ceoff_with_center_tap_in_middle = [0, -20, 0, 51, 0, -98, 0, 173, 0, -282, 0, 445, 0, -693, 0, 1118, 0, -2039, 0, 6402, 10113, 6402, 0, -2039, 0, 1118, 0, -693, 0, 445, 0, -282, 0, 173, 0, -98, 0, 51, 0, -20, 0]
    non_scaled_coef = [0, -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537, 0.5, 0.316537,
0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091, 0,
]
    def check_symetric(b):
        i = 0
        y = len(b) - 1
        while i < y:
            if b[i] != b[y]:
                print("BOOOOO")
            i+=1
            y-=1

    check_symetric(scaled_ceoff_with_center_tap_in_middle)

    def manually_calculate_x_entries_with_scaled_taps(want, x, coeff):
        print("man calc")
        with open("man", 'w') as f:
        
            h = coeff
            index = want
            while index < len(x) - len(h) + 1:
                # real_s = 0
                # imag_s = 0
                # tap_offset = len(h) - 1 - max(0, (index + 1 - len(x)))
                # cur = min(index, len(x) - 1)

                # while cur >= 0 and tap_offset >= 0:

                #     real_s += x[cur].real * h[tap_offset]
                #     imag_s += x[cur].imag * h[tap_offset]
                #     tap_offset -= 1
                #     cur -= 1

                # print(f"{real_s},{imag_s}", file=f)
                # index += 1

                real_s = 0
                imag_s = 0
                tap_offset = 0
                cur = index

                while tap_offset < len(h):

                    real_s += x[cur].real * h[tap_offset]
                    imag_s += x[cur].imag * h[tap_offset]
                    print(f"cur {cur} real_s {real_s} imag_s {imag_s} tap {h[tap_offset]} tap_offset {tap_offset} real {x[cur].real} imag {x[cur].imag}")
                    tap_offset += 1
                    cur += 1
                print(f"{int(real_s) >> 15},{int(imag_s) >> 15}", file=f)
                # print(f"{round(real_s, 6)},{round(imag_s, 6)}", file=f)
                if index % 10000 == 0:
                    print(index)
                index += 1

                if index > want:
                    break



    # our_results = our_impl_of_decimate(mixed, mixed_offset, n2, q2)
    # w, h = signal.freqz([ 0, -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537, 0.5, 0.316537,
        # 0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091, 0,
    # ])
    our_results = our_impl_of_decimate(mixed, mixed_offset, n2, q2, hard_coded_coefficients=scaled_ceoff_with_center_tap_in_middle)

    
    manually_calculate_x_entries_with_scaled_taps(0, mixed, scaled_ceoff_with_center_tap_in_middle)
        

    # return exactly the number of samples requested, from the end of the output since
    # that will be less affected by zero-padding at the start of the FIR processing
    # result = decimated[-required_samples:]


    # with open("python-dec-scipy.tsv", "w", newline='\n') as output:
    #     tsv_output = csv.writer(output, delimiter='\t')
        
    #     tsv_output.writerow("index\tvalue")

    #     for index, r in enumerate(decimated):
    #         tsv_output.writerow([index,r])



    def main():
        n = 0
        with open("man") as file1, open("python-dec-our-impl.tsv") as file2:
            r1, r2 = csv.reader(file1, delimiter=','), csv.reader(file2, delimiter=',')
            for l1, l2 in zip(r1, r2):
                real1, real2 = float(l1[0]), float(l2[0])
                imag1, imag2 = float(l1[1]), float(l2[1]) 
                
                if abs(real1 - real2) > .00001:
                    print(f"{real1} {real2}")

                if abs(imag1 - imag2) > .00001:
                    print(f"{imag1} {imag2}")
                n+= 1

        print(n)

def compare_tsv(fp1, fp2, index1, index2):
    n = 0
    with open(fp1) as file1, open(fp2) as file2:
        r1, r2 = csv.reader(file1, delimiter='\t'), csv.reader(file2, delimiter='\t')
        for l1, l2 in zip(r1, r2):
            if float(l1[index1]) != float(l2[index2]):
                print(f"line {n}: {l1}, {l2}")
                return
            n += 1
    print(n)

compare_tsv("../build/tests/C_unpack.tsv", "../lpcsdr_firmware/python/adc_capture_script_direct_output.tsv", 1, 1)
# capture()