/*
 *  selftest.c - "pg2-util selftest" subcommand
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

#include "internal/core.h"
#include "log.h"
#include "device.h"
#include "io.h"
#include "image.h"
#include "dfu_load.h"
#include "mem_load.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

static bool selftest_single_device(libusb_device *dev, firmware_image_t *image);
static bool do_selftest(const char *image_path, const char *serial_prefix, const char *port_path);
static void show_selftest_help();
int subcommand_selftest(int argc, char * const argv[]);

static void show_selftest_help()
{
    log_verbose("Usage: %s [OPTIONS] FIRMWARE-IMAGE\n"
                "Loads the given firmware image to RAM, then runs basic device self-tests.\n"
                "By default, all detected devices (in DFU or normal mode) will be tested\n"
                "(use -p to select a single device)\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial <prefix>  specify serial number prefix of ProStick to affect\n"
                " -p, --port <bus-n.n.n> specify connected USB port of ProStick to affect\n"
                " -v, --verbose          enable more logging (breaks output formatting)\n",
                argv0);
}

int subcommand_selftest(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial", required_argument, 0, 's' },
        { "port",   required_argument, 0, 'p' },
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;

    verbose_logging = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:hq", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_selftest_help(argv[0]);
            return EXIT_SUCCESS;

        case 'v':
            verbose_logging = true;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        log_error("a firmware image filename is required");
        return EXIT_FAILURE;
    }

    if (optind + 1 < argc) {
        log_error("only one firmware image filename is expected");
        return EXIT_FAILURE;
    }

    return do_selftest(argv[optind], serial_prefix, port_path) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_selftest(const char *image_path, const char *serial_prefix, const char *port_path)
{
    firmware_io_t *io = NULL;
    firmware_image_t *image = NULL;
    pg2sdr_usb_device **devices = NULL;
    bool success = false;

    if (!(io = io_open_file(image_path)))
        goto cleanup;

    if (!(image = image_read(io)))
        goto cleanup;

    ssize_t device_count;
    if ((device_count = pg2sdr__discover_matching(shared_pg2sdr_ctx, serial_prefix, port_path,
                                                  DEVTYPE_PG2SDR|DEVTYPE_AIRSPYMINI|DEVTYPE_PROTOTYPE|DEVTYPE_RECOVERY,
                                                  &devices)) < 0) {
        log_perror_pg2sdr(device_count, "could not enumerate USB devices");
        goto cleanup;
    }

    if (!device_count) {
        log_error("No matching devices found");
        goto cleanup;
    }

    success = true;
    for (size_t i = 0; i < device_count; ++i) {
        if (!selftest_single_device(devices[i]->lu_device, image))
            success = false;
    }

 cleanup:
    if (devices)
        pg2sdr_free_device_list(devices);
    if (image)
        image_free(image);
    if (io)
        io->close(io);

    return success;
}

typedef struct {
    const char *component;
    const char *checks[10];
} diag_info;

static diag_info DIAG_USB = {
    .component = "USB data path",
    .checks = {
        "Check J1 (USB connector)",
        "Check FL1 (ECMF02 ESD protection)",
    }
};

static diag_info DIAG_U3 = {
    .component = "U3 (R860T tuner)",
    .checks = {
        "Check 3.3V on U3 pin 2",    /* VCC */
        "Check 3.3V on U3 pin 11",   /* also VCC */
        "Check 3.3V on U3 pin 18",   /* also VCC */
        "Check 28.8MHz on U3 pin 9", /* XTAL_O */
        "Check 3.3V on U3 pin 6",    /* SCA, weak pullup when idle */
        "Check 3.3V on U3 pin 7",    /* SDA, weak pullup when idle */
    }
};

static diag_info DIAG_U4 = {
    .component = "U4 (WTL28.8 TCXO)",
    .checks = {
        "Check 3.3V on U4 pin 4",      /* Vdd */
        "Check 28.8MHz on U4 pin 3"    /* OUT */
    }
};

