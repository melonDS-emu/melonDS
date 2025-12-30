#include "data.h"

#include "../src/rc_compat.h"

#include <stdlib.h>
#include <string.h>

void fill_image(uint8_t* image, size_t size)
{
  int seed = (int)(size ^ (size >> 8) ^ ((size - 1) * 25387));
  int count;
  uint8_t value;

  while (size > 0)
  {
    switch (seed & 0xFF)
    {
      case 0:
        count = (((seed >> 8) & 0x3F) & ~(size & 0x0F));
        if (count == 0)
          count = 1;
        value = 0;
        break;

      case 1:
        count = ((seed >> 8) & 0x07) + 1;
        value = ((seed >> 16) & 0xFF);
        break;

      case 2:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0xFF;
        break;

      case 3:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0xA5;
        break;

      case 4:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0xC3;
        break;

      case 5:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0x96;
        break;

      case 6:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0x78;
        break;

      case 7:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0x78;
        break;

      default:
        count = 1;
        value = ((seed >> 8) ^ (seed >> 16)) & 0xFF;
        break;
    }

    do
    {
      *image++ = value;
      --size;
    } while (size && --count);

    /* state mutation from psuedo-random number generator */
    seed = (seed * 0x41C64E6D + 12345) & 0x7FFFFFFF;
  }
}

