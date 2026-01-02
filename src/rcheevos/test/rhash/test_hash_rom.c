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

static void test_hash_arcade(const char* path, const char* expected_md5)
{
  char hash_file[33], hash_iterator[33];

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARCADE, path);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, path, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_arduboy()
{
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "67b64633285a7f965064ba29dab45148";

  const char* hex_input =
    ":100000000C94690D0C94910D0C94910D0C94910D20\n"
    ":100010000C94910D0C94910D0C94910D0C94910DE8\n"
    ":100020000C94910D0C94910D0C94C32A0C94352BC7\n"
    ":00000001FF\n";
  mock_file(0, "game.hex", (const uint8_t*)hex_input, strlen(hex_input));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.hex");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.hex", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_arduboy_crlf()
{
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "67b64633285a7f965064ba29dab45148";

  const char* hex_input =
    ":100000000C94690D0C94910D0C94910D0C94910D20\r\n"
    ":100010000C94910D0C94910D0C94910D0C94910DE8\r\n"
    ":100020000C94910D0C94910D0C94C32A0C94352BC7\r\n"
    ":00000001FF\r\n";
  mock_file(0, "game.hex", (const uint8_t*)hex_input, strlen(hex_input));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.hex");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.hex", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_arduboy_no_final_lf()
{
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "67b64633285a7f965064ba29dab45148";

  const char* hex_input =
    ":100000000C94690D0C94910D0C94910D0C94910D20\n"
    ":100010000C94910D0C94910D0C94910D0C94910DE8\n"
    ":100020000C94910D0C94910D0C94C32A0C94352BC7\n"
    ":00000001FF";
  mock_file(0, "game.hex", (const uint8_t*)hex_input, strlen(hex_input));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.hex");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.hex", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static uint8_t* generate_atari_7800_file(size_t kb, int with_header, size_t* image_size)
{
  uint8_t* image;
  size_t size_needed = kb * 1024;
  if (with_header)
    size_needed += 128;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL) {
    if (with_header) {
      const uint8_t header[128] = {
        3, 'A', 'T', 'A', 'R', 'I', '7', '8', '0', '0', 0, 0, 0, 0, 0, 0, /* version + magic text */
        0, 'G', 'a', 'm', 'e', 'N', 'a', 'm', 'e', 0, 0, 0, 0, 0, 0, 0,   /* game name */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                   /* game name (cont'd) */
        0, 0, 2, 0, 0, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0,                   /* attributes */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                   /* unused */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                   /* unused */
        0, 0, 0, 0, 'A', 'C', 'T', 'U', 'A', 'L', ' ', 'C', 'A', 'R', 'T',/* magic text*/
        'D', 'A', 'T', 'A', ' ', 'S', 'T', 'A', 'R', 'T', 'S', ' ', 'H', 'E', 'R', 'E' /* magic text */
      };
      memcpy(image, header, sizeof(header));
      image[50] = (uint8_t)(kb / 4); /* 4-byte value starting at address 49 is the ROM size without header */

      fill_image(image + 128, size_needed - 128);
    }
    else {
      fill_image(image, size_needed);
    }
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

static void test_hash_atari_7800()
{
  size_t image_size;
  uint8_t* image = generate_atari_7800_file(16, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_ATARI_7800, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "455f07d8500f3fabc54906737866167f");
  ASSERT_NUM_EQUALS(image_size, 16384);
}

static void test_hash_atari_7800_with_header()
{
  size_t image_size;
  uint8_t* image = generate_atari_7800_file(16, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_ATARI_7800, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_atari_7800 */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "455f07d8500f3fabc54906737866167f");
  ASSERT_NUM_EQUALS(image_size, 16384 + 128);
}

/* ========================================================================= */

uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size)
{
  uint8_t* image;
  size_t size_needed = kb * 1024;
  if (with_header)
    size_needed += 16;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL) {
    if (with_header) {
      image[0] = 'N';
      image[1] = 'E';
      image[2] = 'S';
      image[3] = '\x1A';
      image[4] = (uint8_t)(kb / 16);

      fill_image(image + 16, size_needed - 16);
    }
    else {
      fill_image(image, size_needed);
    }
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

static uint8_t* generate_fds_file(size_t sides, int with_header, size_t* image_size)
{
  uint8_t* image;
  size_t size_needed = sides * 65500;
  if (with_header)
    size_needed += 16;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL) {
    if (with_header) {
      image[0] = 'F';
      image[1] = 'D';
      image[2] = 'S';
      image[3] = '\x1A';
      image[4] = (uint8_t)sides;

      fill_image(image + 16, size_needed - 16);
    }
    else {
      fill_image(image, size_needed);
    }
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

static void test_hash_nes_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768);
}

static void test_hash_nes_32k_with_header()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_nes_32k */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768 + 16);
}

static void test_hash_nes_256k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(256, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "545d527301b8ae148153988d6c4fcb84");
  ASSERT_NUM_EQUALS(image_size, 262144);
}

