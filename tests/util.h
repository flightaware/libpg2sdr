#include <gtest/gtest.h>
using namespace std;

extern "C" {
    #include "internal.h"
}

int initialize_handle(lpcsdr_device_handle **handle);