uint8_t* generate_gamecube_iso(size_t mb, size_t* image_size)
{
  uint8_t* image;
  const size_t size_needed = mb * 1024 * 1024;
  int ix;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL)
  {
    uint32_t apploader_sizes_addr = 0x2440 + 0x14;
    uint32_t dol_offset_addr = 0x420;
    uint32_t dol_sizes_addr = 0x3000;

    fill_image(image, size_needed);

    image[0x1c] = 0xC2;
    image[0x1d] = 0x33;
    image[0x1e] = 0x9F;
    image[0x1f] = 0x3D;

    for (ix = 0; ix < 8; ix++)
    {
      /* 0x000000ff for both */
      image[apploader_sizes_addr + ix] = (ix % 4 == 3) ? 0xff : 0;
    }
    for (ix = 0; ix < 4; ix++)
    {
      /* 0x00003000 */
      image[dol_offset_addr + ix] = (ix % 4 == 2) ? 0x30 : 0; 
    }
    for (ix = 0; ix < 18 * 4; ix++)
    {
      /* offsets start at 0x00003100 and increment */
      image[dol_sizes_addr + ix] = (ix % 4 == 2) ? (0x30 + 1 + ix / 4) : 0; 
      /* 0x000000ff for every other size */
      image[dol_sizes_addr + 0x90 + ix] = (ix % 8 == 3) ? 0xff : 0; 
    }
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

uint8_t* generate_3do_bin(uint32_t root_directory_sectors, uint32_t binary_size, size_t* image_size)
{
  const uint8_t volume_header[] = {
    0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x01, 0x00, /* header */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* comment */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    'C', 'D', '-', 'R', 'O', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* label */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x2D, 0x79, 0x6E, 0x96, /* identifier */
    0x00, 0x00, 0x08, 0x00, /* block size */
    0x00, 0x00, 0x05, 0x00, /* block count */
    0x31, 0x5a, 0xf2, 0xe6, /* root directory identifier */
    0x00, 0x00, 0x00, 0x01, /* root directory size in blocks */
    0x00, 0x00, 0x08, 0x00, /* block size in root directory */
    0x00, 0x00, 0x00, 0x06, /* number of copies of root directory */
    0x00, 0x00, 0x00, 0x01, /* block location of root directory */
    0x00, 0x00, 0x00, 0x01, /* block location of first copy of root directory */
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, /* block location of last copy of root directory */
  };

  const uint8_t directory_data[] = {
    0xFF, 0xFF, 0xFF, 0xFF, /* next block */
    0xFF, 0xFF, 0xFF, 0xFF, /* previous block */
    0x00, 0x00, 0x00, 0x00, /* flags */
    0x00, 0x00, 0x00, 0xA4, /* end of block */
    0x00, 0x00, 0x00, 0x14, /* start of block */

    0x00, 0x00, 0x00, 0x07, /* flags - directory */
    0x00, 0x00, 0x00, 0x00, /* identifier */
    0x00, 0x00, 0x00, 0x00, /* type */
    0x00, 0x00, 0x08, 0x00, /* block size */
    0x00, 0x00, 0x00, 0x00, /* length in bytes */
    0x00, 0x00, 0x00, 0x00, /* length in blocks */
    0x00, 0x00, 0x00, 0x00, /* burst */
    0x00, 0x00, 0x00, 0x00, /* gap */
    'f', 'o', 'l', 'd', 'e', 'r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* filename */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00, /* extra copies */
    0x00, 0x00, 0x00, 0x00, /* directory block address */

    0x00, 0x00, 0x00, 0x02, /* flags - file */
    0x00, 0x00, 0x00, 0x00, /* identifier */
    0x00, 0x00, 0x00, 0x00, /* type */
    0x00, 0x00, 0x08, 0x00, /* block size */
    0x00, 0x00, 0x00, 0x00, /* length in bytes */
    0x00, 0x00, 0x00, 0x00, /* length in blocks */
    0x00, 0x00, 0x00, 0x00, /* burst */
    0x00, 0x00, 0x00, 0x00, /* gap */
    'L', 'a', 'u', 'n', 'c', 'h', 'M', 'e', 0, 0, 0, 0, 0, 0, 0, 0, /* filename */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00, /* extra copies */
    0x00, 0x00, 0x00, 0x02, /* directory block address */
  };

  size_t size_needed = (root_directory_sectors + 1 + ((binary_size + 2047) / 2048)) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);
  size_t offset = 2048;
  uint32_t i;

  if (!image)
    return NULL;

  /* first sector - volume header */
  memcpy(image, volume_header, sizeof(volume_header));
  image[0x5B] = (uint8_t)root_directory_sectors;

  /* root directory sectors */
  for (i = 0; i < root_directory_sectors; ++i)
  {
    memcpy(&image[offset], directory_data, sizeof(directory_data));
    if (i < root_directory_sectors - 1)
    {
      image[offset + 0] = 0;
      image[offset + 1] = 0;
      image[offset + 2] = 0;
      image[offset + 3] = (uint8_t)(i + 1);

      memcpy(&image[offset + 0x14 + 0x48 + 0x20], "filename", 8);
    }
    else
    {
      image[offset + 0x14 + 0x48 + 0x11] = (binary_size >> 16) & 0xFF;
      image[offset + 0x14 + 0x48 + 0x12] = (binary_size >> 8) & 0xFF;
      image[offset + 0x14 + 0x48 + 0x13] = (binary_size & 0xFF);

      image[offset + 0x14 + 0x48 + 0x16] = (((binary_size + 2047) / 2048) >> 8) & 0xFF;
      image[offset + 0x14 + 0x48 + 0x17] = ((binary_size + 2047) / 2048) & 0xFF;

      image[offset + 0x14 + 0x48 + 0x47] = (uint8_t)(i + 2);
    }

    if (i > 0)
    {
      image[offset + 4] = 0;
      image[offset + 5] = 0;
      image[offset + 6] = 0;
      image[offset + 7] = (uint8_t)(i - 1);
    }

    offset += 2048;
  }

  /* binary data */
  fill_image(&image[offset], binary_size);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_dreamcast_bin(uint32_t track_first_sector, uint32_t binary_size, size_t* image_size)
{
  /* https://mc.pp.se/dc/ip0000.bin.html */
  const uint8_t volume_header[] =
    "SEGA SEGAKATANA "
    "SEGA ENTERPRISES"
    "5966 GD-ROM1/1  " /* device info */
    " U      918FA01 " /* region and peripherals */
    "X-1234N   V1.001" /* product number and version */
    "20200910        " /* release date */
    "1ST_READ.BIN    " /* boot file */
    "RETROACHIEVEMENT" /* company name */
    "UNIT TEST       " /* product name */
    "                "
    "                "
    "                "
    "                "
    "                "
    "                "
    "                ";

  const uint8_t directory_data[] = {
    0x30, /* length of directory record */
    0x00, /* extended attribute record length */
    0xD9, 0xAF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* first logical block of file */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* length in bytes */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* date/time */
    0x00, 0x00, 0x00, /* flags, unit size, gap size */
    0x00, 0x00, 0x00, 0x00, /* sequence number*/
    0x0E, /* length of file identifier */
    '1', 'S', 'T', '_', 'R', 'E', 'A', 'D', '.', 'B', 'I', 'N', ';', '1', /* file identifier */
  };

  const size_t binary_sectors = (binary_size + 2047) / 2048;
  const size_t size_needed = (binary_sectors + 18) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);
  if (!image)
    return NULL;

  /* volume header goes in sector 0 */
  memcpy(&image[0], volume_header, sizeof(volume_header));

  /* directory information goes in sectors 16 and 17 */
  memcpy(&image[2048 * 16], "1CD001", 6);
  image[2048 * 16 + 156 + 2] = 45017 & 0xFF;
  image[2048 * 16 + 156 + 3] = (45017 >> 8) & 0xFF;
  image[2048 * 16 + 156 + 4] = (45017 >> 16) & 0xFF;
  memcpy(&image[2048 * 17], directory_data, sizeof(directory_data));

  track_first_sector += 18;
  image[2048 * 17 + 2] = (track_first_sector & 0xFF);
  image[2048 * 17 + 3] = ((track_first_sector >> 8) & 0xFF);
  image[2048 * 17 + 4] = ((track_first_sector >> 16) & 0xFF);
  image[2048 * 17 + 10] = (binary_size & 0xFF);
  image[2048 * 17 + 11] = ((binary_size >> 8) & 0xFF);
  image[2048 * 17 + 12] = ((binary_size >> 16) & 0xFF);
  image[2048 * 17 + 13] = ((binary_size >> 24) & 0xFF);

  /* binary data */
  fill_image(&image[2048 * 18], binary_sectors * 2048);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_pce_cd_bin(uint32_t binary_sectors, size_t* image_size)
{
  const uint8_t volume_header[] = {
    0x00, 0x00, 0x02,       /* first sector of boot code */
    0x14,                   /* number of sectors for boot code */
    0x00, 0x40,             /* program load address */
    0x00, 0x40,             /* program execute address  */
    0, 1, 2, 3, 4,          /* IPLMPR */
    0,                      /* open mode */
    0, 0, 0, 0, 0, 0,       /* GRPBLK and addr */
    0, 0, 0, 0, 0,          /* ADPBLK and rate */
    0, 0, 0, 0, 0, 0, 0,    /* reserved */
    'P', 'C', ' ', 'E', 'n', 'g', 'i', 'n', 'e', ' ', 'C', 'D', '-', 'R', 'O', 'M',
    ' ', 'S', 'Y', 'S', 'T', 'E', 'M', '\0', 'C', 'o', 'p', 'y', 'r', 'i', 'g', 'h',
    't', ' ', 'H', 'U', 'D', 'S', 'O', 'N', ' ', 'S', 'O', 'F', 'T', ' ', '/', ' ',
    'N', 'E', 'C', ' ', 'H', 'o', 'm', 'e', ' ', 'E', 'l', 'e', 'c', 't', 'r', 'o',
    'n', 'i', 'c', 's', ',', 'L', 't', 'd', '.', '\0', 'G', 'A', 'M', 'E', 'N', 'A',
    'M', 'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
  };

  size_t size_needed = (binary_sectors + 2) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);
  if (!image)
    return NULL;

  /* volume header goes in second sector */
  memcpy(&image[2048], volume_header, sizeof(volume_header));
  image[2048 + 0x03] = (uint8_t)binary_sectors;

  /* binary data */
  fill_image(&image[4096], binary_sectors * 2048);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_pcfx_bin(uint32_t binary_sectors, size_t* image_size)
{
  const uint8_t volume_header[] = {
    'G', 'A', 'M', 'E', 'N', 'A', 'M', 'E', 0, 0, 0, 0, 0, 0, 0, 0, /* title (32-bytes) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x02, 0x00, 0x00, 0x00, /* first sector of boot code */
    0x14, 0x00, 0x00, 0x00, /* number of sectors for boot code */
    0x00, 0x80, 0x00, 0x00, /* program load address */
    0x00, 0x80, 0x00, 0x00, /* program execute address  */
    'N', '/', 'A', '\0',    /* maker id */
    'r', 'c', 'h', 'e', 'e', 'v', 'o', 's', 't', 'e', 's', 't', 0, 0, 0, 0, /* maker name (60-bytes) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00, /* volume number */
    0x00, 0x01,             /* version */
    0x01, 0x00,             /* country */
    '2', '0', '2', '0', 'X', 'X', 'X', 'X', /* date */
  };

  size_t size_needed = (binary_sectors + 2) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);
  if (!image)
    return NULL;

  /* volume header goes in second sector */
  strcpy_s((char*)&image[0], size_needed, "PC-FX:Hu_CD-ROM");
  memcpy(&image[2048], volume_header, sizeof(volume_header));
  image[2048 + 0x24] = (uint8_t)binary_sectors;

  /* binary data */
  fill_image(&image[4096], binary_sectors * 2048);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_iso9660_bin(uint32_t num_sectors, const char* volume_label, size_t* image_size)
{
  const uint8_t identifier[] = { 0x01, 'C', 'D', '0', '0', '1', 0x01, 0x00 };
  uint8_t* volume_descriptor;;
  uint8_t* image;
  
  *image_size = num_sectors * 2048;
  image = calloc(*image_size, 1);
  if (!image)
    return NULL;

  volume_descriptor = &image[16 * 2048];

  /* CD001 identifier */
  memcpy(volume_descriptor, identifier, 8);

  /* volume label */
  memcpy(&volume_descriptor[40], volume_label, strlen(volume_label));

  /* number of sectors (little endian, then big endian) */
  volume_descriptor[80] = image[87] = num_sectors & 0xFF;
  volume_descriptor[81] = image[86] = (num_sectors >> 8) & 0xFF;
  volume_descriptor[82] = image[85] = (num_sectors >> 16) & 0xFF;
  volume_descriptor[83] = image[84] = (num_sectors >> 24) & 0xFF;

  /* size of each sector */
  volume_descriptor[128] = (2048) & 0xFF;
  volume_descriptor[129] = (2048 >> 8) & 0xFF;

  /* root directory record location */
  volume_descriptor[158] = 17;

  /* helper for tracking next free sector - not actually part of iso9660 spec */
  image[17 * 2048 - 4] = 18;

  return image;
}

uint8_t* generate_iso9660_file(uint8_t* image, const char* filename, const uint8_t* contents, size_t contents_size)
{
  const uint32_t root_directory_record_offset = 17 * 2048;
  uint8_t* file_entry_start = &image[root_directory_record_offset];
  uint8_t* file_contents_start;
  size_t filename_len;
  uint32_t next_free_sector = image[root_directory_record_offset - 4] |
      (image[root_directory_record_offset - 3] << 8) | (image[root_directory_record_offset - 2] << 16);
  const char* separator;

  /* we start at the root. ignore explicit root path */
  if (*filename == '\\')
    ++filename;

  /* handle subdirectories */
  do
  {
    separator = filename;
    while (*separator && *separator != '\\')
      ++separator;

    if (!*separator)
      break;

    filename_len = separator - filename;
    int found = 0;
    while (*file_entry_start)
    {
      if (file_entry_start[25] && /* is directory */
          file_entry_start[33 + filename_len] == '\0' && memcmp(&file_entry_start[33], filename, filename_len) == 0)
      {
        const uint32_t directory_sector = file_entry_start[2];
        file_entry_start = &image[directory_sector * 2048];
        found = 1;
        break;
      }

      file_entry_start += *file_entry_start;
    }

    if (!found)
    {
      /* entry size*/
      file_entry_start[0] = (filename_len & 0xFF) + 48;

      /* directory sector */
      file_entry_start[2] = next_free_sector & 0xFF;
      file_entry_start[3] = (next_free_sector >> 8) & 0xFF;

      /* is directory */
      file_entry_start[25] = 1;

      /* directory name */
      file_entry_start[32] = filename_len & 0xFF;
      memcpy(&file_entry_start[33], filename, filename_len);
      file_entry_start[33 + filename_len] = '\0';

      /* advance to next sector */
      file_entry_start = &image[next_free_sector * 2048];
      next_free_sector++;
    }

    filename = separator + 1;
  } while (1);

  /* skip over any items already in the directory */
  while (*file_entry_start)
    file_entry_start += *file_entry_start;

  /* create the new entry */

  /* entry size*/
  filename_len = separator - filename;
  file_entry_start[0] = (filename_len & 0xFF) + 48;

  /* file sector */
  file_entry_start[2] = next_free_sector & 0xFF;
  file_entry_start[3] = (next_free_sector >> 8) & 0xFF;

  /* file size */
  file_entry_start[10] = contents_size & 0xFF;
  file_entry_start[11] = (contents_size >> 8) & 0xFF;
  file_entry_start[12] = (contents_size >> 16) & 0xFF;

  /* file name */
  file_entry_start[32] = (filename_len + 2) & 0xFF;
  memcpy(&file_entry_start[33], filename, filename_len);
  file_entry_start[33 + filename_len] = ';';
  file_entry_start[34 + filename_len] = '1';

  /* contents */
  file_contents_start = &image[next_free_sector * 2048];

  if (contents)
    memcpy(file_contents_start, contents, contents_size);
  else
    fill_image(file_contents_start, contents_size);

  /* update next free sector */
  next_free_sector += (unsigned)(contents_size + 2047) / 2048;
  image[root_directory_record_offset - 4] = (next_free_sector & 0xFF);
  image[root_directory_record_offset - 3] = (next_free_sector >> 8) & 0xFF;
  image[root_directory_record_offset - 2] = (next_free_sector >> 16) & 0xFF;

  /* return pointer to contents so caller can modify if desired */
  return file_contents_start;
}

uint8_t* generate_jaguarcd_bin(uint32_t header_offset, uint32_t binary_size, int byteswapped, size_t* image_size)
{
  size_t size_needed = (((binary_size + 64 + 32 + 8) + 2351) / 2352) * 2352;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);
  size_t i;

  if (!image)
    return NULL;

  /* header is 64 bytes of ATRI repeating followed by approved data message, load address, and binary size */
  for (i = 0; i < 64; i += 4)
      memcpy(&image[header_offset + i], "ATRI", 4);
  memcpy(&image[header_offset + 64], "ATARI APPROVED DATA HEADER ATRI ", 32);
  image[header_offset + 64 + 32 + 2] = 0xA0;
  image[header_offset + 64 + 32 + 4 + 1] = (binary_size >> 16);
  image[header_offset + 64 + 32 + 4 + 2] = (binary_size >> 8) & 0xFF;
  image[header_offset + 64 + 32 + 4 + 3] = (binary_size & 0xFF);

  /* binary data */
  fill_image(&image[header_offset + 64 + 32 + 8], size_needed - (header_offset + 64 + 32 + 8));

  if (byteswapped)
  {
      for (i = 0; i < size_needed; i += 2)
      {
          uint8_t tmp = image[i];
          image[i] = image[i + 1];
          image[i + 1] = tmp;
      }
  }

  *image_size = size_needed;
  return image;
}

