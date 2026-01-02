#include "../rc_libretro.h"

#include "../rc_compat.h"
#include "rc_consoles.h"

#include "test_framework.h"
#include "rhash/mock_filereader.h"

static void* retro_memory_data[4] = { NULL, NULL, NULL, NULL };
static size_t retro_memory_size[4] = { 0, 0, 0, 0 };

static void libretro_get_core_memory_info(uint32_t id, rc_libretro_core_memory_info_t* info)
{
  info->data = retro_memory_data[id];
  info->size = retro_memory_size[id];
}

static int libretro_get_image_path(uint32_t index, char* buffer, size_t buffer_size)
{
  if (index < 0 || index > 9)
    return 0;

  snprintf(buffer, buffer_size, "save%d.dsk", index);
  return 1;
}

static void test_allowed_setting(const char* library_name, const char* setting, const char* value) {
  const rc_disallowed_setting_t* settings = rc_libretro_get_disallowed_settings(library_name);
  if (!settings)
    return;

  ASSERT_TRUE(rc_libretro_is_setting_allowed(settings, setting, value));
}

static void test_disallowed_setting(const char* library_name, const char* setting, const char* value) {
  const rc_disallowed_setting_t* settings = rc_libretro_get_disallowed_settings(library_name);
  ASSERT_PTR_NOT_NULL(settings);
  ASSERT_FALSE(rc_libretro_is_setting_allowed(settings, setting, value));
}

static void test_allowed_system(const char* library_name, uint32_t console_id) {
  ASSERT_TRUE(rc_libretro_is_system_allowed(library_name, console_id));
}

static void test_disallowed_system(const char* library_name, uint32_t console_id) {
  ASSERT_FALSE(rc_libretro_is_system_allowed(library_name, console_id));
}

static void test_memory_init_without_regions() {
  rc_libretro_memory_regions_t regions;
  uint32_t avail;
  #define BUFFER1_SIZE 16
  #define BUFFER2_SIZE 8
  /* put explicit gap between buffer1 and buffer2 to prevent them from being merged when building regions */
  uint8_t buffer[BUFFER1_SIZE + 4 + BUFFER2_SIZE];
  uint8_t* buffer1 = &buffer[0];
  uint8_t* buffer2 = &buffer[20];
  uint8_t buffer3[4];
  uint32_t i;

  for (i = 0; i < BUFFER1_SIZE; ++i)
      buffer1[i] = i;
  for (i = 0; i < BUFFER2_SIZE; ++i)
      buffer2[i] = i;
  for (i = 0; i < sizeof(buffer3); ++i)
      buffer3[i] = i;

  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = BUFFER1_SIZE;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = BUFFER2_SIZE;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, BUFFER1_SIZE + BUFFER2_SIZE);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 2), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, BUFFER1_SIZE + 2), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, BUFFER1_SIZE + BUFFER2_SIZE + 2));

  ASSERT_PTR_EQUALS(rc_libretro_memory_find_avail(&regions, 2, &avail), &buffer1[2]);
  ASSERT_NUM_EQUALS(avail, BUFFER1_SIZE - 2);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find_avail(&regions, BUFFER1_SIZE - 1, &avail), &buffer1[BUFFER1_SIZE - 1]);
  ASSERT_NUM_EQUALS(avail, 1);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find_avail(&regions, BUFFER1_SIZE + 2, &avail), &buffer2[2]);
  ASSERT_NUM_EQUALS(avail, BUFFER2_SIZE - 2);
  ASSERT_PTR_NULL(rc_libretro_memory_find_avail(&regions, BUFFER1_SIZE + BUFFER2_SIZE + 2, &avail));
  ASSERT_NUM_EQUALS(avail, 0);

  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 2, buffer3, 1), 1);
  ASSERT_TRUE(memcmp(buffer3, &buffer1[2], 1) == 0);
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 7, buffer3, 4), 4);
  ASSERT_TRUE(memcmp(buffer3, &buffer1[7], 4) == 0);
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, BUFFER1_SIZE - 2, buffer3, 3), 3); /* read across boundary */
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, BUFFER1_SIZE + BUFFER2_SIZE + 2, buffer3, 1), 0);

  #undef BUFFER2_SIZE
  #undef BUFFER1_SIZE
}

