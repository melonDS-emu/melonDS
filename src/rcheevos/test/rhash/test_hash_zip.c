#include "rc_hash.h"

#include "../rhash/rc_hash_internal.h"

#include "../rc_compat.h"
#include "../test_framework.h"
#include "data.h"
#include "mock_filereader.h"

#include <stdlib.h>

typedef struct mock_zip_file_t {
  uint8_t* buffer;
  uint8_t* ptr;
  uint8_t* file_ptr[8];
  uint32_t num_files;
  uint8_t  is_zip64;
} mock_zip_file_t;

static void mock_zip_add_file(mock_zip_file_t* zip, const char* filename, uint32_t crc32, uint32_t size)
{
  const size_t filename_len = strlen(filename);
  uint8_t* out = zip->ptr;

  zip->file_ptr[zip->num_files++] = out;

  /* local file signature */
  *out++ = 'P';
  *out++ = 'K';
  *out++ = 0x03;
  *out++ = 0x04;

  /* version needed to extract */
  *out++ = 0x14;
  *out++ = 0x00;

  /* general purpose bit flag*/
  *out++ = 0x02;
  *out++ = 0x00;

  /* compression method */
  *out++ = 0x08;
  *out++ = 0x00;

  /* file last modified time */
  *out++ = 0x00;
  *out++ = 0xBC;

  /* file list modified date */
  *out++ = 0x98;
  *out++ = 0x21;

  /* CRC-32 */
  *out++ = crc32 & 0xFF;
  *out++ = (crc32 >> 8) & 0xFF;
  *out++ = (crc32 >> 16) & 0xFF;
  *out++ = (crc32 >> 24) & 0xFF;

  /* compressed size */
  *out++ = size & 0xFF;
  *out++ = (size >> 8) & 0xFF;
  *out++ = (size >> 16) & 0xFF;
  *out++ = (size >> 24) & 0xFF;

  /* uncompressed size */
  *out++ = size & 0xFF;
  *out++ = (size >> 8) & 0xFF;
  *out++ = (size >> 16) & 0xFF;
  *out++ = (size >> 24) & 0xFF;

  /* file name length */
  *out++ = filename_len & 0xFF;
  *out++ = (filename_len >> 8) & 0xFF;

  /* extra field length */
  *out++ = 0;
  *out++ = 0;

  /* file name */
  memcpy(out, filename, filename_len);
  out += filename_len;

  /* compressed content */
  *out++ = 0x73;
  *out++ = 0x02;
  *out++ = 0x00;

  zip->ptr = out;
}