uint8_t* generate_psx_bin(const char* binary_name, uint32_t binary_size, size_t* image_size)
{
  const uint32_t sectors_needed = (((binary_size + 2047) / 2048) + 20);
  char system_cnf[256];
  uint8_t* image;
  uint8_t* exe;

  snprintf(system_cnf, sizeof(system_cnf), "BOOT=cdrom:\\%s;1\nTCB=4\nEVENT=10\nSTACK=801FFFF0\n", binary_name);

  image = generate_iso9660_bin(sectors_needed, "TEST", image_size);
  generate_iso9660_file(image, "SYSTEM.CNF", (const uint8_t*)system_cnf, strlen(system_cnf));

  /* binary data */
  exe = generate_iso9660_file(image, binary_name, NULL, binary_size);
  memcpy(exe, "PS-X EXE", 8);

  binary_size -= 2048;
  exe[28] = binary_size & 0xFF;
  exe[29] = (binary_size >> 8) & 0xFF;
  exe[30] = (binary_size >> 16) & 0xFF;
  exe[31] = (binary_size >> 24) & 0xFF;

  return image;
}

uint8_t* generate_ps2_bin(const char* binary_name, uint32_t binary_size, size_t* image_size)
{
  const uint32_t sectors_needed = (((binary_size + 2047) / 2048) + 20);
  char system_cnf[256];
  uint8_t* image;
  uint8_t* exe;

  snprintf(system_cnf, sizeof(system_cnf), "BOOT2 = cdrom0:\\%s;1\nVER = 1.0\nVMODE = NTSC\n", binary_name);

  image = generate_iso9660_bin(sectors_needed, "TEST", image_size);
  generate_iso9660_file(image, "SYSTEM.CNF", (const uint8_t*)system_cnf, strlen(system_cnf));

  /* binary data */
  exe = generate_iso9660_file(image, binary_name, NULL, binary_size);
  memcpy(exe, "\x7f\x45\x4c\x46", 4);

  return image;
}

