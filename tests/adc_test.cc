#include <gtest/gtest.h>

extern "C" { /* Because our code is in C */
  #include "internal.h"
}

/* Tests on utils max() function */
TEST (ADCTest, Test) {
  EXPECT_EQ (2, 2);
  EXPECT_EQ (1, 2);
//   EXPECT_NEQ();
}