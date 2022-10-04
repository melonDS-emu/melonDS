#ifndef DUCKSTATION_COMPAT_H
#define DUCKSTATION_COMPAT_H

#include "../types.h"

#include <assert.h>

#define ALWAYS_INLINE __attribute__((always_inline)) inline

#define AssertMsg(cond, msg) assert(cond && msg)

#endif