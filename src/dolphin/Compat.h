// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license_dolphin.txt file included.

// Stubs for Assert.h and Log.h
#pragma once

#include <assert.h>

// Assert stub
#define ASSERT_MSG(_t_, _a_, _fmt_, ...)                                                           \
  assert(_a_) \
  /*do                                                                                               \
  {                                                                                                \
    if (!(_a_))                                                                                    \
    {                                                                                              \
      if (!PanicYesNo(_fmt_, ##__VA_ARGS__))                                                       \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)*/

#define DEBUG_ASSERT_MSG(_t_, _a_, _msg_, ...)                                                     \
  assert(_a_); \
  /*do                                                                                               \
  {                                                                                                \
    if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG && !(_a_))                                    \
    {                                                                                              \
      ERROR_LOG(_t_, _msg_, ##__VA_ARGS__);                                                        \
      if (!PanicYesNo(_msg_, ##__VA_ARGS__))                                                       \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)*/

#define ASSERT(_a_)                                                                                \
  assert(_a_) \
  /*do                                                                                               \
  {                                                                                                \
    ASSERT_MSG(MASTER_LOG, _a_,                                                                    \
               _trans("An error occurred.\n\n  Line: %d\n  File: %s\n\nIgnore and continue?"),     \
               __LINE__, __FILE__);                                                                \
  } while (0)*/

#define DEBUG_ASSERT(_a_)                                                                          \
  assert(_a_) \
  /*do                                                                                               \
  {                                                                                                \
    if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)                                              \
      ASSERT(_a_);                                                                                 \
  } while (0)*/

// Log Stub
#include <cstdio>

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

#if __cplusplus < 201703L
// cheat
namespace std
{
template <typename T>
T clamp(const T& v, const T& lo, const T& hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}
}
#endif