#include "rc_internal.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
#include "rc_client_raintegration.h"
#endif

#include "test_framework.h"

#include <assert.h>

#define TIMING_TEST 0

extern void test_timing();

extern void test_condition();
extern void test_memref();
extern void test_operand();
extern void test_condset();
extern void test_trigger();
extern void test_value();
extern void test_format();
extern void test_lboard();
extern void test_richpresence();
extern void test_runtime();
extern void test_runtime_progress();

extern void test_client();
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
extern void test_client_external();
#endif
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
extern void test_client_raintegration();
#endif

extern void test_consoleinfo();
extern void test_rc_libretro();
extern void test_rc_validate();

extern void test_hash();
#ifndef RC_HASH_NO_ROM
extern void test_hash_rom();
#endif
#ifndef RC_HASH_NO_DISC
extern void test_cdreader();
extern void test_hash_disc();
#endif
#ifndef RC_HASH_NO_ZIP
extern void test_hash_zip();
#endif

extern void test_rapi_common();
extern void test_rapi_user();
extern void test_rapi_runtime();
extern void test_rapi_info();
extern void test_rapi_editor();

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

#if TIMING_TEST
  test_timing();
#else
  test_memref();
  test_operand();
  test_condition();
  test_condset();
  test_trigger();
  test_value();
  test_format();
  test_lboard();
  test_richpresence();
  test_runtime();
  test_runtime_progress();

  test_consoleinfo();
  test_rc_validate();

  test_rapi_common();
  test_rapi_user();
  test_rapi_runtime();
  test_rapi_info();
  test_rapi_editor();

  test_client();
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  test_client_external();
#endif
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  test_client_raintegration();
#endif
#ifdef RC_CLIENT_SUPPORTS_HASH
  /* no direct compile option for hash support, so leverage RC_CLIENT_SUPPORTS_HASH */
  test_rc_libretro(); /* libretro extensions require hash support */
  test_hash();
 #ifndef RC_HASH_NO_ROM
  test_hash_rom();
 #endif
 #ifndef RC_HASH_NO_DISC
  test_cdreader();
  test_hash_disc();
 #endif
 #ifndef RC_HASH_NO_ZIP
  test_hash_zip();
 #endif
#endif
#endif

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