uint8_t* generate_generic_file(size_t size)
{
  uint8_t* image;
  image = (uint8_t*)calloc(size, 1);
  if (image != NULL)
    fill_image(image, size);

  return image;
}

uint8_t* convert_to_2352(uint8_t* input, size_t* size, uint32_t first_sector)
{
    const uint32_t num_sectors = (uint32_t)((*size + 2047) / 2048);
    const uint32_t output_size = num_sectors * 2352;
    const uint8_t sync_pattern[] = {
      0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
    };
    uint8_t* output = (uint8_t*)malloc(output_size);
    uint8_t* input_ptr = input;
    uint8_t* ptr = output;
    uint8_t minutes, seconds, frames;
    uint32_t i;

    first_sector += 150;
    frames = (first_sector % 75);
    first_sector /= 75;
    seconds = (first_sector % 60);
    minutes = first_sector / 60;

    for (i = 0; i < num_sectors; i++)
    {
      /* 16 - byte sync header */
      memcpy(ptr, sync_pattern, 12);
      ptr += 12;
      *ptr++ = ((minutes / 10) << 4) | (minutes % 10);
      *ptr++ = ((seconds / 10) << 4) | (seconds % 10);
      *ptr++ = ((frames / 10) << 4) | (frames % 10);
      if (++frames == 75)
      {
        frames = 0;
        if (++seconds == 60)
        {
          seconds = 0;
          ++minutes;
        }
      }
      *ptr++ = 2;

      /* 2048 bytes data */
      memcpy(ptr, input_ptr, 2048);
      input_ptr += 2048;

      /* 288 bytes parity / checksums */
      ptr += 2352 - 16;
    }

    free(input);
    *size = output_size;
    return output;
}