static size_t mock_zip_finalize(mock_zip_file_t* zip, const char* comment)
{
  size_t comment_len = strlen(comment);
  uint8_t* out = zip->ptr;
  uint8_t* first_cdir_entry = zip->ptr;
  size_t offset;
  uint32_t filename_len;
  uint32_t i;

  for (i = 0; i < zip->num_files; i++) {
    uint8_t* in = zip->file_ptr[i];

    /* central directory file header */
    *out++ = 'P';
    *out++ = 'K';
    *out++ = 0x01;
    *out++ = 0x02;

    /* version made by */
    *out++ = 0x14;
    *out++ = 0x00;

    /* version needed to extract (2) */
    /* general purpose bit flag (2) */
    /* compression method (2) */
    /* file last modified time (2) */
    /* file list modified date (2) */
    /* CRC-32 (4) */
    /* compressed size (4) */
    /* uncompressed size (4) */
    /* file name length (2) */
    /* extra field length (2) */
    memcpy(out, &in[4], 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2);
    if (zip->is_zip64) {
      /* in zip64 mode, blank out the size and the actual size will be appended in an extended field */
      memset(&out[14], 0xFF, 8);
      out[24] = 0x14; /* extra field length */
    }
    out += 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2;

    /* file comment length */
    *out++ = 0;
    *out++ = 0;

    /* disk number where file starts */
    *out++ = 0;
    *out++ = 0;

    /* internal file attributes */
    *out++ = 0;
    *out++ = 0;

    /* external file attributes */
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* local file header offset */
    offset = in - zip->buffer;
    *out++ = offset & 0xFF;
    *out++ = (offset >> 8) & 0xFF;
    *out++ = (offset >> 16) & 0xFF;
    *out++ = (offset >> 24) & 0xFF;

    /* file name */
    filename_len = (in[27] << 8) | in[26];
    memcpy(out, &in[30], filename_len);
    out += filename_len;

    if (zip->is_zip64) {
      /* zip64 extended information extra field header id */
      *out++ = 0x01;
      *out++ = 0x00;

      /* size of extra field chunk */
      *out++ = 0x10; /* only providing file sizes */
      *out++ = 0x00;

      /* uncompressed file size */
      memset(out, 0, 28);
      memcpy(out, &in[22], 4);
      out += 8;

      /* compressed file size */
      memset(out, 0, 16);
      memcpy(out, &in[18], 4);
      out += 8;
    }
  }

  zip->ptr = out;

  if (zip->is_zip64) {
    /* end of central directory header */
    *out++ = 'P';
    *out++ = 'K';
    *out++ = 0x06;
    *out++ = 0x06;

    /* size of EOCD64 minus 12 */
    *out++ = 0x2C;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* version made by */
    *out++ = 0x2D;
    *out++ = 0x00;

    /* version needed to extract */
    *out++ = 0x2D;
    *out++ = 0x00;

    /* disk number */
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* disk number of central directory */
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* number of central directory records on this disk */
    *out++ = zip->num_files & 0xFF;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* total number of central directory records */
    *out++ = zip->num_files & 0xFF;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* size of central directory */
    offset = zip->ptr - first_cdir_entry;
    *out++ = offset & 0xFF;
    *out++ = (offset >> 8) & 0xFF;
    *out++ = (offset >> 16) & 0xFF;
    *out++ = (offset >> 24) & 0xFF;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* address of first central directory entry */
    offset = first_cdir_entry - zip->buffer;
    *out++ = offset & 0xFF;
    *out++ = (offset >> 8) & 0xFF;
    *out++ = (offset >> 16) & 0xFF;
    *out++ = (offset >> 24) & 0xFF;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* end of central directory locator header */
    *out++ = 'P';
    *out++ = 'K';
    *out++ = 0x06;
    *out++ = 0x07;

    /* disk number */
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* address of central directory 64 */
    offset = zip->ptr - zip->buffer;
    *out++ = offset & 0xFF;
    *out++ = (offset >> 8) & 0xFF;
    *out++ = (offset >> 16) & 0xFF;
    *out++ = (offset >> 24) & 0xFF;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    /* total number of disks */
    *out++ = 1;
    *out++ = 0;
    *out++ = 0;
    *out++ = 0;

    zip->ptr = out;
  }

  /* end of central directory header */
  *out++ = 'P';
  *out++ = 'K';
  *out++ = 0x05;
  *out++ = 0x06;

  /* disk number */
  *out++ = 0;
  *out++ = 0;

  /* central directory disk number */
  *out++ = 0;
  *out++ = 0;

  /* number of central directory records on this disk */
  *out++ = zip->num_files & 0xFF;
  *out++ = 0;

  /* total number of central directory records */
  *out++ = zip->num_files & 0xFF;
  *out++ = 0;

  if (zip->is_zip64) {
    /* size and address of central directory are -1 in zip64 */
    memset(out, 0xFF, 8);
    out += 8;
  }
  else {
    /* size of central directory */
    offset = zip->ptr - first_cdir_entry;
    *out++ = offset & 0xFF;
    *out++ = (offset >> 8) & 0xFF;
    *out++ = (offset >> 16) & 0xFF;
    *out++ = (offset >> 24) & 0xFF;

    /* address of first central directory entry */
    offset = first_cdir_entry - zip->buffer;
    *out++ = offset & 0xFF;
    *out++ = (offset >> 8) & 0xFF;
    *out++ = (offset >> 16) & 0xFF;
    *out++ = (offset >> 24) & 0xFF;
  }

  /* comment length */
  *out++ = comment_len & 0xFF;
  *out++ = (comment_len >> 8) & 0xFF;

  if (comment_len) {
    memcpy(out, comment, comment_len);
    out += comment_len;
  }

  zip->ptr = out;

  return (zip->ptr - zip->buffer);
}