static void test_memory_init_without_regions_system_ram_only() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[16];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = sizeof(buffer1);
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, sizeof(buffer1));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 2), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, sizeof(buffer1) + 2));
}

static void test_memory_init_without_regions_save_ram_only() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = sizeof(buffer2);

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, sizeof(buffer2));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 2), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, sizeof(buffer2) + 2));
}

static void test_memory_init_without_regions_no_ram() {
  rc_libretro_memory_regions_t regions;
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_FALSE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 0);
  ASSERT_NUM_EQUALS(regions.total_size, 0);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 2));
}

static void test_memory_init_from_unmapped_memory() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8], buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x10000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0x10000;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_null_filler() {
  rc_libretro_memory_regions_t regions;
  uint32_t avail;
  uint8_t buffer1[16], buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = sizeof(buffer1);
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = sizeof(buffer2);

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 4); /* two valid regions and two null fillers */
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00012));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x1000A));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));

  ASSERT_PTR_EQUALS(rc_libretro_memory_find_avail(&regions, 0x00002, &avail), &buffer1[2]);
  ASSERT_NUM_EQUALS(avail, sizeof(buffer1) - 2);
  ASSERT_PTR_NULL(rc_libretro_memory_find_avail(&regions, 0x00012, &avail));
  ASSERT_NUM_EQUALS(avail, 0);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find_avail(&regions, 0x10002, &avail), &buffer2[2]);
  ASSERT_NUM_EQUALS(avail, sizeof(buffer2) - 2);
  ASSERT_PTR_NULL(rc_libretro_memory_find_avail(&regions, 0x1000A, &avail));
  ASSERT_NUM_EQUALS(avail, 0);
}

static void test_memory_init_from_unmapped_memory_no_save_ram() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[16];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x10000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_merge_neighbors() {
  rc_libretro_memory_regions_t regions;
  uint8_t* buffer1 = (uint8_t*)malloc(0x10000); /* have to malloc to prevent array-bounds compiler warnings */
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x10000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_ATARI_LYNX));

  ASSERT_NUM_EQUALS(regions.count, 1); /* all regions are adjacent, so should be merged */
  ASSERT_NUM_EQUALS(regions.total_size, 0x10000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0002), &buffer1[0x0002]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0102), &buffer1[0x0102]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0xFFFF), &buffer1[0xFFFF]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10000));

  free(buffer1);
}

static void test_memory_init_from_unmapped_memory_no_ram() {
  rc_libretro_memory_regions_t regions;
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  /* init returns false */
  ASSERT_FALSE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  /* but one null-filled region is still generated */
  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_ds() {
  rc_libretro_memory_regions_t regions;
  uint8_t* buffer1 = (uint8_t*)malloc(0x02000000); /* have to malloc to prevent array-bounds compiler warnings */
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x02000000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  /* init returns true */
  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_NINTENDO_DS));

  /* primary region should be generated, but TCM won't be available */
  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x1004000);
  ASSERT_PTR_NOT_NULL(rc_libretro_memory_find(&regions, 0x00000002));
  ASSERT_PTR_NOT_NULL(rc_libretro_memory_find(&regions, 0x003FFFFF));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00400000));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x01000002));

  free(buffer1);
}

static void test_memory_init_from_unmapped_memory_wii() {
  rc_libretro_memory_regions_t regions;
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  /* init returns false */
  ASSERT_FALSE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_WII));

  /* but one null-filled region is still generated */
  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, 0x14000000);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00000002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x14000002));
}

static void test_memory_init_from_memory_map() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" },
    { RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_null_filler() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" },
    { RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_no_save_ram() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_merge_neighbors() {
  rc_libretro_memory_regions_t regions;
  uint8_t* buffer1 = (uint8_t*)malloc(0x10000); /* have to malloc to prevent array-bounds compiler warnings */
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0x0000], 0, 0x0000U, 0, 0, 0xFC00, "RAM" },
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0xFC00], 0, 0xFC00U, 0, 0, 0x0400, "Hardware controllers" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_ATARI_LYNX));

  ASSERT_NUM_EQUALS(regions.count, 1); /* all regions are adjacent, so should be merged */
  ASSERT_NUM_EQUALS(regions.total_size, 0x10000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0002), &buffer1[0x0002]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0102), &buffer1[0x0102]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0xFFFF), &buffer1[0xFFFF]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10000));

  free(buffer1);
}

