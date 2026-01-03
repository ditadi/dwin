#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(condition, message)                                             \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "Assertion failed: %s\n", message);                      \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b, message)                                               \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      fprintf(stderr, "Assertion failed: %s\n", message);                      \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define TEST_PASS(name) printf("✓ %s passed\n", name);

#endif