static void test_hash_fds_two_sides()
{
  size_t image_size;
  uint8_t* image = generate_fds_file(2, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "fd770d4d34c00760fabda6ad294a8f0b");
  ASSERT_NUM_EQUALS(image_size, 65500 * 2);
}

static void test_hash_fds_two_sides_with_header()
{
  size_t image_size;
  uint8_t* image = generate_fds_file(2, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_fds_two_sides */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "fd770d4d34c00760fabda6ad294a8f0b");
  ASSERT_NUM_EQUALS(image_size, 65500 * 2 + 16);
}

static void test_hash_nes_file_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash[33];
  int result;

  mock_file(0, "test.nes", image, image_size);
  result = rc_hash_generate_from_file(hash, RC_CONSOLE_NINTENDO, "test.nes");
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768);
}

static void test_hash_nes_iterator_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash1[33], hash2[33];
  int result1, result2;
  struct rc_hash_iterator iterator;

  mock_file(0, "test.nes", image, image_size);
  rc_hash_initialize_iterator(&iterator, "test.nes", NULL, 0);
  result1 = rc_hash_iterate(hash1, &iterator);
  result2 = rc_hash_iterate(hash2, &iterator);
  rc_hash_destroy_iterator(&iterator);
  free(image);

  ASSERT_NUM_EQUALS(result1, 1);
  ASSERT_STR_EQUALS(hash1, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_NUM_EQUALS(result2, 0);
  ASSERT_STR_EQUALS(hash2, "");
}

static void test_hash_nes_file_iterator_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash1[33], hash2[33];
  int result1, result2;
  struct rc_hash_iterator iterator;
  rc_hash_initialize_iterator(&iterator, "test.nes", image, image_size);
  result1 = rc_hash_iterate(hash1, &iterator);
  result2 = rc_hash_iterate(hash2, &iterator);
  rc_hash_destroy_iterator(&iterator);
  free(image);

  ASSERT_NUM_EQUALS(result1, 1);
  ASSERT_STR_EQUALS(hash1, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_NUM_EQUALS(result2, 0);
  ASSERT_STR_EQUALS(hash2, "");
}

/* ========================================================================= */

/* first 64 bytes of SUPER MARIO 64 ROM in each N64 format */
static uint8_t test_rom_z64[64] = {
  0x80, 0x37, 0x12, 0x40, 0x00, 0x00, 0x00, 0x0F, 0x80, 0x24, 0x60, 0x00, 0x00, 0x00, 0x14, 0x44,
  0x63, 0x5A, 0x2B, 0xFF, 0x8B, 0x02, 0x23, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x53, 0x55, 0x50, 0x45, 0x52, 0x20, 0x4D, 0x41, 0x52, 0x49, 0x4F, 0x20, 0x36, 0x34, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x53, 0x4D, 0x45, 0x00
};

static uint8_t test_rom_v64[64] = {
  0x37, 0x80, 0x40, 0x12, 0x00, 0x00, 0x0F, 0x00, 0x24, 0x80, 0x00, 0x60, 0x00, 0x00, 0x44, 0x14,
  0x5A, 0x63, 0xFF, 0x2B, 0x02, 0x8B, 0x26, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x55, 0x53, 0x45, 0x50, 0x20, 0x52, 0x41, 0x4D, 0x49, 0x52, 0x20, 0x4F, 0x34, 0x36, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x4D, 0x53, 0x00, 0x45
};