static diag_info DIAG_U5 = {
    .component = "U5 (LPC4370 MCU)",
    .checks = {
        /* do we have accessible test points for these? */
        "Check 3.3V on U5 (what are the test points here?)",
        "Check 5V on U5 pad E3",          /* VBUS */
        "Check 12MHz on U5 pads B1-C1",   /* XTAL1, XTAL2 */
    },
};

static diag_info DIAG_U6 = {
    .component = "U6 (AP7374 regulator for VDD3_RF)",
    .checks = {
        "Check 5V on U6 pin 1",      /* VIN */
        "Check 3.3V on U6 pin 5",    /* EN from LPC4370 */
        "Check 3.3V on U6 pin 3",    /* VOUT */
    },
};

static diag_info DIAG_U7 = {
    .component = "U7 (W25Q80DV flash memory)",
    .checks = {
        "Check 3.3V on U7 pin 8",    /* VCC */
    },
};

static diag_info DIAG_U8 = {
    .component = "U8 (AP7374 regulator for VDD3)",
    .checks = {
        "Check 5V on U8 pin 1",      /* VIN */
        "Check 5V on U8 pin 5",      /* EN */
        "Check 3.3V on U8 pin 3",    /* VOUT */
    },
};

static diag_info DIAG_Y1 = {
    .component = "Y1 (FA-238V 12MHz crystal)",
    .checks = {
        "Check 12MHz on Y1 pins 1-2"
    },
};

typedef bool (*selftest_test_fn)(libusb_device_handle *handled);

typedef struct {
    const char *name;
    selftest_test_fn test;   /* NULL means "load firmware" */
    unsigned require_passed; /* bitmask of previous steps that must have passed */
    diag_info *diags[10];
} selftest_step;

static bool selftest_load_firmware(libusb_device *dev, firmware_image_t *image, libusb_device_handle **handle);
static bool selftest_check_clocks(libusb_device_handle *handle);
static bool selftest_flash(libusb_device_handle *handle);
static bool selftest_tuner_detect(libusb_device_handle *handle);
static bool selftest_tuner_978(libusb_device_handle *handle);
static bool selftest_tuner_1090(libusb_device_handle *handle);
static bool selftest_adc_20(libusb_device_handle *handle);
static bool selftest_adc_19_2(libusb_device_handle *handle);
static bool selftest_adc_16_67(libusb_device_handle *handle);
static bool selftest_cleanup(libusb_device_handle *handle, bool passed);

static selftest_step selftest_steps[] = {
    [0] = {
        .name = "Load firmware",
        .test = NULL, /* special case meaning "load firmware" */
        .diags = { &DIAG_USB, &DIAG_U8, &DIAG_Y1, &DIAG_U5 },
    },

    [1] = {
        .name = "Check LPC4370 clock rates",
        .test = selftest_check_clocks,
        .diags = { &DIAG_U5, &DIAG_Y1 }
    },

    [2] = {
        .name = "Check flash memory I/O",
        .test = selftest_flash,
        .diags = { &DIAG_U7 },
    },

    [3] = {
        .name = "Detect R860T",
        .test = selftest_tuner_detect,
        .diags = { &DIAG_U6, &DIAG_U3, &DIAG_U4 },
    },

    [4] = {
        .name = "Configure R860T PLL for 978MHz",
        .test = selftest_tuner_978,
        .require_passed = (1<<3), /* detect R860T */
        .diags = { &DIAG_U3, &DIAG_U4 },
    },

    [5] = {
        .name = "Configure R860T PLL for 1090MHz",
        .test = selftest_tuner_1090,
        .require_passed = (1<<3), /* detect R860T */
        .diags = { &DIAG_U3, &DIAG_U4 },
    },

    [6] = {
        .name = "Configure LPC4370 HSADC for 19.2 MHz",
        .test = selftest_adc_19_2,
        .diags = { &DIAG_Y1, &DIAG_U5 },
    },

    [7] = {
        .name = "Configure LPC4370 HSADC for 20 MHz",
        .test = selftest_adc_20,
        .diags = { &DIAG_Y1, &DIAG_U5 },
    },

    [8] = {
        .name = "Configure LPC4370 HSADC for 16.67 MHz",
        .test = selftest_adc_16_67,
        .diags = { &DIAG_Y1, &DIAG_U5 },
    },
};
#define NUM_SELFTEST_STEPS (sizeof(selftest_steps) / sizeof(selftest_steps[0]))

