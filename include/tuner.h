#ifndef LPCSDR_TUNER_H
#define LPCSDR_TUNER_H

typedef struct {
    bool valid;

    bool refdiv;
    int seldiv;
    int feedback_n;
    int feedback_sdm;

    double actual_vco;
    double actual_frequency;
} tuner_pll_config_t;

int lpcsdr__init_tuner(lpcsdr_device_handle *dev);
int lpcsdr__start_pll(lpcsdr_device_handle *dev, tuner_pll_config_t *params);
int lpcsdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out);
int lpcsdr__has_pll_lock(lpcsdr_device_handle *dev);
int lpcsdr__configure_pll_settings(lpcsdr_device_handle *dev, tuner_pll_config_t *params);

int lpcsdr__tuner_set_lna(lpcsdr_device_handle *dev, unsigned lna);
int lpcsdr__tuner_set_mix(lpcsdr_device_handle *dev, unsigned mix);
int lpcsdr__tuner_set_vga(lpcsdr_device_handle *dev, unsigned vga);
int lpcsdr__tuner_set_gains(lpcsdr_device_handle *dev, int lna, int mix, int vga);

int lpcsdr__tuner_set_bandpass(lpcsdr_device_handle *dev, const lpcsdr_bandpass_table_t *settings);

#endif