static uint8_t test_rom_n64[64] = {
  0x40, 0x12, 0x37, 0x80, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x60, 0x24, 0x80, 0x44, 0x14, 0x00, 0x00,
  0xFF, 0x2B, 0x5A, 0x63, 0x26, 0x23, 0x02, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x45, 0x50, 0x55, 0x53, 0x41, 0x4D, 0x20, 0x52, 0x20, 0x4F, 0x49, 0x52, 0x20, 0x20, 0x34, 0x36,
  0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x45, 0x4D, 0x53
};

/* first 64 bytes of DOSHIN THE GIANT in ndd format */
static uint8_t test_rom_ndd[64] = {
  0xE8, 0x48, 0xD3, 0x16, 0x10, 0x13, 0x00, 0x45, 0x0C, 0x18, 0x24, 0x30, 0x3C, 0x48, 0x54, 0x60,
  0x6C, 0x78, 0x84, 0x90, 0x9C, 0xA8, 0xB4, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0x02, 0x5C, 0x00,
  0x10, 0x16, 0x1C, 0x22, 0x28, 0x2A, 0x31, 0x32, 0x3A, 0x40, 0x46, 0x4C, 0x04, 0x0C, 0x14, 0x1C,
  0x24, 0x2C, 0x34, 0x3C, 0x44, 0x4C, 0x54, 0x5C, 0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C
};

static void test_hash_n64(uint8_t* buffer, size_t buffer_size, const char* expected_hash)
{
  char hash[33];
  int result;

  rc_hash_reset_filereader(); /* explicitly unset the filereader */
  result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO_64, buffer, buffer_size);
  init_mock_filereader(); /* restore the mock filereader */

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, expected_hash);
}