static bool selftest_single_device(libusb_device *dev, firmware_image_t *image)
{
    fprintf(stderr, "Selftest for port %s: %s\n", device_ports(dev), device_string(dev));

    bool all_pass = true;
    libusb_device_handle *handle = NULL;

    unsigned passed = 0;
    unsigned failed = 0;
    for (unsigned i = 0; i < NUM_SELFTEST_STEPS; ++i) {
        const selftest_step *step = &selftest_steps[i];
        fprintf(stderr, "  %-40s: ... ", step->name);
        fflush(stderr);

        if ((passed & step->require_passed) != step->require_passed) {
            /* earlier required step failed, skip this step */
            fprintf(stderr, "skipped (previous test failed)\n");
            continue;
        }

        if (step->test && !handle) {
            /* need firmware, but it's not loaded */
            fprintf(stderr, "skipped (unable to load firmware)\n");
            continue;
        }

        bool success;
        if (!step->test) {
            success = selftest_load_firmware(dev, image, &handle); /* null test means "load firmware" */
            if (success)
                dev = libusb_get_device(handle);
        } else {
            success = step->test(handle);
        }

        if (success) {
            passed |= (1 << i);
            fprintf(stderr, "\r  %-40s: pass\n", step->name);
        } else {
            failed |= (1 << i);
            all_pass = false;
            fprintf(stderr, "\r  %-40s: FAILED\n", step->name);
            for (unsigned j = 0; step->diags[j]; ++j) {
                fprintf(stderr, "    %s:\n", step->diags[j]->component);
                for (unsigned k = 0; step->diags[j]->checks[k]; ++k) {
                    fprintf(stderr, "      %s\n", step->diags[j]->checks[k]);
                }
            }
            fprintf(stderr, "\n");
        }
    }

    fprintf(stderr, "Selftest %s for port %s: %s\n", all_pass ? "passed" : "FAILED", device_ports(dev), device_string(dev));

    if (handle) {
        all_pass = selftest_cleanup(handle, all_pass);
        device_close(handle);
    }

    return all_pass;
}