static void test_memory_init_from_memory_map_no_ram() {
  rc_libretro_memory_regions_t regions;
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, NULL, 0, 0xFF0000U, 0, 0, 0x10000, "RAM" },
    { RETRO_MEMDESC_SAVE_RAM,   NULL, 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  /* init returns false */
  ASSERT_FALSE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  /* but one null-filled region is still generated */
  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_splice() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8], buffer2[8], buffer3[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x08000, "RAM1" },
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer2[0], 0, 0xFF8000U, 0, 0, 0x08000, "RAM2" },
    { RETRO_MEMDESC_SAVE_RAM,   &buffer3[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 3);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x08002), &buffer2[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer3[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_mirrored() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0xFF0000U, 0x00C000U, 0x04000, "RAM" },
    { RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0x000000U, 0x000000U, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  /* select of 0xFF0000 should mirror the 0x4000 bytes at 0xFF0000 into 0xFF4000, 0xFF8000, and 0xFFC000 */
  ASSERT_NUM_EQUALS(regions.count, 5);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x04002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x08002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0C002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_out_of_order() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" },
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_disconnect_gaps() {
  rc_libretro_memory_regions_t regions;
  uint8_t buffer[256];
  /* the disconnect bit is smaller than the region size, so only parts of the memory map
   * will be filled by the region. in this case, 00-1F will be buffer[00-1F], but
   * buffer[20-3F] will be associated to addresses 40-5F! */
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer[0], 0, 0x0000, 0xFC20, 0x0020, sizeof(buffer), "RAM" },
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MAGNAVOX_ODYSSEY2));

  ASSERT_NUM_EQUALS(regions.count, 10);
  ASSERT_NUM_EQUALS(regions.total_size, 0x140);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0002), &buffer[0x02]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x0022));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0042), &buffer[0x22]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x0062));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0082), &buffer[0x42]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00A2));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00C2), &buffer[0x62]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00E2));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0102), &buffer[0x82]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x0122));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x0142));
}


static void test_memory_read()
{
  rc_libretro_memory_regions_t regions;
  /* intentionally put buffer3 between buffer1 and buffer2 for the read that spans buffers */
  uint8_t buffer1[8], buffer3[4], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0x000000U, 0, 0, 0x000008, "RAM A" },
    { RETRO_MEMDESC_SYSTEM_RAM, &buffer2[0], 0, 0x000008U, 0, 0, 0x000008, "RAM B" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };
  int i;

  for (i = 0; i < 8; ++i) {
    buffer1[i] = i;
    buffer2[i] = i + 8;
  }

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_NINTENDO));

  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 0, buffer3, 4), 4);
  ASSERT_NUM_EQUALS(buffer3[0], 0);
  ASSERT_NUM_EQUALS(buffer3[1], 1);
  ASSERT_NUM_EQUALS(buffer3[2], 2);
  ASSERT_NUM_EQUALS(buffer3[3], 3);

  /* only requesting two bytes, other two remain unmodified */
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 3, buffer3, 2), 2);
  ASSERT_NUM_EQUALS(buffer3[0], 3);
  ASSERT_NUM_EQUALS(buffer3[1], 4);
  ASSERT_NUM_EQUALS(buffer3[2], 2);
  ASSERT_NUM_EQUALS(buffer3[3], 3);

  /* one two bytes are available in buffer1, the rest have to be read from buffer2 */
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 6, buffer3, 4), 4);
  ASSERT_NUM_EQUALS(buffer3[0], 6);
  ASSERT_NUM_EQUALS(buffer3[1], 7);
  ASSERT_NUM_EQUALS(buffer3[2], 8); /* this comes from buffer2 */
  ASSERT_NUM_EQUALS(buffer3[3], 9);

  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 8, buffer3, 4), 4);
  ASSERT_NUM_EQUALS(buffer3[0], 8);
  ASSERT_NUM_EQUALS(buffer3[1], 9);
  ASSERT_NUM_EQUALS(buffer3[2], 10);
  ASSERT_NUM_EQUALS(buffer3[3], 11);

  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 11, buffer3, 4), 4);
  ASSERT_NUM_EQUALS(buffer3[0], 11);
  ASSERT_NUM_EQUALS(buffer3[1], 12);
  ASSERT_NUM_EQUALS(buffer3[2], 13);
  ASSERT_NUM_EQUALS(buffer3[3], 14);

  /* only requesting 1 byte. other three remain unmodified */
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 13, buffer3, 1), 1);
  ASSERT_NUM_EQUALS(buffer3[0], 13);
  ASSERT_NUM_EQUALS(buffer3[1], 12);
  ASSERT_NUM_EQUALS(buffer3[2], 13);
  ASSERT_NUM_EQUALS(buffer3[3], 14);

  /* only two bytes are available at address 14 */
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 14, buffer3, 3), 2);
  ASSERT_NUM_EQUALS(buffer3[0], 14);
  ASSERT_NUM_EQUALS(buffer3[1], 15);
  ASSERT_NUM_EQUALS(buffer3[2], 13);
  ASSERT_NUM_EQUALS(buffer3[3], 14);

  /* no bytes are available at invalid address */
  ASSERT_NUM_EQUALS(rc_libretro_memory_read(&regions, 16, buffer3, 4), 0);
  ASSERT_NUM_EQUALS(buffer3[0], 14);
  ASSERT_NUM_EQUALS(buffer3[1], 15);
  ASSERT_NUM_EQUALS(buffer3[2], 13);
  ASSERT_NUM_EQUALS(buffer3[3], 14);
}

