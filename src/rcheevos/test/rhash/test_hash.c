#include "rc_hash.h"

#include "../rhash/rc_hash_internal.h"

#include "../rc_compat.h"
#include "../test_framework.h"
#include "data.h"
#include "mock_filereader.h"

#include <stdlib.h>

/* ========================================================================= */

void test_hash_full_file(uint32_t console_id, const char* filename, size_t size, const char* expected_md5)
{
  uint8_t* image = generate_generic_file(size);
  char hash_buffer[33], hash_file[33], hash_iterator[33];
  mock_file(0, filename, image, size);

  /* test full buffer hash */
  int result_buffer = rc_hash_generate_from_buffer(hash_buffer, console_id, image, size);

  /* test full file hash */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_buffer, 1);
  ASSERT_STR_EQUALS(hash_buffer, expected_md5);

  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

void test_hash_m3u(uint32_t console_id, const char* filename, size_t size, const char* expected_md5)
{
  uint8_t* image = generate_generic_file(size);
  char hash_file[33], hash_iterator[33];
  const char* m3u_filename = "test.m3u";

  mock_file(0, filename, image, size);
  mock_file(1, m3u_filename, (uint8_t*)filename, strlen(filename));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void assert_valid_m3u(const char* disc_filename, const char* m3u_filename, const char* m3u_contents)
{
  const size_t size = 131072;
  uint8_t* image = generate_generic_file(size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "a0f425b23200568132ba76b2405e3933";

  mock_file(0, disc_filename, image, size);
  mock_file(1, m3u_filename, (uint8_t*)m3u_contents, strlen(m3u_contents));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC8800, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_m3u_buffered()
{
  const size_t size = 131072;
  uint8_t* image = generate_generic_file(size);
  char hash_iterator[33];
  const char* m3u_filename = "test.m3u";
  const char* filename = "test.d88";
  const char* expected_md5 = "a0f425b23200568132ba76b2405e3933";
  uint8_t* m3u_contents = (uint8_t*)filename;
  const size_t m3u_size = strlen(filename);

  mock_file(0, filename, image, size);
  mock_file(1, m3u_filename, m3u_contents, m3u_size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, m3u_contents, m3u_size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_m3u_with_comments()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\ntest.d88\r\n");
}

static void test_hash_m3u_empty()
{
  char hash_file[33], hash_iterator[33];
  const char* m3u_filename = "test.m3u";
  const char* m3u_contents = "#EXTM3U\r\n\r\n#EXTBYT:131072\r\n";

  mock_file(0, m3u_filename, (uint8_t*)m3u_contents, strlen(m3u_contents));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC8800, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_m3u_trailing_whitespace()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U  \r\n  \r\n#EXTBYT:131072  \r\ntest.d88  \t  \r\n");
}

static void test_hash_m3u_line_ending()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U\n\n#EXTBYT:131072\ntest.d88\n");
}

static void test_hash_m3u_extension_case()
{
  assert_valid_m3u("test.D88", "test.M3U", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\ntest.D88\r\n");
}

static void test_hash_m3u_relative_path()
{
  assert_valid_m3u("folder1/folder2/test.d88", "folder1/test.m3u", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\nfolder2/test.d88");
}

static void test_hash_m3u_absolute_path(const char* absolute_path)
{
  char m3u_contents[128];
  snprintf(m3u_contents, sizeof(m3u_contents), "#EXTM3U\r\n\r\n#EXTBYT:131072\r\n%s", absolute_path);

  assert_valid_m3u(absolute_path, "relative/test.m3u", m3u_contents);
}

#ifndef RC_HASH_NO_ROM
uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size);

static void test_hash_file_without_ext()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* filename = "test";

  mock_file(0, filename, image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO, filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */

  /* specifying a console will use the appropriate hasher */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, "6a2305a2b6675a97ff792709be1ca857");

  /* no extension will use the default full file iterator, so hash should include header */
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, "64b131c5c7fec32985d9c99700babb7e");
}
#endif

static void test_hash_handler_table_order()
{
  size_t num_handlers;
  const rc_hash_iterator_ext_handler_entry_t* handlers = rc_hash_get_iterator_ext_handlers(&num_handlers);
  int index;

  for (index = 1; index < num_handlers; ++index) {
    if (strcmp(handlers[index].ext, handlers[index - 1].ext) <= 0) {
      ASSERT_FAIL("handler[%s] after handler[%s]", handlers[index].ext, handlers[index - 1].ext);
    }
  }
}

/* ========================================================================= */

void test_hash(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();

  /* Amstrad CPC */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_AMSTRAD_PC, "test.dsk", 194816, "9d616e4ad3f16966f61422c57e22aadd");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_AMSTRAD_PC, "test.dsk", 194816, "9d616e4ad3f16966f61422c57e22aadd");

  /* Apple II */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.nib", 232960, "96e8d33bdc385fd494327d6e6791cbe4");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");

  /* Commodore 64 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COMMODORE_64, "test.nib", 327936, "e7767d32b23e3fa62c5a250a08caeba3");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COMMODORE_64, "test.d64", 174848, "ecd5a8ef4e77f2e9469d9b6e891394f0");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_COMMODORE_64, "test.d64", 174848, "ecd5a8ef4e77f2e9469d9b6e891394f0");

  /* MSX */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");

  /* PC-8800 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");

  /* ZX Spectrum */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ZX_SPECTRUM, "test.tap", 1596, "714a9f455e616813dd5421c5b347e5e5");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ZX_SPECTRUM, "test.tzx", 14971, "93723e6d1100f9d1d448a27cf6618c47");

  /* m3u support */
  TEST(test_hash_m3u_buffered);
  TEST(test_hash_m3u_with_comments);
  TEST(test_hash_m3u_empty);
  TEST(test_hash_m3u_trailing_whitespace);
  TEST(test_hash_m3u_line_ending);
  TEST(test_hash_m3u_extension_case);
  TEST(test_hash_m3u_relative_path);
  TEST_PARAMS1(test_hash_m3u_absolute_path, "/absolute/test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "C:\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "\\\\server\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "samba:/absolute/test.d88");

  /* other */
#ifndef RC_HASH_NO_ROM
  TEST(test_hash_file_without_ext);
#endif
  TEST(test_hash_handler_table_order);

  TEST_SUITE_END();
}
