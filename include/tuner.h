#ifndef LPCSDR_TUNER_H
#define LPCSDR_TUNER_H

#include <stdint.h>
typedef struct {
    bool valid;
    float cutoff;
    int lpf_coarse;
    int lpf_fine;
    int lpf_q;
    int lpf_narrow;
} lpf_settings;

typedef struct {
    bool valid;
    float cutoff;
    int hpf_corner;
} hpf_settings;

typedef struct {
    bool valid;

    bool refdiv;
    int seldiv;
    int feedback_n;
    int feedback_sdm;

    double actual_vco;
    double actual_frequency;
} tuner_pll_config_t;

const lpf_settings *lpcsdr__lpf_settings_for(double target, double max);
const hpf_settings *lpcsdr__hpf_settings_for(double target);
int lpcsdr__tuner_set_bandpass(lpcsdr_device_handle *dev, const hpf_settings *hpf, const lpf_settings *lpf);

int lpcsdr__init_tuner(lpcsdr_device_handle *dev);
int lpcsdr__start_pll(lpcsdr_device_handle *dev, tuner_pll_config_t *params);
int lpcsdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out);
int lpcsdr__has_pll_lock(lpcsdr_device_handle *dev);
int lpcsdr__configure_pll_settings(lpcsdr_device_handle *dev, tuner_pll_config_t *params);

int lpcsdr__vco_scan(lpcsdr_device_handle *dev);

int lpcsdr__tuner_set_lna(lpcsdr_device_handle *dev, unsigned lna);
int lpcsdr__tuner_set_mix(lpcsdr_device_handle *dev, unsigned mix);
int lpcsdr__tuner_set_vga(lpcsdr_device_handle *dev, unsigned vga);
int lpcsdr__tuner_set_gains(lpcsdr_device_handle *dev, int lna, int mix, int vga);

#endif