static void test_hash_set_add_single() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char hash[] = "ABCDEF01234567899876543210ABCDEF";

  get_mock_filereader(&file_reader);

  rc_libretro_hash_set_init(&hash_set, "file.rom", libretro_get_image_path, &file_reader);

  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file.rom"));
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash), 0);

  rc_libretro_hash_set_add(&hash_set, "file.rom", 1234, hash);

  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file.rom"), hash);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash), 1234);

  rc_libretro_hash_set_destroy(&hash_set);
}

static void test_hash_set_update_single() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char hash[] = "ABCDEF01234567899876543210ABCDEF";
  const char hash2[] = "0123456789ABCDEF0123456789ABCDEF";

  get_mock_filereader(&file_reader);

  rc_libretro_hash_set_init(&hash_set, "file.rom", libretro_get_image_path, &file_reader);

  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file.rom"));
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash), 0);

  rc_libretro_hash_set_add(&hash_set, "file.rom", 99, hash);

  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file.rom"), hash);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash), 99);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash2), 0);

  rc_libretro_hash_set_add(&hash_set, "file.rom", 1234, hash2);
  ASSERT_NUM_EQUALS(hash_set.entries_count, 1);

  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file.rom"), hash2);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash), 0);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash2), 1234);

  rc_libretro_hash_set_destroy(&hash_set);
}

static void test_hash_set_add_many() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char hash1[] = "ABCDEF01234567899876543210ABCDE1";
  const char hash2[] = "ABCDEF01234567899876543210ABCDE2";
  const char hash3[] = "ABCDEF01234567899876543210ABCDE3";
  const char hash4[] = "ABCDEF01234567899876543210ABCDE4";
  const char hash5[] = "ABCDEF01234567899876543210ABCDE5";
  const char hash6[] = "ABCDEF01234567899876543210ABCDE6";
  const char hash7[] = "ABCDEF01234567899876543210ABCDE7";
  const char hash8[] = "ABCDEF01234567899876543210ABCDE8";

  get_mock_filereader(&file_reader);

  rc_libretro_hash_set_init(&hash_set, "file.rom", libretro_get_image_path, &file_reader);

  rc_libretro_hash_set_add(&hash_set, "file1.rom", 1, hash1);
  rc_libretro_hash_set_add(&hash_set, "file2.rom", 2, hash2);
  rc_libretro_hash_set_add(&hash_set, "file3.rom", 3, hash3);
  rc_libretro_hash_set_add(&hash_set, "file4.rom", 4, hash4);
  rc_libretro_hash_set_add(&hash_set, "file5.rom", 5, hash5);
  rc_libretro_hash_set_add(&hash_set, "file6.rom", 6, hash6);
  rc_libretro_hash_set_add(&hash_set, "file7.rom", 7, hash7);
  rc_libretro_hash_set_add(&hash_set, "file8.rom", 8, hash8);

  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file1.rom"), hash1);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash1), 1);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file2.rom"), hash2);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash2), 2);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file3.rom"), hash3);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash3), 3);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file4.rom"), hash4);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash4), 4);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file5.rom"), hash5);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash5), 5);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file6.rom"), hash6);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash6), 6);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file7.rom"), hash7);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash7), 7);
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "file8.rom"), hash8);
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash8), 8);

  rc_libretro_hash_set_destroy(&hash_set);
}

