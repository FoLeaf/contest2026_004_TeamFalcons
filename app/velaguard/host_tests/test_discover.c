#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vg_discover.h"

static int expect_true(int cond, const char *msg)
{
  if (!cond)
    {
      fprintf(stderr, "FAIL: %s\n", msg);
      return 1;
    }

  return 0;
}

int main(void)
{
  int fails = 0;
  int amin;
  int amax;
  float v;

  fails += expect_true(vg_discover_parse_addr_range("1-8", &amin, &amax),
                       "parse 1-8");
  fails += expect_true(amin == 1 && amax == 8, "range 1-8 values");

  fails += expect_true(vg_discover_parse_addr_range("3", &amin, &amax),
                       "parse single");
  fails += expect_true(amin == 3 && amax == 3, "single addr");

  fails += expect_true(!vg_discover_parse_addr_range("0-1", &amin, &amax),
                       "reject addr 0");

  v = vg_discover_decode_int16_scaled(1000, 0.1f);
  fails += expect_true(v >= 99.9f && v <= 100.1f, "decode 1000*0.1");

  if (fails == 0)
    {
      printf("test_discover: OK\n");
      return 0;
    }

  fprintf(stderr, "test_discover: %d failure(s)\n", fails);
  return 1;
}
