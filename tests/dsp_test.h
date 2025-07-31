#include <gtest/gtest.h>
using namespace std;
using namespace testing;
#include <iostream>
#include <fstream>
#include <string>

extern "C" {
    #include "internal.h"
}

string print_cs16_t(cs16_t v);
AssertionResult AssertionSuccess();
AssertionResult AssertionFailure();
AssertionResult Succeeded(cs16_t value, cs16_t expected);