#include "rc_hash.h"

#include "../rhash/rc_hash_internal.h"

#include "../rc_compat.h"
#include "../test_framework.h"
#include "data.h"
#include "mock_filereader.h"

#include <stdlib.h>

/* in test_hash.c */
void test_hash_full_file(uint32_t console_id, const char* filename, size_t size, const char* expected_md5);
void test_hash_m3u(uint32_t console_id, const char* filename, size_t size, const char* expected_md5);

/* ========================================================================= */

static void test_hash_unknown_format(uint32_t console_id, const char* path)
{
  char hash_file[33] = "", hash_iterator[33] = "";

  /* test file hash (won't match) */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, path);

  /* test file identification from iterator (won't match) */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, path, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_STR_EQUALS(hash_file, "");

  ASSERT_NUM_EQUALS(result_iterator, 0);
  ASSERT_STR_EQUALS(hash_iterator, "");
}

/* ========================================================================= */

static void test_hash_3do_bin()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 123456, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "9b2266b8f5abed9c12cce780750e88d6";

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  mock_file_size(0, 45678901); /* must be > 32MB for iterator to consider CD formats for bin */
  rc_hash_initialize_iterator(&iterator, "game.bin", NULL, 0);
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

static void test_hash_3do_cue()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 9347, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "257d1d19365a864266b236214dbea29c";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_3do_iso()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 9347, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "257d1d19365a864266b236214dbea29c";

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
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

static void test_hash_3do_invalid_header()
{
  /* this is meant to simulate attempting to open a non-3DO CD. TODO: generate PSX CD */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 12, &image_size);
  char hash_file[33];

  /* make the header not match */
  image[3] = 0x34;

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
}

static void test_hash_3do_launchme_case_insensitive()
{
  /* main executable for "Captain Quazar" is "launchme" */
  /* main executable for "Rise of the Robots" is "launchMe" */
  /* main executable for "Road Rash" is "LaunchMe" */
  /* main executable for "Sewer Shark" is "Launchme" */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 6543, &image_size);
  char hash_file[33];
  const char* expected_md5 = "59622882e3261237e8a1e396825ae4f5";

  memcpy(&image[2048 + 0x14 + 0x48 + 0x20], "launchme", 8);
  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
}

static void test_hash_3do_no_launchme()
{
  /* this case should not happen */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 6543, &image_size);
  char hash_file[33];

  memcpy(&image[2048 + 0x14 + 0x48 + 0x20], "filename", 8);
  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
}

static void test_hash_3do_long_directory()
{
  /* root directory for "Dragon's Lair" uses more than one sector */
  size_t image_size;
  uint8_t* image = generate_3do_bin(3, 6543, &image_size);
  char hash_file[33];
  const char* expected_md5 = "8979e876ae502e0f79218f7ff7bd8c2a";

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
}

/* ========================================================================= */

