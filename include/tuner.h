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

int pg2sdr__init_tuner(pg2sdr_device_handle *dev);
int pg2sdr__start_pll(pg2sdr_device_handle *dev, tuner_pll_config_t *params);
int pg2sdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out);
int pg2sdr__has_pll_lock(pg2sdr_device_handle *dev);
int pg2sdr__configure_pll_settings(pg2sdr_device_handle *dev, tuner_pll_config_t *params);

int pg2sdr__tuner_set_lna(pg2sdr_device_handle *dev, unsigned lna);
int pg2sdr__tuner_set_mix(pg2sdr_device_handle *dev, unsigned mix);
int pg2sdr__tuner_set_vga(pg2sdr_device_handle *dev, unsigned vga);
int pg2sdr__tuner_set_gains(pg2sdr_device_handle *dev, int lna, int mix, int vga);

int pg2sdr__tuner_set_bandpass(pg2sdr_device_handle *dev, const pg2sdr_bandpass_table_t *settings);

#endif /* PG2SDR_TUNER_H */