static bool selftest_load_firmware(libusb_device *dev, firmware_image_t *image, libusb_device_handle **handle)
{
    libusb_device *newdev = NULL;
    libusb_device_handle *newhandle = NULL;

    switch (pg2sdr__identify_device(dev)) {
    case DEVTYPE_RECOVERY:
        /* patch boot_mode to indicate use of DFU / recovery mode */
        image_patch_boot_mode(image, BOOT_MODE_RECOVERY);
        if (!dfu_load(image, dev, &newdev))
            goto fail;
        break;
    case DEVTYPE_PG2SDR:
    case DEVTYPE_AIRSPYMINI:
    case DEVTYPE_PROTOTYPE:
        /* patch boot_mode to indicate use of LOAD_IMAGE */
        image_patch_boot_mode(image, BOOT_MODE_LOAD_IMAGE);
        if (!mem_load(image, dev, &newdev))
            goto fail;
        break;
    default:
        log_error("device does not seem to be a ProStick Gen 2");
        goto fail;
    }

    if (!(newhandle = device_open(newdev, true)))
        goto fail;

    int error;
    if ((error = pg2sdr__ctrl_comms_check(newhandle, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "USB comms check failed");
        goto fail;
    }

    /* Set 2Hz blinking yellow (yyy-000-) while we work */
    if ((error = pg2sdr__ctrl_led_pattern(newhandle, 0x3e0, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "LED_PATTERN failed");
        goto fail;
    }

    libusb_unref_device(newdev);
    *handle = newhandle;
    return true;

 fail:
    if (newhandle)
        libusb_close(newhandle);
    if (newdev)
        libusb_unref_device(newdev);

    *handle = NULL;
    return false;
}

static bool selftest_check_clocks(libusb_device_handle *handle)
{
    int error;
    ep0_in_board_status_t status;

    if ((error = pg2sdr__ctrl_get_status(handle, &status, /* measure_clocks */ true, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "GET_STATUS failed");
        return false;
    }

    const double tolerance = 1.015; /* IRC trimmed to 1% per the datasheet, allow up to 1.5% */
    bool okay = true;

    /* IRC - internal RC 12MHz oscillator */
    if (status.clock_irc < (12e6 / tolerance) || status.clock_irc > (12e6 * tolerance)) {
        log_error("Clock frequency outside normal range: expected IRC = 12 MHz, measured %.3f Hz",
                  status.clock_irc/1e6);
        okay = false;
    }

    /* PLL0USB - 480MHz USB clock derived from the external 12MHz crystal */
    if (status.clock_pll0usb < (480e6 / tolerance) || status.clock_pll0usb > (480e6 * tolerance)) {
        log_error("Clock frequency outside normal range: expected PLL0USB = 480 MHz, measured %.3f MHz",
                  status.clock_pll0usb/1e6);
        okay = false;
    }

    /* PLL1 - CPU clock, should be at 24MHz after reset */
    if (status.clock_pll1 < (24e6 / tolerance) || status.clock_pll1 > (24e6 * tolerance)) {
        log_error("Clock frequency outside normal range: expected PLL1 = 24 MHz, measured %.3f Hz",
                  status.clock_pll1 / 1e6);
        okay = false;
    }

    return okay;
}

static bool selftest_flash(libusb_device_handle *handle)
{
    /* test by rewriting the last sector, which shouldn't interfere with any firmware image */
    const unsigned TEST_SECTOR = 0x0FF000;

    int error;
    uint8_t buf[256];

    if ((error = pg2sdr__ctrl_flash_erase(handle, /* sector address */ TEST_SECTOR, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "FLASH_ERASE failed");
        return false;
    }

    if ((error = pg2sdr__ctrl_flash_read_quad(handle, /* page address */ TEST_SECTOR, buf, sizeof(buf), /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "FLASH_READ_QUAD failed");
        return false;
    }

    for (unsigned i = 0; i < sizeof(buf); ++i) {
        if (buf[i] != 0xFF) {
            log_error("Flash read erased sector mismatch at offset 0x%02x: expected 0xFF, got 0x%02x", i, buf[i]);
            return false;
        }
    }

    for (unsigned i = 0; i < sizeof(buf); ++i) {
        buf[i] = (uint8_t)(256-i);
    }

    if ((error = pg2sdr__ctrl_flash_write(handle, /* page address */ TEST_SECTOR, buf, sizeof(buf), /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "FLASH_WRITE failed");
        return false;
    }

    if ((error = pg2sdr__ctrl_flash_read_quad(handle, /* page address */ TEST_SECTOR, buf, sizeof(buf), /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "FLASH_READ_QUAD failed");
        return false;
    }

    for (unsigned i = 0; i < sizeof(buf); ++i) {
        if (buf[i] != (uint8_t)(256-i)) {
            log_error("Flash read test pattern mismatch at offset 0x%02x: expected 0x%02x, got 0x%02x", i, (uint8_t)(256-i), buf[i]);
            return false;
        }
    }

    return true;
}

static bool selftest_tuner_detect(libusb_device_handle *handle)
{
    int error;

    if ((error = pg2sdr__ctrl_set_rf_power(handle, RF_POWER_RESET, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "SET_RF_POWER failed");
        return false;
    }

    uint8_t reg0;
    if ((error = pg2sdr__ctrl_read_tuner_register(handle, /* reg */ 0, CACHE_NORMAL, &reg0, sizeof(reg0), /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "TUNER_READ failed");
        return false;
    }

    if (reg0 != 0x96) {
        log_error("tuner detection failed: expected register 0 to be 0x96, but was 0x%02x", reg0);
        return false;
    }

    return true;
}

static bool selftest_tuner_pll(libusb_device_handle *handle, const uint8_t *regs_and_masks, uint16_t len)
{
    int error;

    /* tuner setup */
    if ((error = pg2sdr__ctrl_tuner_update(handle, /* first */ 5, regs_and_masks, len, /* timeout_ms */ 0))) {
        log_perror_pg2sdr(error, "TUNER_UPDATE failed");
        return false;
    }

    /* wait for PLL lock */
    uint16_t vco_currents[5] = {4, 3, 2, 1, 0};
    for (unsigned i = 0; i < sizeof(vco_currents)/sizeof(vco_currents[0]); i++) {
        error = pg2sdr__ctrl_update_tuner_lock(handle, vco_currents[i], /* lock_timeout_ms */ 50, /* control timeout */ 0);
        if (error < 0) {
            log_perror_pg2sdr(error, "TUNER_LOCK control transfer failed");
            return false;
        }

        if (error > 0)
            break;
    }

    if (error == 0) {
        log_error("Tuner PLL failed to lock");
        return false;
    }

    /* set PLL_AUTO_CLK (R26 bits 3:2) = 2 */
    static uint8_t auto_clk[] = {
        /* R26 value */ 0x08,
        /* R26 mask  */ 0x0C
    };

    if ((error = pg2sdr__ctrl_tuner_update(handle, /* first */ 26, auto_clk, sizeof(auto_clk), /* timeout_ms */ 0))) {
        log_perror_pg2sdr(error, "TUNER_UPDATE (PLL_AUTO_CLK) failed");
        return false;
    }

    /* success */
    return true;
}

static bool selftest_tuner_978(libusb_device_handle *handle)
{
    /* pre-baked tuner config for 978MHz */
    static uint8_t regs_and_masks[27*2] = {
        /* register values */
        0x99,  /* R5  PWD_LT=1             PWD_LNA1=0           LNA_GAIN_MODE=1      LNA_GAIN=9            */
        0x32,  /* R6  PWD_PDET1=0          PWD_PDET2=0          FILT_3DB=1           PW_LNA=2              */
        0x6C,  /* R7  img_r=0              PW_MIX=1             PW0_MIX=1            MIXGAIN_MODE=0       MIX_GAIN=12           */
        0xC0,  /* R8  PW_AMP=1             PW0_AMP=1            imr_g_path=0         IMR_G=0               */
        0x40,  /* R9  PWD_IFFILT=0         PW1_IFFILT=1         imr_p_path=0         IMR_P=0               */
        0xF0,  /* R10 PW_FILT=1            filter_cur=3         iffilt_q=1           iffilt_fine_lpf=0     */
        0x8F,  /* R11 iffilt_narrow=1      iffilt_coarse_lpf=0  calibration_trigger=0 iffilt_hpf_corner=15  */
        0xEF,  /* R12 pwd_adc=1            PW_VGA=1             VGA_GAIN_MODE=0      VGA_GAIN=15           */
        0x53,  /* R13 LNA_VTH_H=5          LNA_VTH_L=3           */
        0x75,  /* R14 MIX_VTH_H=7          MIX_VTH_L=5           */
        0x38,  /* R15 flt_ext_widest=0     clk_out_dis=1        ring_disable=1       clk_agc_dis=0         */
        0x14,  /* R16 SEL_DIV=0            REF_DIV2=1           xtal_drive=0         det1_cap=1           CAPX=0                */
        0x40,  /* R17 PW_LDO_A=1           cp_current=0          */
        0x80,  /* R18 vco_current=4        sdm_dither_dis=0     PWD_SDM=0             */
        0x0D,  /* R19 vco_mode=0           vco_dac=13            */
        0xCD,  /* R20 S_I2C=3              N_I2C=13              */
        0x39,  /* R21 SDM_IN_LSB=57         */
        0x0E,  /* R22 SDM_IN_MSB=14         */
        0xB4,  /* R23 PW_LDO_D=2           div_buf_cur=3        OPEN_D=0              */
        0x40,  /* R24 ring_se23=0          pw_ring=0            ring_n=0              */
        0xCC,  /* R25 PW_RFFILT=1          rffilt_current=2     SW_AGC=0             ring_seldiv=0         */
        0x60,  /* R26 RFMUX=1              agc_clock=2          PLL_AUTO_CLK=0       RFFILT=0              */
        0x00,  /* R27 TF_NCH=0             TF_LP=0               */
        0x54,  /* R28 PDET3_GAIN=5         discharge_mode=1     rf_source=0           */
        0xA6,  /* R29 detect_bw=2          PDET1_GAIN=4         PDET2_GAIN=6          */
        0x0A,  /* R30 sw_pdet=0            FILTER_EXT=0         PDET_CLK=10           */
        0xC0,  /* R31 lt_att=1             ring_att=0            */

        /* register masks (just update everything) */
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF,
    };

    return selftest_tuner_pll(handle, regs_and_masks, sizeof(regs_and_masks));
}

static bool selftest_tuner_1090(libusb_device_handle *handle)
{
    /* pre-baked tuner config for 1090MHz */
    static const uint8_t regs_and_masks[27*2] = {
        /* register values */
        0x99,  /* R5  PWD_LT=1             PWD_LNA1=0           LNA_GAIN_MODE=1      LNA_GAIN=9            */
        0x32,  /* R6  PWD_PDET1=0          PWD_PDET2=0          FILT_3DB=1           PW_LNA=2              */
        0x6C,  /* R7  img_r=0              PW_MIX=1             PW0_MIX=1            MIXGAIN_MODE=0       MIX_GAIN=12           */
        0xC0,  /* R8  PW_AMP=1             PW0_AMP=1            imr_g_path=0         IMR_G=0               */
        0x40,  /* R9  PWD_IFFILT=0         PW1_IFFILT=1         imr_p_path=0         IMR_P=0               */
        0xF0,  /* R10 PW_FILT=1            filter_cur=3         iffilt_q=1           iffilt_fine_lpf=0     */
        0x8F,  /* R11 iffilt_narrow=1      iffilt_coarse_lpf=0  calibration_trigger=0 iffilt_hpf_corner=15  */
        0xEF,  /* R12 pwd_adc=1            PW_VGA=1             VGA_GAIN_MODE=0      VGA_GAIN=15           */
        0x53,  /* R13 LNA_VTH_H=5          LNA_VTH_L=3           */
        0x75,  /* R14 MIX_VTH_H=7          MIX_VTH_L=5           */
        0x38,  /* R15 flt_ext_widest=0     clk_out_dis=1        ring_disable=1       clk_agc_dis=0         */
        0x14,  /* R16 SEL_DIV=0            REF_DIV2=1           xtal_drive=0         det1_cap=1           CAPX=0                */
        0x40,  /* R17 PW_LDO_A=1           cp_current=0          */
        0x80,  /* R18 vco_current=4        sdm_dither_dis=0     PWD_SDM=0             */
        0x14,  /* R19 vco_mode=0           vco_dac=20            */
        0x8F,  /* R20 S_I2C=2              N_I2C=15              */
        0x55,  /* R21 SDM_IN_LSB=85         */
        0xD5,  /* R22 SDM_IN_MSB=213        */
        0xB4,  /* R23 PW_LDO_D=2           div_buf_cur=3        OPEN_D=0              */
        0x40,  /* R24 ring_se23=0          pw_ring=0            ring_n=0              */
        0xCC,  /* R25 PW_RFFILT=1          rffilt_current=2     SW_AGC=0             ring_seldiv=0         */
        0x60,  /* R26 RFMUX=1              agc_clock=2          PLL_AUTO_CLK=0       RFFILT=0              */
        0x00,  /* R27 TF_NCH=0             TF_LP=0               */
        0x54,  /* R28 PDET3_GAIN=5         discharge_mode=1     rf_source=0           */
        0xA6,  /* R29 detect_bw=2          PDET1_GAIN=4         PDET2_GAIN=6          */
        0x0A,  /* R30 sw_pdet=0            FILTER_EXT=0         PDET_CLK=10           */
        0xC0,  /* R31 lt_att=1             ring_att=0            */

        /* register masks (just update everything) */
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF,
    };

    return selftest_tuner_pll(handle, regs_and_masks, sizeof(regs_and_masks));
}

static bool selftest_adc_rate(libusb_device_handle *handle, const ep0_out_start_transfer_t *config, double expected_rate)
{
    int error;
    if ((error = pg2sdr__ctrl_start_transfer(handle, config, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "START_TRANSFER(%.3fMHz) failed", expected_rate/1e6);
        return false;
    }

    /* measure actual clock rate from PLL0AUDIO (IDIV_E is not used in this config) */
    ep0_in_board_status_t status;
    if ((error = pg2sdr__ctrl_get_status(handle, &status, /* measure_clocks */ true, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "GET_STATUS failed");
        return false;
    }

    const double tolerance = 1.015; /* IRC trimmed to 1% per the datasheet, allow up to 1.5% */
    const char *clocksource;
    double measured;
    if (config->idiv_divisor == 0) {
        /* not using IDIV_E */
        measured = status.clock_pll0audio;
        clocksource = "PLL0AUDIO";
    } else {
        /* using IDIV_E */
        measured = status.clock_idiv_e;
        clocksource = "IDIV_E";
    }

    if (measured < expected_rate / tolerance || measured > expected_rate * tolerance) {
        log_error("ADC clock frequency out of range: expected %s = %.3f MHz, measured %.3f MHz",
                  clocksource, expected_rate/1e6, measured / 1e6);
        return false;
    }

    return true;
}

static bool selftest_adc_20(libusb_device_handle *handle)
{
    /* precalculated ADC settings for 20MHz:
     *   N=0
     *   M=15.0 (integer mode)
     *   P=9
     *   I=0
     * =>
     *   fCCO = 360MHz
     *   fADC = 20MHz
     */
    static ep0_out_start_transfer_t config = {
        .n_divisor = 0,
        .m_divisor = (15 << 15),   /* 15.0, 15-bit fixed-point */
        .p_divisor = 9,
        .idiv_divisor = 0
    };

    return selftest_adc_rate(handle, &config, 20e6);
}

static bool selftest_adc_19_2(libusb_device_handle *handle)
{
    /* precalculated ADC settings for 19.2MHz:
     *   N=0
     *   M=12.0 (integer mode)
     *   P=0
     *   I=15
     * =>
     *   fCCO = 288MHz
     *   fADC = 19.2MHz
     */
    static ep0_out_start_transfer_t config = {
        .n_divisor = 0,
        .m_divisor = (12 << 15),   /* 12.0, 15-bit fixed-point */
        .p_divisor = 0,
        .idiv_divisor = 15
    };

    return selftest_adc_rate(handle, &config, 19.2e6);
}

static bool selftest_adc_16_67(libusb_device_handle *handle)
{
    /* precalculated ADC settings for 16.666667MHz (for UAT):
     *   N=2
     *   M=25.0 (integer mode)
     *   P=9
     *   I=0
     * =>
     *   fCCO = 300.0MHz
     *   fADC = 16.67MHz
     */
    static ep0_out_start_transfer_t config = {
        .n_divisor = 2,
        .m_divisor = (25 << 15),   /* 12.0, 15-bit fixed-point */
        .p_divisor = 9,
        .idiv_divisor = 0
    };

    return selftest_adc_rate(handle, &config, 16.666667e6);
}

static bool selftest_cleanup(libusb_device_handle *handle, bool passed)
{
    int error;

    /* if we passed so far, turn off ADC and RF power */
    if (passed) {
        if ((error = pg2sdr__ctrl_stop_transfer(handle, /* timeout_ms */ 0)) < 0) {
            log_perror_pg2sdr(error, "STOP_TRANSFER failed");
            passed = false;
        }

        if ((error = pg2sdr__ctrl_set_rf_power(handle, RF_POWER_OFF, /* timeout_ms */ 0)) < 0) {
            log_perror_pg2sdr(error, "SET_RF_POWER failed");
            passed = false;
        }
    }

    /* set LED blink pattern:
     *   success: 37bdfbde: yyg-yyg-yyg-ygy-ygy-ygy- (0.7Hz blink, yellow-green)
     *   failed:  2a0:      yrr-000-                 (2Hz blink, red)
     */
    uint32_t pattern = passed ? 0x35ad7fff : 0x2a0;
    if ((error = pg2sdr__ctrl_led_pattern(handle, pattern, /* timeout_ms */ 0)) < 0) {
        log_perror_pg2sdr(error, "LED_PATTERN failed");
        passed = false;
    }

    return passed;
}
