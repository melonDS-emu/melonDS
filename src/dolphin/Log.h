#pragma once

#include "CommonFuncs.h"

#include <stdio.h>

#define PanicAlert(fmt, ...) \
  do \
  { \
    printf(fmt "\n", ## __VA_ARGS__); \
    abort(); \
  } while (false)


#define DYNA_REC 0

#define ERROR_LOG(which, fmt, ...) \
    do \
    { \
        printf(fmt "\n", ## __VA_ARGS__); \
    } while (false)
