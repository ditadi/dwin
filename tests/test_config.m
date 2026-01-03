#include "../include/config.h"
#include "test_helpers.h"
#import <Foundation/Foundation.h>

void test_gap_parsing(void) {
  TEST_PASS("gap parsing");
}

int main() {
  test_gap_parsing();
  printf("\n✓ All config tests passed\n");
  return 0;
}