static void test_hash_n64_file(const char* filename, uint8_t* buffer, size_t buffer_size, const char* expected_hash)
{
  char hash_file[33], hash_iterator[33];
  mock_file(0, filename, buffer, buffer_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_64, filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

/* ========================================================================= */

static uint8_t* generate_nds_file(size_t mb, uint32_t arm9_size, uint32_t arm7_size, size_t* image_size)
{
  uint8_t* image;
  const size_t size_needed = mb * 1024 * 1024;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL) {
    uint32_t arm9_addr = 65536;
    uint32_t arm7_addr = arm9_addr + arm9_size;
    uint32_t icon_addr = arm7_addr + arm7_size;

    fill_image(image, size_needed);

    image[0x20] = (arm9_addr & 0xFF);
    image[0x21] = ((arm9_addr >> 8) & 0xFF);
    image[0x22] = ((arm9_addr >> 16) & 0xFF);
    image[0x23] = ((arm9_addr >> 24) & 0xFF);
    image[0x2C] = (arm9_size & 0xFF);
    image[0x2D] = ((arm9_size >> 8) & 0xFF);
    image[0x2E] = ((arm9_size >> 16) & 0xFF);
    image[0x2F] = ((arm9_size >> 24) & 0xFF);

    image[0x30] = (arm7_addr & 0xFF);
    image[0x31] = ((arm7_addr >> 8) & 0xFF);
    image[0x32] = ((arm7_addr >> 16) & 0xFF);
    image[0x33] = ((arm7_addr >> 24) & 0xFF);
    image[0x3C] = (arm7_size & 0xFF);
    image[0x3D] = ((arm7_size >> 8) & 0xFF);
    image[0x3E] = ((arm7_size >> 16) & 0xFF);
    image[0x3F] = ((arm7_size >> 24) & 0xFF);

    image[0x68] = (icon_addr & 0xFF);
    image[0x69] = ((icon_addr >> 8) & 0xFF);
    image[0x6A] = ((icon_addr >> 16) & 0xFF);
    image[0x6B] = ((icon_addr >> 24) & 0xFF);
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

static void test_hash_nds()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  mock_file(0, "game.nds", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_DS, "game.nds");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

static void test_hash_nds_supercard()
{
  size_t image_size, image2_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";
  ASSERT_PTR_NOT_NULL(image);
  ASSERT_NUM_GREATER(image_size, 0);

  /* inject the SuperCard header (512 bytes) */
  image2_size = image_size + 512;
  uint8_t* image2 = malloc(image2_size);
  ASSERT_PTR_NOT_NULL(image2);
  memcpy(&image2[512], &image[0], image_size);
  memset(&image2[0], 0, 512);
  image2[0] = 0x2E;
  image2[1] = 0x00;
  image2[2] = 0x00;
  image2[3] = 0xEA;
  image2[0xB0] = 0x44;
  image2[0xB1] = 0x46;
  image2[0xB2] = 0x96;
  image2[0xB3] = 0x00;

  mock_file(0, "game.nds", image2, image2_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_DS, "game.nds");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(image2);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

static void test_hash_nds_buffered()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_buffer[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  /* test file hash */
  int result_buffer = rc_hash_generate_from_buffer(hash_buffer, RC_CONSOLE_NINTENDO_DS, image, image_size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", image, image_size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_buffer, 1);
  ASSERT_STR_EQUALS(hash_buffer, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

/* ========================================================================= */

static void test_hash_dsi()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  mock_file(0, "game.nds", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_DSI, "game.nds");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

static void test_hash_dsi_buffered()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_buffer[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  /* test file hash */
  int result_buffer = rc_hash_generate_from_buffer(hash_buffer, RC_CONSOLE_NINTENDO_DSI, image, image_size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", image, image_size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_buffer, 1);
  ASSERT_STR_EQUALS(hash_buffer, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

/* ========================================================================= */

static void test_hash_scv_cart()
{
  size_t image_size = 32768 + 32;
  uint8_t* image = generate_generic_file(image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "4309c9844b44f9ff8256dfc04687b8fd";

  memcpy(image, "EmuSCV....CART..................", 32);

  mock_file(0, "game.cart", image, image_size);
  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_SUPER_CASSETTEVISION, "game.cart");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cart", NULL, 0);
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

void test_hash_rom(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();

  /* Arcade */
  TEST_PARAMS2(test_hash_arcade, "game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "game.7z", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "roms\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "C:\\roms\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "/home/user/roms/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "/home/user/games/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "/home/user/roms/game.7z", "c8d46d341bea4fd5bff866a65ff8aea9");

  TEST_PARAMS2(test_hash_arcade, "/home/user/nes_game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS2(test_hash_arcade, "/home/user/nes/game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS2(test_hash_arcade, "C:\\roms\\nes\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS2(test_hash_arcade, "C:\\roms\\NES\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS2(test_hash_arcade, "nes\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS2(test_hash_arcade, "/home/user/snes/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS2(test_hash_arcade, "/home/user/nes2/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");

  /* we don't care that multiple aliases for the same system generate different hashes - the point is
   * that they don't generate the same hash as an actual arcade ROM with the same filename. */
  TEST_PARAMS2(test_hash_arcade, "/home/user/chf/game.zip", "6ef57f16562ea0c7f49d93853b313e32");
  TEST_PARAMS2(test_hash_arcade, "/home/user/channelf/game.zip", "7b6506637a0cc79bd1d24a43a34fa3b9");
  TEST_PARAMS2(test_hash_arcade, "/home/user/coleco/game.zip", "c546f63ae7de98add4b9f221a4749260");
  TEST_PARAMS2(test_hash_arcade, "/home/user/colecovision/game.zip", "47279207b94dbf2a45cb13efa56d685e");
  TEST_PARAMS2(test_hash_arcade, "/home/user/msx/game.zip", "59ab85f6b56324fd81b4e324b804c29f");
  TEST_PARAMS2(test_hash_arcade, "/home/user/msx1/game.zip", "33328d832dcb0854383cdd4a4565c459");
  TEST_PARAMS2(test_hash_arcade, "/home/user/pce/game.zip", "c414a783f3983bbe2e9e01d9d5320c7e");
  TEST_PARAMS2(test_hash_arcade, "/home/user/pcengine/game.zip", "49370c3cbe98bdcdce545c68379487db");
  TEST_PARAMS2(test_hash_arcade, "/home/user/sgx/game.zip", "db545ab29694bfda1010317d4bac83b8");
  TEST_PARAMS2(test_hash_arcade, "/home/user/supergrafx/game.zip", "5665c9ef4c2f6609d8e420c4d86ba692");
  TEST_PARAMS2(test_hash_arcade, "/home/user/tg16/game.zip", "8b6c5c2e54915be2cdba63973862e143");
  TEST_PARAMS2(test_hash_arcade, "/home/user/fds/game.zip", "c0c135a97e8c577cfdf9204823ff211f");
  TEST_PARAMS2(test_hash_arcade, "/home/user/gamegear/game.zip", "f6f471e952b8103032b723f57bdbe767");
  TEST_PARAMS2(test_hash_arcade, "/home/user/mastersystem/game.zip", "f4805afe0ff5647140a26bd0a1057373");
  TEST_PARAMS2(test_hash_arcade, "/home/user/sms/game.zip", "43f35f575dead94dd2f42f9caf69fe5a");
  TEST_PARAMS2(test_hash_arcade, "/home/user/megadriv/game.zip", "f99d0aaf12ba3eb6ced9878c76692c63");
  TEST_PARAMS2(test_hash_arcade, "/home/user/megadrive/game.zip", "73eb5d7034b382093b1d36414d9e84e4");
  TEST_PARAMS2(test_hash_arcade, "/home/user/genesis/game.zip", "b62f810c63e1cba7f5b7569643bec236");
  TEST_PARAMS2(test_hash_arcade, "/home/user/sg1000/game.zip", "e8f6c711c4371f09537b4f2a7a304d6c");
  TEST_PARAMS2(test_hash_arcade, "/home/user/spectrum/game.zip", "a5f62157b2617bd728c4b1bc885c29e9");
  TEST_PARAMS2(test_hash_arcade, "/home/user/ngp/game.zip", "d4133b74c4e57274ca514e27a370dcb6");

  /* Arduboy */
  TEST(test_hash_arduboy);
  TEST(test_hash_arduboy_crlf);
  TEST(test_hash_arduboy_no_final_lf);

  /* Arcadia 2001 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ARCADIA_2001, "test.bin", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* Atari 2600 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_2600, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Atari 7800 */
  TEST(test_hash_atari_7800);
  TEST(test_hash_atari_7800_with_header);

  /* Atari Jaguar */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_JAGUAR, "test.jag", 0x400000, "a247ec8a8c42e18fcb80702dfadac14b");

  /* Colecovision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COLECOVISION, "test.col", 16384, "455f07d8500f3fabc54906737866167f");

  /* Elektor TV Games Computer */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER, "test.pgm", 4096, "572686c3a073162e4ec6eff86e6f6e3a");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER, "test.tvc", 1861, "37097124a29aff663432d049654a17dc");

  /* Fairchild Channel F */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_FAIRCHILD_CHANNEL_F, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_FAIRCHILD_CHANNEL_F, "test.chf", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Gameboy */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY, "test.gb", 131072, "a0f425b23200568132ba76b2405e3933");

  /* Gameboy Color */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY_COLOR, "test.gbc", 2097152, "cf86acf519625a25a17b1246975e90ae");

  /* Gameboy Advance */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY_COLOR, "test.gba", 4194304, "a247ec8a8c42e18fcb80702dfadac14b");

  /* Game Gear */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAME_GEAR, "test.gg", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Intellivision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_INTELLIVISION, "test.bin", 8192, "ce1127f881b40ce6a67ecefba50e2835");

  /* Interton VC 4000 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_INTERTON_VC_4000, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Magnavox Odyssey 2 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MAGNAVOX_ODYSSEY2, "test.bin", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* Master System */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MASTER_SYSTEM, "test.sms", 131072, "a0f425b23200568132ba76b2405e3933");

  /* Mega Drive */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MEGA_DRIVE, "test.md", 1048576, "da9461b3b0f74becc3ccf6c2a094c516");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_MEGA_DRIVE, "test.md", 1048576, "da9461b3b0f74becc3ccf6c2a094c516");

  /* Mega Duck */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MEGADUCK, "test.bin", 65536, "8e6576cd5c21e44e0bbfc4480577b040");

  /* Neo Geo Pocket */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_NEOGEO_POCKET, "test.ngc", 2097152, "cf86acf519625a25a17b1246975e90ae");

  /* NES */
  TEST(test_hash_nes_32k);
  TEST(test_hash_nes_32k_with_header);
  TEST(test_hash_nes_256k);
  TEST(test_hash_fds_two_sides);
  TEST(test_hash_fds_two_sides_with_header);

  TEST(test_hash_nes_file_32k);
  TEST(test_hash_nes_file_iterator_32k);
  TEST(test_hash_nes_iterator_32k);

  /* Nintendo 64 */
  TEST_PARAMS3(test_hash_n64, test_rom_z64, sizeof(test_rom_z64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS3(test_hash_n64, test_rom_v64, sizeof(test_rom_v64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS3(test_hash_n64, test_rom_n64, sizeof(test_rom_n64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.z64", test_rom_z64, sizeof(test_rom_z64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.v64", test_rom_v64, sizeof(test_rom_v64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.n64", test_rom_n64, sizeof(test_rom_n64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.n64", test_rom_z64, sizeof(test_rom_z64), "06096d7ce21cb6bcde38391534c4eb91"); /* misnamed */
  TEST_PARAMS4(test_hash_n64_file, "game.z64", test_rom_n64, sizeof(test_rom_n64), "06096d7ce21cb6bcde38391534c4eb91"); /* misnamed */
  TEST_PARAMS3(test_hash_n64, test_rom_ndd, sizeof(test_rom_ndd), "a698b32a52970d8a52a5a52c83acc2a9");

  /* Nintendo DS */
  TEST(test_hash_nds);
  TEST(test_hash_nds_supercard);
  TEST(test_hash_nds_buffered);

  /* Nintendo DSi */
  TEST(test_hash_dsi);
  TEST(test_hash_dsi_buffered);

  /* Oric (no fixed file size) */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ORIC, "test.tap", 18119, "953a2baa3232c63286aeae36b2172cef");

  /* PC Engine */
  /* NOTE: because the data after the header doesn't match, the headered and non-headered hashes won't match
   * but the test results ensure that we're only hashing the portion after the header when detected */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC_ENGINE, "test.pce", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC_ENGINE, "test.pce", 524288 + 512, "258c93ebaca1c3f488ab48218e5e8d38");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC_ENGINE, "test.pce", 491520 + 512, "ebb565a7f964ccdfaecdce0d6ed540af");

  /* Pokemon Mini */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_POKEMON_MINI, "test.min", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Sega 32X */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SEGA_32X, "test.bin", 3145728, "07d733f252896ec41b4fd521fe610e2c");

  /* SG-1000 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.sg", 32768, "6a2305a2b6675a97ff792709be1ca857");

  /* SNES */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPER_NINTENDO, "test.smc", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPER_NINTENDO, "test.smc", 524288 + 512, "258c93ebaca1c3f488ab48218e5e8d38");

  /* Super Cassette Vision */
  TEST(test_hash_scv_cart);
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPER_CASSETTEVISION, "test.bin", 32768, "6a2305a2b6675a97ff792709be1ca857");

  /* TI-83 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_TI83, "test.83g", 1695, "bfb6048395a425c69743900785987c42");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_TI83, "test.83p", 2500, "6e81d530ee9a79d4f4f505729ad74bb5");

  /* TIC-80 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_TIC80, "test.tic", 67682, "79b96f4ffcedb3ce8210a83b22cd2c69");

  /* Uzebox */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_UZEBOX, "test.uze", 53654, "a9aab505e92edc034d3c732869159789");

  /* Vectrex */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.vec", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* VirtualBoy */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.vb", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Watara Supervision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPERVISION, "test.sv", 32768, "6a2305a2b6675a97ff792709be1ca857");

  /* WASM-4 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WASM4, "test.wasm", 33454, "bce38bb5f05622fc7e0e56757059d180");

  /* WonderSwan */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WONDERSWAN, "test.ws", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WONDERSWAN, "test.wsc", 4194304, "a247ec8a8c42e18fcb80702dfadac14b");

  TEST_SUITE_END();
}
