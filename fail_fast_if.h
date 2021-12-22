#pragma once

#include <stdio.h>

#define FAIL_FAST_IF(expr)                                                                   \
  if ((expr)) {                                                                              \
    printf("[lttng-go] Fatal error:\n\texpr: %s\n\tfile: %s\n\tline: %d\n", #expr, __FILE__, \
           __LINE__);                                                                        \
    quick_exit(1);                                                                           \
  }