static void test_hash_set_m3u_single() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char hash[] = "ABCDEF01234567899876543210ABCDEF";
  const char* m3u_contents = "file.dsk";

  get_mock_filereader(&file_reader);
  mock_file(0, "game.m3u", (uint8_t*)m3u_contents, strlen(m3u_contents));

  rc_libretro_hash_set_init(&hash_set, "game.m3u", libretro_get_image_path, &file_reader);

  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file.dsk"));
  ASSERT_NUM_EQUALS(rc_libretro_hash_set_get_game_id(&hash_set, hash), 0);
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "save1.dsk"));

  rc_libretro_hash_set_destroy(&hash_set);
}

static void test_hash_set_m3u_savedisk() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char* m3u_contents = "file.dsk\n#SAVEDISK:";

  get_mock_filereader(&file_reader);
  mock_file(0, "game.m3u", (uint8_t*)m3u_contents, strlen(m3u_contents));

  rc_libretro_hash_set_init(&hash_set, "game.m3u", libretro_get_image_path, &file_reader);

  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file.dsk"));
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "save1.dsk"), "[SAVE DISK]");

  rc_libretro_hash_set_destroy(&hash_set);
}

static void test_hash_set_m3u_savedisk_volume_label() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char* m3u_contents = "file.dsk\n#SAVEDISK:DSAVE";

  get_mock_filereader(&file_reader);
  mock_file(0, "game.m3u", (uint8_t*)m3u_contents, strlen(m3u_contents));

  rc_libretro_hash_set_init(&hash_set, "game.m3u", libretro_get_image_path, &file_reader);

  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file.dsk"));
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "save1.dsk"), "[SAVE DISK]");

  rc_libretro_hash_set_destroy(&hash_set);
}

static void test_hash_set_m3u_savedisk_multiple_with_comments_and_whitespace() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char* m3u_contents =
      "#EXTM3U\n"
      "file.dsk\n" /* index 0 */
      "\n"
      "#Save disk in the middle, because why not?\n"
      "#SAVEDISK:\n" /* index 1 */
      "    \r\n"
      "\tfile2.dsk|File 2\n" /* index 2 */
      "#SAVEDISK:DSAVE\n" /* index 3 */
      "\t\r\n"
      "#LABEL:My Custom Disk Label\n"
      "file3.dsk" /* index 4 */
      "\r\n"
      "#SAVEDISK:|No Custom Label for Save Disk"; /* index 5 */

  get_mock_filereader(&file_reader);
  mock_file(0, "game.m3u", (uint8_t*)m3u_contents, strlen(m3u_contents));

  rc_libretro_hash_set_init(&hash_set, "game.m3u", libretro_get_image_path, &file_reader);

  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file.dsk"));
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "save1.dsk"), "[SAVE DISK]");
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file2.dsk"));
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "save3.dsk"), "[SAVE DISK]");
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file3.dsk"));
  ASSERT_STR_EQUALS(rc_libretro_hash_set_get_hash(&hash_set, "save5.dsk"), "[SAVE DISK]");

  rc_libretro_hash_set_destroy(&hash_set);
}

static int libretro_get_image_path_no_core_support(uint32_t index, char* buffer, size_t buffer_size)
{
  if (index < 0 || index > 1)
    return 0;

  snprintf(buffer, buffer_size, "file%d.dsk", index);
  return 1;
}