/* ========================================================================= */

static void test_hash_arduboy_fx()
{
  char hash_file[33], hash_iterator[33];
  mock_zip_file_t zip;
  uint8_t zip_contents[768];
  size_t zip_size;
  const char* expected_md5 = "e696445c353e9d6b3d60bf5d194b82cf";
  int result_file, result_iterator;

  memset(&zip, 0, sizeof(zip));
  zip.ptr = zip.buffer = zip_contents;
  mock_zip_add_file(&zip, "info.json", 0xA40B2541, 35);
  mock_zip_add_file(&zip, "game.bin", 0x5AA654C0, 96);
  mock_zip_add_file(&zip, "save.bin", 0xFF000000, 1);
  mock_zip_add_file(&zip, "interp_s2_ArduboyFX.hex", 0x50648360, 71);
  mock_zip_add_file(&zip, "screenshot.png", 0x30056694, 48);
  zip_size = mock_zip_finalize(&zip, "");
  ASSERT_NUM_LESS_EQUALS(zip_size, sizeof(zip_contents));

  mock_file(0, "game.arduboy", zip_contents, zip_size);

  /* test file hash */
  result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.arduboy");

  /* test file identification from iterator */
  struct rc_hash_iterator iterator;
  rc_hash_initialize_iterator(&iterator, "game.arduboy", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_msdos_dosz()
{
  char hash_file[33], hash_iterator[33];
  mock_zip_file_t zip;
  uint8_t zip_contents[512];
  size_t zip_size;
  const char* expected_md5 = "59a255662262f5ada32791b8c36e8ea7";
  int result_file, result_iterator;

  memset(&zip, 0, sizeof(zip));
  zip.ptr = zip.buffer = zip_contents;
  mock_zip_add_file(&zip, "FOLDER/", 0, 0);
  mock_zip_add_file(&zip, "FOLDER/SUB.TXT", 0x4AD0CF31, 1);
  mock_zip_add_file(&zip, "ROOT.TXT", 0xD3D99E8B, 1);
  zip_size = mock_zip_finalize(&zip, "TORRENTZIPPED-FD07C52C");
  ASSERT_NUM_LESS_EQUALS(zip_size, sizeof(zip_contents));

  mock_file(0, "game.dosz", zip_contents, zip_size);

  /* test file hash */
  result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_MS_DOS, "game.dosz");

  /* test file identification from iterator */
  struct rc_hash_iterator iterator;
  rc_hash_initialize_iterator(&iterator, "game.dosz", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_msdos_dosz_zip64()
{
  char hash_file[33];
  mock_zip_file_t zip;
  uint8_t zip_contents[512];
  size_t zip_size;
  const char* expected_md5 = "927dad0a57a2860267ab7bcdb8bc3f61";
  int result_file;

  memset(&zip, 0, sizeof(zip));
  zip.ptr = zip.buffer = zip_contents;
  zip.is_zip64 = 1;
  mock_zip_add_file(&zip, "README", 0x69FFE77E, 36);
  zip_size = mock_zip_finalize(&zip, "");
  ASSERT_NUM_LESS_EQUALS(zip_size, sizeof(zip_contents));

  mock_file(0, "game.dosz", zip_contents, zip_size);

  /* test file hash */
  result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_MS_DOS, "game.dosz");

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
}

static void test_hash_msdos_dosz_with_dosc()
{
  char hash_dosc[33];
  const char* expected_dosc_md5 = "dd0c0b0c170c30722784e5e962764c35";
  mock_zip_file_t zip;
  uint8_t zip_contents[512];
  size_t zip_size;
  int result_dosc;

  memset(&zip, 0, sizeof(zip));
  zip.ptr = zip.buffer = zip_contents;
  mock_zip_add_file(&zip, "FOLDER/", 0, 0);
  mock_zip_add_file(&zip, "FOLDER/SUB.TXT", 0x4AD0CF31, 1);
  mock_zip_add_file(&zip, "ROOT.TXT", 0xD3D99E8B, 1);
  zip_size = mock_zip_finalize(&zip, "TORRENTZIPPED-FD07C52C");
  ASSERT_NUM_LESS_EQUALS(zip_size, sizeof(zip_contents));

  /* Add main dosz file and overlay dosc file which will get hashed together */
  /* Just use the same file for both to simplify the test */
  mock_file(0, "game.dosz", zip_contents, zip_size);
  mock_file(1, "game.dosc", zip_contents, zip_size);

  /* test file hash */
  result_dosc = rc_hash_generate_from_file(hash_dosc, RC_CONSOLE_MS_DOS, "game.dosz");

  /* validation */
  ASSERT_NUM_EQUALS(result_dosc, 1);
  ASSERT_STR_EQUALS(hash_dosc, expected_dosc_md5);
}

static void test_hash_msdos_dosz_with_parent()
{
  char hash_dosz[33], hash_dosc[33], hash_dosc2[33];
  const char* expected_dosz_md5 = "623c759476b8b5adb46362f8f0b60769";
  const char* expected_dosc_md5 = "ecd9d776cbaad63094829d7b8dbe5959";
  const char* expected_dosc2_md5 = "cb55c123936ad84479032ea6444cb1a1";
  mock_zip_file_t dosz, dosc;
  uint8_t dosz_contents[512], dosc_contents[512];
  size_t dosz_size, dosc_size;
  int result_dosz, result_dosc, result_dosc2;

  memset(&dosz, 0, sizeof(dosz));
  dosz.ptr = dosz.buffer = dosz_contents;
  mock_zip_add_file(&dosz, "FOLDER/", 0, 0);
  mock_zip_add_file(&dosz, "FOLDER/SUB.TXT", 0x4AD0CF31, 1);
  mock_zip_add_file(&dosz, "ROOT.TXT", 0xD3D99E8B, 1);
  dosz_size = mock_zip_finalize(&dosz, "TORRENTZIPPED-FD07C52C");
  ASSERT_NUM_LESS_EQUALS(dosz_size, sizeof(dosz_contents));

  memset(&dosc, 0, sizeof(dosz));
  dosc.ptr = dosc.buffer = dosc_contents;
  mock_zip_add_file(&dosc, "base.dosz.parent", 0, 0);
  mock_zip_add_file(&dosc, "CHILD.TXT", 0x22B35429, 5);
  dosc_size = mock_zip_finalize(&dosc, "");
  ASSERT_NUM_LESS_EQUALS(dosz_size, sizeof(dosc_contents));

  /* Add base dosz file and child dosz file which will get hashed together */
  mock_file(0, "base.dosz", dosz_contents, dosz_size);
  mock_file(1, "child.dosz", dosc_contents, dosc_size);

  /* test file hash */
  result_dosz = rc_hash_generate_from_file(hash_dosz, RC_CONSOLE_MS_DOS, "child.dosz");

  /* test file hash with base.dosc also existing */
  mock_file(2, "base.dosc", dosz_contents, dosz_size);
  result_dosc = rc_hash_generate_from_file(hash_dosc, RC_CONSOLE_MS_DOS, "child.dosz");

  /* test file hash with child.dosc also existing */
  mock_file(3, "child.dosc", dosz_contents, dosz_size);
  result_dosc2 = rc_hash_generate_from_file(hash_dosc2, RC_CONSOLE_MS_DOS, "child.dosz");

  /* validation */
  ASSERT_NUM_EQUALS(result_dosz, 1);
  ASSERT_NUM_EQUALS(result_dosc, 1);
  ASSERT_NUM_EQUALS(result_dosc2, 1);
  ASSERT_STR_EQUALS(hash_dosz, expected_dosz_md5);
  ASSERT_STR_EQUALS(hash_dosc, expected_dosc_md5);
  ASSERT_STR_EQUALS(hash_dosc2, expected_dosc2_md5);
}

/* ========================================================================= */

void test_hash_zip(void) {
  TEST_SUITE_BEGIN();

  /* Arduboy FX */
  TEST(test_hash_arduboy_fx);

  /* MS DOS */
  TEST(test_hash_msdos_dosz);
  TEST(test_hash_msdos_dosz_zip64);
  TEST(test_hash_msdos_dosz_with_dosc);
  TEST(test_hash_msdos_dosz_with_parent);

  TEST_SUITE_END();
}
