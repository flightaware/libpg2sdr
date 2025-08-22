#include "dsp_test.h"

string print_cs16_t(cs16_t v) {
    std::stringstream ss;
    ss << "real: " << v.i << ",imag: " << v.q;
    return ss.str();
}

AssertionResult Succeeded(cs16_t value, cs16_t expected) {
    if (value.i == expected.i && value.q == expected.q)
        return testing::AssertionSuccess();
    else {
        return testing::AssertionFailure() << "Got cs16_t " << print_cs16_t(value) << ". Expected " << print_cs16_t(expected);
    }
}

TEST(DSPTEST_for_dev, Test_process_cs16) {
    cout << "ji \n\n";
    ifstream ifs("../../test_files/inputs/default-test-case-input.tsv");
    string line;

    uint32_t num_lines = 1926664;
    int16_t *buffer = (int16_t *) calloc(num_lines, sizeof(int16_t));
    cs16_t *out;
    uint32_t index = 0;
    while (getline(ifs, line)) {
        std::stringstream ss(line);
        // cout << line << "\n\n";
		std::string tmp;
        getline(ss, tmp, '\t');
        getline(ss, tmp, '\t');
        buffer[index++] = stoi(tmp);
	}

    lpcsdr_decimate *default_filter;

    uint32_t usb_samples_per_block = 13616/2;
    uint32_t required_samples = 960000;
    
    assert(lpcsdr_dsp_decimate_create(lpcsdr_standard_filter_ntaps, lpcsdr_standard_filter_taps, &default_filter) == LPCSDR_SUCCESS);
    
    lpcsdr_decimate_complex_baseband(default_filter, usb_samples_per_block, buffer, num_lines, &out, required_samples, "../../test_files/outputs/default-test-case-output.tsv");
}

TEST(DSPTEST_for_dev_scaled, Test_process_cs16) {
    cout << "ji \n\n";
    ifstream ifs("../../test_files/inputs/scaled.tsv");
    string line;

    uint32_t num_lines = 1926664;
    int16_t *buffer = (int16_t *) calloc(num_lines, sizeof(int16_t));
    cs16_t *out;
    uint32_t index = 0;
    while (getline(ifs, line)) {
        std::stringstream ss(line);
        // cout << line << "\n\n";
		std::string tmp;
        getline(ss, tmp, '\t');
        getline(ss, tmp, '\t');
        buffer[index++] = stoi(tmp);
	}

    lpcsdr_decimate *default_filter;

    uint32_t usb_samples_per_block = 13616/2;
    uint32_t required_samples = 960000;
    
    assert(lpcsdr_dsp_decimate_create(lpcsdr_standard_filter_ntaps, lpcsdr_standard_filter_taps, &default_filter) == LPCSDR_SUCCESS);
    
    lpcsdr_decimate_complex_baseband(default_filter, usb_samples_per_block, buffer, num_lines, &out, required_samples, "../../test_files/outputs/default-test-case-scaled.tsv");
}

TEST(Test_process_cs16, history_available_less_than_ntaps) {
    cs16_t history[20] = {
        {
            .i = 1,
            .q = 1,
        }
    };
    unsigned int history_len = 1;
    int16_t taps[10] = {1,2,3,4,5,6,7,8,9,10};
    lpcsdr_decimate d = {
        .ntaps = 10,
        .taps = taps,
        .history = history,
        .history_max = 20,
        .history_len = history_len,
    };

    cs16_t in[5] = {{.i = 1, .q = 1}, {.i = 1, .q = 1}, {.i = 1, .q = 1}, {.i = 1, .q = 1}, {.i = 1, .q = 1}};
    cs16_t out[5] = {};
    int count = sizeof(in)/sizeof(in[0]);
    int return_count = process_cs16(&d, in, out, count);
    ASSERT_EQ(return_count, 0);
    ASSERT_EQ(d.history_len, history_len + count);
}