static void test_hash_set_m3u_savedisk_no_core_support() {
  rc_libretro_hash_set_t hash_set;
  rc_hash_filereader_t file_reader;
  const char* m3u_contents = "file1.dsk\n#SAVEDISK:\nfile2.dsk";

  get_mock_filereader(&file_reader);
  mock_file(0, "game.m3u", (uint8_t*)m3u_contents, strlen(m3u_contents));

  rc_libretro_hash_set_init(&hash_set, "game.m3u", libretro_get_image_path_no_core_support, &file_reader);

  ASSERT_NUM_EQUALS(hash_set.entries_count, 0);
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file1.dsk"));
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file2.dsk"));
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "file3.dsk"));
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "save1.dsk"));
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "save2.dsk"));
  ASSERT_PTR_NULL(rc_libretro_hash_set_get_hash(&hash_set, "save3.dsk"));

  rc_libretro_hash_set_destroy(&hash_set);
}


void test_rc_libretro(void) {
  TEST_SUITE_BEGIN();

  /* rc_libretro_disallowed_settings */
  TEST_PARAMS3(test_allowed_setting,    "Beetle PSX", "beetle_psx_cpu_freq_scale", "750%");
  TEST_PARAMS3(test_allowed_setting,    "Beetle PSX", "beetle_psx_cpu_freq_scale", "100%(native)");
  TEST_PARAMS3(test_disallowed_setting, "Beetle PSX", "beetle_psx_cpu_freq_scale", "99%");
  TEST_PARAMS3(test_disallowed_setting, "Beetle PSX", "beetle_psx_cpu_freq_scale", "50%");

  TEST_PARAMS3(test_allowed_setting,    "Beetle PSX HW", "beetle_psx_hw_cpu_freq_scale", "750%");
  TEST_PARAMS3(test_allowed_setting,    "Beetle PSX HW", "beetle_psx_hw_cpu_freq_scale", "100%(native)");
  TEST_PARAMS3(test_disallowed_setting, "Beetle PSX HW", "beetle_psx_hw_cpu_freq_scale", "99%");
  TEST_PARAMS3(test_disallowed_setting, "Beetle PSX HW", "beetle_psx_hw_cpu_freq_scale", "50%");

  TEST_PARAMS3(test_allowed_setting,    "bsnes-mercury", "bsnes_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "bsnes-mercury", "bsnes_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "bsnes-mercury", "bsnes_region", "PAL");

  TEST_PARAMS3(test_allowed_setting,    "cap32", "cap32_autorun", "enabled");
  TEST_PARAMS3(test_disallowed_setting, "cap32", "cap32_autorun", "disabled");

  TEST_PARAMS3(test_allowed_setting,    "dolphin-emu", "dolphin_cheats_enabled", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "dolphin-emu", "dolphin_cheats_enabled", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "DOSBox-pure", "dosbox_pure_strict_mode", "true");
  TEST_PARAMS3(test_disallowed_setting, "DOSBox-pure", "dosbox_pure_strict_mode", "false");

  TEST_PARAMS3(test_allowed_setting,    "DuckStation", "duckstation_CDROM.LoadImagePatches", "false");
  TEST_PARAMS3(test_disallowed_setting, "DuckStation", "duckstation_CDROM.LoadImagePatches", "true");

  TEST_PARAMS3(test_allowed_setting,    "ecwolf", "ecwolf-invulnerability", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "ecwolf", "ecwolf-invulnerability", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_region", "PAL");
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_region", "pal"); /* case insensitive */
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_region", "Dendy");
  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_palette", "default"); /* setting we don't care about */
  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_game_genie", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_game_genie", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "Flycast", "reicast_sh4clock", "500");
  TEST_PARAMS3(test_allowed_setting,    "Flycast", "reicast_sh4clock", "200");
  TEST_PARAMS3(test_disallowed_setting, "Flycast", "reicast_sh4clock", "190");
  TEST_PARAMS3(test_disallowed_setting, "Flycast", "reicast_sh4clock", "50");

  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-allow-patched-romsets", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-allow-patched-romsets", "enabled");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "disabled"); /* wildcard key match */
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "enabled");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "0 - Disabled"); /* multi-not value match */
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "1 - Enabled");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cpu-speed-adjust", "25%");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cpu-speed-adjust", "50%");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cpu-speed-adjust", "75%");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cpu-speed-adjust", "90%");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cpu-speed-adjust", "95%");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cpu-speed-adjust", "99%");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cpu-speed-adjust", "100%");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cpu-speed-adjust", "150%");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cpu-speed-adjust", "200%");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cpu-speed-adjust", "400%");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-dipswitch-mslug-BIOS", "MVS Asia/Europe ver. 6 (1 slot)");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-dipswitch-mslug-BIOS", "Universe BIOS ver. 2.3 (alt)");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-neogeo-mode", "DIPSWITCH");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-neogeo-mode", "UNIBIOS");

  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_lock_on", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX", "genesis_plus_gx_lock_on", "action replay (pro)");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX", "genesis_plus_gx_lock_on", "game genie");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_lock_on", "sonic & knuckles");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_region_detect", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_region_detect", "NTSC-U");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX", "genesis_plus_gx_region_detect", "PAL");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_region_detect", "NTSC-J");

  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "action replay (pro)");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "game genie");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "sonic & knuckles");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "NTSC-U");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "PAL");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "NTSC-J");

  TEST_PARAMS3(test_allowed_setting,    "Mesen", "mesen_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Mesen", "mesen_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "Mesen", "mesen_region", "PAL");
  TEST_PARAMS3(test_disallowed_setting, "Mesen", "mesen_region", "Dendy");

  TEST_PARAMS3(test_allowed_setting,    "Mesen-S", "mesen-s_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Mesen-S", "mesen-s_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "Mesen-S", "mesen-s_region", "PAL");

  TEST_PARAMS3(test_allowed_setting,    "NeoCD", "neocd_bios", "neocd.bin (CDZ)");
  TEST_PARAMS3(test_disallowed_setting, "NeoCD", "neocd_bios", "uni-bioscd.rom (CDZ, Universe 3.3)");

  TEST_PARAMS3(test_allowed_setting,    "PPSSPP", "ppsspp_cheats", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "PPSSPP", "ppsspp_cheats", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "PCSX-ReARMed", "pcsx_rearmed_region", "PAL");
  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_psxclock", "auto");
  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_psxclock", "100");
  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_psxclock", "57");
  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_psxclock", "55");
  TEST_PARAMS3(test_disallowed_setting, "PCSX-ReARMed", "pcsx_rearmed_psxclock", "54");
  TEST_PARAMS3(test_disallowed_setting, "PCSX-ReARMed", "pcsx_rearmed_psxclock", "30");

  TEST_PARAMS3(test_allowed_setting,    "PicoDrive", "picodrive_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "PicoDrive", "picodrive_region", "US");
  TEST_PARAMS3(test_allowed_setting,    "PicoDrive", "picodrive_region", "Japan NTSC");
  TEST_PARAMS3(test_disallowed_setting, "PicoDrive", "picodrive_region", "Europe");
  TEST_PARAMS3(test_disallowed_setting, "PicoDrive", "picodrive_region", "Japan PAL");

  TEST_PARAMS3(test_allowed_setting,    "QUASI88", "q88_cpu_clock", "16");
  TEST_PARAMS3(test_allowed_setting,    "QUASI88", "q88_cpu_clock", "8");
  TEST_PARAMS3(test_allowed_setting,    "QUASI88", "q88_cpu_clock", "4");
  TEST_PARAMS3(test_disallowed_setting, "QUASI88", "q88_cpu_clock", "2");
  TEST_PARAMS3(test_disallowed_setting, "QUASI88", "q88_cpu_clock", "1");

  TEST_PARAMS3(test_allowed_setting,    "SMS Plus GX", "smsplus_region", "auto");
  TEST_PARAMS3(test_allowed_setting,    "SMS Plus GX", "smsplus_region", "ntsc-u");
  TEST_PARAMS3(test_disallowed_setting, "SMS Plus GX", "smsplus_region", "pal");
  TEST_PARAMS3(test_allowed_setting,    "SMS Plus GX", "smsplus_region", "ntsc-j");

  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "Snes9x", "snes9x_region", "PAL");
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_gfx_clip", "enabled");
  TEST_PARAMS3(test_disallowed_setting, "Snes9x", "snes9x_gfx_clip", "disabled");
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_gfx_transp", "enabled");
  TEST_PARAMS3(test_disallowed_setting, "Snes9x", "snes9x_gfx_transp", "disabled");
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_layer_1", "enabled");
  TEST_PARAMS3(test_disallowed_setting, "Snes9x", "snes9x_layer_1", "disabled");
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_layer_5", "enabled");
  TEST_PARAMS3(test_disallowed_setting, "Snes9x", "snes9x_layer_5", "disabled");

  TEST_PARAMS3(test_allowed_setting,    "SwanStation", "swanstation_CPU_Overclock", "1000");
  TEST_PARAMS3(test_allowed_setting,    "SwanStation", "swanstation_CPU_Overclock", "100");
  TEST_PARAMS3(test_disallowed_setting, "SwanStation", "swanstation_CPU_Overclock", "99");
  TEST_PARAMS3(test_disallowed_setting, "SwanStation", "swanstation_CPU_Overclock", "50");

  TEST_PARAMS3(test_allowed_setting,    "VICE x64", "vice_autostart", "enabled");
  TEST_PARAMS3(test_disallowed_setting, "VICE x64", "vice_autostart", "disabled");
  TEST_PARAMS3(test_allowed_setting,    "VICE x64", "vice_autostart", "warp");
  TEST_PARAMS3(test_allowed_setting,    "VICE x64", "vice_reset", "autostart");
  TEST_PARAMS3(test_disallowed_setting, "VICE x64", "vice_reset", "soft");
  TEST_PARAMS3(test_disallowed_setting, "VICE x64", "vice_reset", "hard");
  TEST_PARAMS3(test_disallowed_setting, "VICE x64", "vice_reset", "freeze");

  TEST_PARAMS3(test_allowed_setting,    "Virtual Jaguar", "virtualjaguar_pal", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "Virtual Jaguar", "virtualjaguar_pal", "enabled");

  /* rc_libretro_is_system_allowed */
  TEST_PARAMS2(test_allowed_system,     "FCEUmm", RC_CONSOLE_NINTENDO);

  TEST_PARAMS2(test_allowed_system,     "Mesen-S", RC_CONSOLE_SUPER_NINTENDO);
  TEST_PARAMS2(test_disallowed_system,  "Mesen-S", RC_CONSOLE_GAMEBOY);
  TEST_PARAMS2(test_disallowed_system,  "Mesen-S", RC_CONSOLE_GAMEBOY_COLOR);

  /* rc_libretro_memory_init */
  TEST(test_memory_init_without_regions);
  TEST(test_memory_init_without_regions_system_ram_only);
  TEST(test_memory_init_without_regions_save_ram_only);
  TEST(test_memory_init_without_regions_no_ram);

  TEST(test_memory_init_from_unmapped_memory);
  TEST(test_memory_init_from_unmapped_memory_null_filler);
  TEST(test_memory_init_from_unmapped_memory_no_save_ram);
  TEST(test_memory_init_from_unmapped_memory_merge_neighbors);
  TEST(test_memory_init_from_unmapped_memory_no_ram);
  TEST(test_memory_init_from_unmapped_memory_ds);
  TEST(test_memory_init_from_unmapped_memory_wii);

  TEST(test_memory_init_from_memory_map);
  TEST(test_memory_init_from_memory_map_null_filler);
  TEST(test_memory_init_from_memory_map_no_save_ram);
  TEST(test_memory_init_from_memory_map_merge_neighbors);
  TEST(test_memory_init_from_memory_map_no_ram);
  TEST(test_memory_init_from_memory_map_splice);
  TEST(test_memory_init_from_memory_map_mirrored);
  TEST(test_memory_init_from_memory_map_out_of_order);
  TEST(test_memory_init_from_memory_map_disconnect_gaps);

  /* rc_libretro_memory_read */
  TEST(test_memory_read)

  /* rc_libretro_hash_set_t */
  TEST(test_hash_set_add_single);
  TEST(test_hash_set_update_single);
  TEST(test_hash_set_add_many);

  TEST(test_hash_set_m3u_single);
  TEST(test_hash_set_m3u_savedisk);
  TEST(test_hash_set_m3u_savedisk_volume_label);
  TEST(test_hash_set_m3u_savedisk_multiple_with_comments_and_whitespace);
  TEST(test_hash_set_m3u_savedisk_no_core_support);

  TEST_SUITE_END();
}