static void test_hash_atari_jaguar_cd()
{
  const char* cue_file =
      "REM SESSION 01\n"
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "REM SESSION 02\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 AUDIO\n"
      "    INDEX 01 00:00:00\n";
  size_t image_size;
  uint8_t* image = generate_jaguarcd_bin(2, 60024, 0, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "c324d95dc5831c2d5c470eefb18c346b";

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track02.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual FIRST_OF_SECOND_SESSION calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ATARI_JAGUAR_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_atari_jaguar_cd_byteswapped()
{
  const char* cue_file =
      "REM SESSION 01\n"
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "REM SESSION 02\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 AUDIO\n"
      "    INDEX 01 00:00:00\n";
  size_t image_size;
  uint8_t* image = generate_jaguarcd_bin(2, 60024, 1, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "c324d95dc5831c2d5c470eefb18c346b";

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track02.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual FIRST_OF_SECOND_SESSION calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ATARI_JAGUAR_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_atari_jaguar_cd_track3()
{
  const char* cue_file =
      "REM SESSION 01\n"
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "REM SESSION 02\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 AUDIO\n"
      "    INDEX 01 00:00:00\n";
  size_t image_size;
  uint8_t* image = generate_jaguarcd_bin(1470, 99200, 1, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "060e9d223c584b581cf7d7ce17c0e5dc";

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track03.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual FIRST_OF_SECOND_SESSION calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ATARI_JAGUAR_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_atari_jaguar_cd_no_header()
{
  const char* cue_file =
      "REM SESSION 01\n"
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "REM SESSION 02\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 AUDIO\n"
      "    INDEX 01 00:00:00\n";
  size_t image_size;
  uint8_t* image = generate_jaguarcd_bin(2, 32768, 1, &image_size);
  char hash_file[33], hash_iterator[33];

  image[2 + 64 + 12] = 'B'; /* corrupt the header */

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track02.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual FIRST_OF_SECOND_SESSION calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ATARI_JAGUAR_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_atari_jaguar_cd_no_sessions()
{
  const char* cue_file =
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 AUDIO\n"
      "    INDEX 01 00:00:00\n";
  size_t image_size;
  uint8_t* image = generate_jaguarcd_bin(2, 99200, 1, &image_size);
  char hash_file[33], hash_iterator[33];

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track03.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual FIRST_OF_SECOND_SESSION calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ATARI_JAGUAR_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

extern const char* _rc_hash_jaguar_cd_homebrew_hash;

static void test_hash_atari_jaguar_cd_homebrew()
{
  /* Jaguar CD homebrew games all appear to have a common bootloader in the primary boot executable space. They only
   * differ in a secondary executable in the second track (part of the first session). This doesn't appear to be
   * intentional behavior based on the CD BIOS documentation, which states that all developer code should be in the
   * first track of the second session. I speculate this is done to work around the authentication logic. */
  const char* cue_file =
      "REM SESSION 01\n"
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 01 00:00:00\n"
      "REM SESSION 02\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 AUDIO\n"
      "    INDEX 01 00:00:00\n";
  size_t image_size, image_size2;
  uint8_t* image = generate_jaguarcd_bin(2, 45760, 1, &image_size);
  uint8_t* image2 = generate_jaguarcd_bin(2, 986742, 1, &image_size2);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "3fdf70e362c845524c9e447aacaed0a9";

  image2[0x60] = 0x21; /* ATARI APPROVED DATA HEADER ATRI! */
  memcpy(&image2[0xA2], &image2[0x62], 8); /* addr / size */
  memcpy(&image2[0x62], "RTKARTKARTKARTKA", 16); /* KARTKARTKARTKART */
  memcpy(&image2[0x72], "RTKARTKARTKARTKA", 16);
  memcpy(&image2[0x82], "RTKARTKARTKARTKA", 16);
  memcpy(&image2[0x92], "RTKARTKARTKARTKA", 16);

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(2, "track02.bin", image2, image_size2);
  mock_file(1, "track03.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual FIRST_OF_SECOND_SESSION calculation */
  _rc_hash_jaguar_cd_homebrew_hash = "4e4114b2675eff21bb77dd41e141ddd6"; /* mock the hash of the homebrew bootloader */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ATARI_JAGUAR_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  _rc_hash_jaguar_cd_homebrew_hash = NULL;
  free(image);
  free(image2);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_dreamcast_single_bin()
{
  size_t image_size;
  uint8_t* image = generate_dreamcast_bin(45000, 1458208, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "2a550500caee9f06e5d061fe10a46f6e";

  mock_file(0, "track03.bin", image, image_size);
  mock_file_first_sector(0, 45000);
  mock_file(1, "game.gdi", (uint8_t*)"game.bin", 8);
  mock_cd_num_tracks(3);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_DREAMCAST, "game.gdi");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.gdi", NULL, 0);
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

static void test_hash_dreamcast_split_bin()
{
  size_t image_size;
  uint8_t* image = generate_dreamcast_bin(548106, 1830912, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "771e56aff169230ede4505013a4bcf9f";

  mock_file(0, "game.gdi", (uint8_t*)"game.bin", 8);
  mock_file(1, "track03.bin", image, image_size);
  mock_file_first_sector(1, 45000);
  mock_file(2, "track26.bin", image, image_size);
  mock_file_first_sector(2, 548106);
  mock_cd_num_tracks(26);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_DREAMCAST, "game.gdi");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.gdi", NULL, 0);
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

static void test_hash_dreamcast_cue()
{
  const char* cue_file =
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 MODE1/2352\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 00 00:00:00\n"
      "    INDEX 01 00:02:00\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 MODE1/2352\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track04.bin\" BINARY\n"
      "  TRACK 04 AUDIO\n"
      "    INDEX 00 00:00:00\n"
      "    INDEX 01 00:02:00\n"
      "FILE \"track05.bin\" BINARY\n"
      "  TRACK 05 MODE1/2352\n"
      "    INDEX 00 00:00:00\n"
      "    INDEX 01 00:03:00\n";
  size_t image_size;
  uint8_t* image = convert_to_2352(generate_dreamcast_bin(45000, 1697028, &image_size), &image_size, 45000);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "c952864c3364591d2a8793ce2cfbf3a0";

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track01.bin", image, 1425312);    /* 606 sectors */
  mock_file(2, "track02.bin", image, 1589952);    /* 676 sectors */
  mock_file(3, "track03.bin", image, image_size); /* 737 sectors */
  mock_file(4, "track04.bin", image, 1237152);    /* 526 sectors */
  mock_file(5, "track05.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual first_track_sector calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_DREAMCAST, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_gamecube()
{
  size_t image_size;
  uint8_t* image = generate_gamecube_iso(32, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "c7803b704fa43d22d8f6e55f4789cb45";

  mock_file(0, "test.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_GAMECUBE, "test.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "test.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);

  ASSERT_NUM_EQUALS(image_size, 32 * 1024 * 1024);
}

/* ========================================================================= */

static void test_hash_neogeocd()
{
  const char* ipl_txt = "FIXA.FIX,0,0\r\nPROG.PRG,0,0\r\nSOUND.PCM,0,0\r\n\x1a";
  const size_t prog_prg_size = 273470;
  uint8_t* prog_prg = generate_generic_file(prog_prg_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "96f35b20c6cf902286da45e81a50b2a3";

  generate_iso9660_file(image, "IPL.TXT", (uint8_t*)ipl_txt, strlen(ipl_txt));
  generate_iso9660_file(image, "PROG.PRG", prog_prg, prog_prg_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NEO_GEO_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(prog_prg);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_neogeocd_multiple_prg()
{
  const char* ipl_txt = "FIXA.FIX,0,0\r\nPROG1.PRG,0,0\r\nSOUND.PCM,0,0\r\nPROG2.PRG,0,44000\r\n\x1a";
  const size_t prog1_prg_size = 273470;
  uint8_t* prog1_prg = generate_generic_file(prog1_prg_size);
  const size_t prog2_prg_size = 13768;
  uint8_t* prog2_prg = generate_generic_file(prog2_prg_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "d62df483c4786d3c63f27b6c5f17eeca";

  generate_iso9660_file(image, "IPL.TXT", (uint8_t*)ipl_txt, strlen(ipl_txt));
  generate_iso9660_file(image, "PROG1.PRG", prog1_prg, prog1_prg_size);
  generate_iso9660_file(image, "PROG2.PRG", prog2_prg, prog2_prg_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NEO_GEO_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(prog1_prg);
  free(prog2_prg);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_neogeocd_lowercase_ipl_contents()
{
  const char* ipl_txt = "fixa.fix,0,0\r\nprog.prg,0,0\r\nsound.pcm,0,0\r\n\x1a";
  const size_t prog_prg_size = 273470;
  uint8_t* prog_prg = generate_generic_file(prog_prg_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "96f35b20c6cf902286da45e81a50b2a3";

  generate_iso9660_file(image, "IPL.TXT", (uint8_t*)ipl_txt, strlen(ipl_txt));
  generate_iso9660_file(image, "PROG.PRG", prog_prg, prog_prg_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NEO_GEO_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(prog_prg);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_pce_cd()
{
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "6565819195a49323e080e7539b54f251";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC_ENGINE_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_pce_cd_invalid_header()
{
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* make the header not match */
  image[2048 + 0x24] = 0x34;

  test_hash_unknown_format(RC_CONSOLE_PC_ENGINE_CD, "game.cue");

  free(image);
}

/* ========================================================================= */

static void test_hash_pcfx()
{
  size_t image_size;
  uint8_t* image = generate_pcfx_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "0a03af66559b8529c50c4e7788379598";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PCFX, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_pcfx_invalid_header()
{
  size_t image_size;
  uint8_t* image = generate_pcfx_bin(72, &image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* make the header not match */
  image[12] = 0x34;

  test_hash_unknown_format(RC_CONSOLE_PCFX, "game.cue");

  free(image);
}

static void test_hash_pcfx_pce_cd()
{
  /* Battle Heat is formatted as a PC-Engine CD */
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "6565819195a49323e080e7539b54f251";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);
  mock_file(2, "game2.bin", image, image_size); /* PC-Engine CD check only applies to track 2 */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PCFX, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_psx_cd()
{
  /* BOOT=cdrom:\SLUS_007.45 */
  size_t image_size;
  uint8_t* image = generate_psx_bin("SLUS_007.45", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "db433fb038cde4fb15c144e8c7dea6e3";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_psx_cd_no_system_cnf()
{
  size_t image_size;
  uint8_t* image;
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "e494c79a7315be0dc3e8571c45df162c";
  uint32_t binary_size = 0x12000;
  const uint32_t sectors_needed = (((binary_size + 2047) / 2048) + 20);
  uint8_t* exe;

  image = generate_iso9660_bin(sectors_needed, "HOMEBREW", &image_size);
  exe = generate_iso9660_file(image, "PSX.EXE", NULL, binary_size);
  memcpy(exe, "PS-X EXE", 8);
    binary_size -= 2048;
  exe[28] = binary_size & 0xFF;
  exe[29] = (binary_size >> 8) & 0xFF;
  exe[30] = (binary_size >> 16) & 0xFF;
  exe[31] = (binary_size >> 24) & 0xFF;

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_psx_cd_exe_in_subfolder()
{
  /* BOOT=cdrom:\bin\SLUS_012.37 */
  size_t image_size;
  uint8_t* image = generate_psx_bin("bin\\SCES_012.37", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "674018e23a4052113665dfb264e9c2fc";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_psx_cd_extra_slash()
{
  /* BOOT=cdrom:\\SLUS_007.45 */
  size_t image_size;
  uint8_t* image = generate_psx_bin("\\SLUS_007.45", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "db433fb038cde4fb15c144e8c7dea6e3";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_ps2_iso()
{
  size_t image_size;
  uint8_t* image = generate_ps2_bin("SLUS_200.64", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "01a517e4ad72c6c2654d1b839be7579d";

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION_2, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
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

static void test_hash_ps2_psx()
{
  size_t image_size;
  uint8_t* image = generate_psx_bin("SLUS_007.45", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "db433fb038cde4fb15c144e8c7dea6e3";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION_2, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5); /* PSX hash */
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation (should not generate PS2 hash for PSX file) */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_psp()
{
  const size_t param_sfo_size = 690;
  uint8_t* param_sfo = generate_generic_file(param_sfo_size);
  const size_t eboot_bin_size = 273470;
  uint8_t* eboot_bin = generate_generic_file(eboot_bin_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "27ec2f9b7238b2ef29af31ddd254f201";

  generate_iso9660_file(image, "PSP_GAME\\PARAM.SFO", param_sfo, param_sfo_size);
  generate_iso9660_file(image, "PSP_GAME\\SYSDIR\\EBOOT.BIN", eboot_bin, eboot_bin_size);

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PSP, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(eboot_bin);
  free(param_sfo);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_psp_video()
{
  const size_t param_sfo_size = 690;
  uint8_t* param_sfo = generate_generic_file(param_sfo_size);
  const size_t eboot_bin_size = 273470;
  uint8_t* eboot_bin = generate_generic_file(eboot_bin_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];

  /* UMD video disc may have an UPDATE folder, but nothing in the PSP_GAME or SYSDIR folders. */
  generate_iso9660_file(image, "PSP_GAME\\SYSDIR\\UPDATE\\EBOOT.BIN", eboot_bin, eboot_bin_size);
  /* the PARAM.SFO file is in the UMD_VIDEO folder. */
  generate_iso9660_file(image, "UMD_VIDEO\\PARAM.SFO", param_sfo, param_sfo_size);

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PSP, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(eboot_bin);
  free(param_sfo);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_psp_homebrew()
{
  const size_t image_size = 3532124;
  uint8_t* image = generate_generic_file(image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "fcde8760893b09e508e5f4fe642eb132";

  mock_file(0, "eboot.pbp", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PSP, "eboot.pbp");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "eboot.pbp", NULL, 0);
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

static void test_hash_sega_cd()
{
  /* the first 512 bytes of sector 0 are a volume header and ROM header. 
   * generate a generic block and add the Sega CD marker */
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "574498e1453cb8934df60c4ab906e783";
  memcpy(image, "SEGADISCSYSTEM  ", 16);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_SEGA_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_sega_cd_invalid_header()
{
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  test_hash_unknown_format(RC_CONSOLE_SEGA_CD, "game.cue");

  free(image);
}

static void test_hash_saturn()
{
  /* the first 512 bytes of sector 0 are a volume header and ROM header. 
   * generate a generic block and add the Sega CD marker */
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "4cd9c8e41cd8d137be15bbe6a93ae1d8";
  memcpy(image, "SEGA SEGASATURN ", 16);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_SATURN, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
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

static void test_hash_saturn_invalid_header()
{
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  test_hash_unknown_format(RC_CONSOLE_SATURN, "game.cue");

  free(image);
}

/* ========================================================================= */

void test_hash_disc(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();
  init_mock_cdreader();

  /* 3DO */
  TEST(test_hash_3do_bin);
  TEST(test_hash_3do_cue);
  TEST(test_hash_3do_iso);
  TEST(test_hash_3do_invalid_header);
  TEST(test_hash_3do_launchme_case_insensitive);
  TEST(test_hash_3do_no_launchme);
  TEST(test_hash_3do_long_directory);

  /* Amstrad CPC */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_AMSTRAD_PC, "test.dsk", 194816, "9d616e4ad3f16966f61422c57e22aadd");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_AMSTRAD_PC, "test.dsk", 194816, "9d616e4ad3f16966f61422c57e22aadd");

  /* Apple II */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.nib", 232960, "96e8d33bdc385fd494327d6e6791cbe4");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");

  /* Atari Jaguar CD */
  TEST(test_hash_atari_jaguar_cd);
  TEST(test_hash_atari_jaguar_cd_byteswapped);
  TEST(test_hash_atari_jaguar_cd_track3);
  TEST(test_hash_atari_jaguar_cd_no_header);
  TEST(test_hash_atari_jaguar_cd_no_sessions);
  TEST(test_hash_atari_jaguar_cd_homebrew);

  /* Commodore 64 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COMMODORE_64, "test.nib", 327936, "e7767d32b23e3fa62c5a250a08caeba3");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COMMODORE_64, "test.d64", 174848, "ecd5a8ef4e77f2e9469d9b6e891394f0");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_COMMODORE_64, "test.d64", 174848, "ecd5a8ef4e77f2e9469d9b6e891394f0");

  /* Dreamcast */
  TEST(test_hash_dreamcast_single_bin);
  TEST(test_hash_dreamcast_split_bin);
  TEST(test_hash_dreamcast_cue);

  /* Gamecube */
  TEST(test_hash_gamecube);

  /* MSX */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");

  /* Neo Geo CD */
  TEST(test_hash_neogeocd);
  TEST(test_hash_neogeocd_multiple_prg);
  TEST(test_hash_neogeocd_lowercase_ipl_contents);

  /* PC-8800 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");

  /* PC Engine CD */
  TEST(test_hash_pce_cd);
  TEST(test_hash_pce_cd_invalid_header);

  /* PC-FX */
  TEST(test_hash_pcfx);
  TEST(test_hash_pcfx_invalid_header);
  TEST(test_hash_pcfx_pce_cd);

  /* Playstation */
  TEST(test_hash_psx_cd);
  TEST(test_hash_psx_cd_no_system_cnf);
  TEST(test_hash_psx_cd_exe_in_subfolder);
  TEST(test_hash_psx_cd_extra_slash);

  /* Playstation 2 */
  TEST(test_hash_ps2_iso);
  TEST(test_hash_ps2_psx);

  /* Playstation Portable */
  TEST(test_hash_psp);
  TEST(test_hash_psp_video);
  TEST(test_hash_psp_homebrew);

  /* Sega CD */
  TEST(test_hash_sega_cd);
  TEST(test_hash_sega_cd_invalid_header);

  /* Sega Saturn */
  TEST(test_hash_saturn);
  TEST(test_hash_saturn_invalid_header);

  /* ZX Spectrum */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ZX_SPECTRUM, "test.tap", 1596, "714a9f455e616813dd5421c5b347e5e5");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ZX_SPECTRUM, "test.tzx", 14971, "93723e6d1100f9d1d448a27cf6618c47");

  TEST_SUITE_END();
}