TEST(Test_process_cs16, history_processed_less_than_history_len) {
    cs16_t history[20] = {
        {
            .i = 3276,
            .q = 3276,
        },
        {
            .i = 6553,
            .q = 6553,
        },
        {
            .i = 9830,
            .q = 9830,
        },
        {
            .i = 13107,
            .q = 13107,
        },
        {
            .i = 16384,
            .q = 16384,
        },
    };
    unsigned int history_len = 5;
    int16_t taps[3] = {10,11,12};
    lpcsdr_decimate d = {
        .ntaps = 3,
        .taps = taps,
        .history = history,
        .history_max = 20,
        .history_len = history_len,
    };

    cs16_t in[1] = {{
        .i = 19660,
        .q = 19660,
    }};
    cs16_t out[5] = {};
    int count = sizeof(in)/sizeof(in[0]);

    float expected_values[3] = {6, 13};
    int expected_return_count = 2;
    int expected_history_length = 2;
    int return_count = process_cs16(&d, in, out, count);
    ASSERT_EQ(return_count, expected_return_count);
    ASSERT_EQ(d.history_len, expected_history_length);


    for (int index = 0; expected_return_count < 2; index++) {
        ASSERT_EQ(out[index].i, expected_values[index]);
        ASSERT_EQ(out[index].q, expected_values[index]);
    }
}

TEST(Test_process_cs16, process_main_block) {
    cs16_t history[20] = {
        {
            .i = 3276,
            .q = 3276,
        },
        {
            .i = 6553,
            .q = 6553,
        },
    };
    unsigned int history_len = 2;
    unsigned int ntaps = 2;
    int16_t taps[ntaps] = {10,11};
    lpcsdr_decimate d = {
        .ntaps = ntaps,
        .taps = taps,
        .history = history,
        .history_max = 4,
        .history_len = history_len,
    };

    int count = 6;
    cs16_t in[count] = {
        {
            .i = 9830,
            .q = 9830,
        },
        {
            .i = 13107,
            .q = 13107,
        },
        {
            .i = 16384,
            .q = 16384,
        },
        {
            .i = 19660,
            .q = 19660,
        },
        {
            .i = 22932,
            .q = 22932,
        },
        {
            .i = 26208,
            .q = 26208,
        },
    };
    cs16_t out[5] = {};

    int expected_return_count = 3;
    int expected_history_length = 2;
    float expected_values[expected_return_count] = {3, 7, 11};

    int return_count = process_cs16(&d, in, out, count);
    ASSERT_EQ(return_count, expected_return_count);
    ASSERT_EQ(d.history_len, expected_history_length);


    for (int index = 0; index < expected_return_count; index++) {
        ASSERT_EQ(out[index].i, expected_values[index]);
        ASSERT_EQ(out[index].q, expected_values[index]);
    }

    EXPECT_TRUE(Succeeded(in[4], d.history[0]));
    EXPECT_TRUE(Succeeded(in[5], d.history[1]));

}


TEST(DSP_decimate_cs16, successful) {
    const unsigned int ntaps = 2;
    const int16_t taps[2] = {10,11};
    cs16_t in[6] = {
        {
            .i = 3276,
            .q = 3276,
        },
        {
            .i = 6553,
            .q = 6553,
        },
        {
            .i = 9830,
            .q = 9830,
        },
        {
            .i = 13107,
            .q = 13107,
        },
        {
            .i = 16384,
            .q = 16384,
        },
        {
            .i = 19660,
            .q = 19660,
        }
    };

    uint16_t expected_return_length = 3;
    cs16_t out[3];

    uint16_t return_index = decimate_cs16(ntaps, taps, in, out, sizeof(in)/sizeof(in[0]));
    ASSERT_EQ(return_index, expected_return_length);
    
    float expected_values[3] = {3, 7, 11};

    for (int index = 0; index < expected_return_length; index++) {
        ASSERT_EQ(out[index].i, expected_values[index]);
        ASSERT_EQ(out[index].q, expected_values[index]);
    }
}

TEST(Test_downmix_samples, Successful) {
    int16_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    cs16_t out[8];

    ASSERT_EQ(downmix_samples(4, in, out, 8), LPCSDR_SUCCESS);

    cs16_t expected_out[8] = {
        {
            .i = 1,
            .q = 0
        },
        {
            .i = 0,
            .q = 2
        },
        {
            .i = -3,
            .q = 0
        },
        {
            .i = 0,
            .q = -4
        },
        {
            .i = 5,
            .q = 0
        },
        {
            .i = 0,
            .q = 6
        },
        {
            .i = -7,
            .q = 0
        },
        {
            .i = 0,
            .q = -8
        }
    };

    for (int o = 0; o < 8; o++) {
        EXPECT_TRUE(Succeeded(out[o], expected_out[o]));
    }
}