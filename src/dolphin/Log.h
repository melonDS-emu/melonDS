#pragma once

#include "CommonFuncs.h"

#include <stdio.h>

#define PanicAlert(msg) \
    do \
    { \
        printf("%s\n", msg); \
        Crash(); \
    } while (false)

#define DYNA_REC 0

#define ERROR_LOG(which, fmt, ...) \
    do \
    { \
        printf(fmt "\n", ## __VA_ARGS__); \
    } while (false)
