/*
 *  internal/tuner.h - PG2 host library internals, R860T helpers
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

#ifndef PG2SDR_TUNER_H
#define PG2SDR_TUNER_H

typedef struct {
    bool valid;

    bool refdiv;
    int seldiv;
    int feedback_n;
    int feedback_sdm;

    double actual_vco;
    double actual_frequency;
} tuner_pll_config_t;

int pg2sdr__init_tuner(pg2sdr_device *dev);
int pg2sdr__start_pll(pg2sdr_device *dev, tuner_pll_config_t *params);
int pg2sdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out);
int pg2sdr__has_pll_lock(pg2sdr_device *dev);
int pg2sdr__configure_pll_settings(pg2sdr_device *dev, tuner_pll_config_t *params);

int pg2sdr__tuner_set_lna(pg2sdr_device *dev, unsigned lna);
int pg2sdr__tuner_set_mix(pg2sdr_device *dev, unsigned mix);
int pg2sdr__tuner_set_vga(pg2sdr_device *dev, unsigned vga);
int pg2sdr__tuner_set_gains(pg2sdr_device *dev, int lna, int mix, int vga);

int pg2sdr__tuner_set_bandpass(pg2sdr_device *dev, const pg2sdr_bandpass_table_t *settings);

#endif /* PG2SDR_TUNER_H */
