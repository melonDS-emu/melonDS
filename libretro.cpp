#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <stdio.h>
#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif
#include "NDS.h"
#include "GPU.h"
#include "libretro.h"

static uint16_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static bool use_audio_cb;
static float last_aspect;
static float last_sample_rate;
char retro_base_directory[4096];
char retro_game_path[4096];

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}


static retro_environment_t environ_cb;

void retro_init(void)
{
   frame_buf = (uint16_t*)malloc(256 * 384 * sizeof(uint16_t));
   const char *dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
   {
      snprintf(retro_base_directory, sizeof(retro_base_directory), "%s", dir);
   }
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "melonDS";
   info->library_version  = "0.1";
   info->need_fullpath    = true;
   info->valid_extensions = "nds";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   float aspect                = 0.0f;
   float sampling_rate         = 30000.0f;

   info->geometry.base_width   = 256;
   info->geometry.base_height  = 386;
   info->geometry.max_width    = 256;
   info->geometry.max_height   = 384;
   info->geometry.aspect_ratio = aspect;

   last_aspect                 = aspect;
   last_sample_rate            = sampling_rate;
}

static struct retro_rumble_interface rumble;

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   static const struct retro_controller_description controllers[] = {
      { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) },
   };

   static const struct retro_controller_info ports[] = {
      { controllers, 1 },
      { NULL, 0 },
   };

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static unsigned x_coord;
static unsigned y_coord;
static unsigned phase;
static int mouse_rel_x;
static int mouse_rel_y;

void retro_reset(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void update_input(void)
{
   int dir_x = 0;
   int dir_y = 0;

   input_poll_cb();
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
      dir_y--;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
      dir_y++;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
      dir_x--;
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      dir_x++;

   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN))
      log_cb(RETRO_LOG_INFO, "Return key is pressed!\n");

   if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_x))
      log_cb(RETRO_LOG_INFO, "x key is pressed!\n");

   int16_t mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   int16_t mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
   bool mouse_l    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
   bool mouse_r    = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   bool mouse_down = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
   bool mouse_up   = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
   bool mouse_middle = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
   if (mouse_x)
      log_cb(RETRO_LOG_INFO, "Mouse X: %d\n", mouse_x);
   if (mouse_y)
      log_cb(RETRO_LOG_INFO, "Mouse Y: %d\n", mouse_y);
   if (mouse_l)
      log_cb(RETRO_LOG_INFO, "Mouse L pressed.\n");
   if (mouse_r)
      log_cb(RETRO_LOG_INFO, "Mouse R pressed.\n");
   if (mouse_down)
      log_cb(RETRO_LOG_INFO, "Mouse wheeldown pressed.\n");
   if (mouse_up)
      log_cb(RETRO_LOG_INFO, "Mouse wheelup pressed.\n");
   if (mouse_middle)
      log_cb(RETRO_LOG_INFO, "Mouse middle pressed.\n");

   mouse_rel_x += mouse_x;
   mouse_rel_y += mouse_y;
   if (mouse_rel_x >= 310)
      mouse_rel_x = 309;
   else if (mouse_rel_x < 10)
      mouse_rel_x = 10;
   if (mouse_rel_y >= 230)
      mouse_rel_y = 229;
   else if (mouse_rel_y < 10)
      mouse_rel_y = 10;

   bool pointer_pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
   int16_t pointer_x = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
   int16_t pointer_y = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
   if (pointer_pressed)
      log_cb(RETRO_LOG_INFO, "Pointer: (%6d, %6d).\n", pointer_x, pointer_y);

   dir_x += input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 5000;
   dir_y += input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 5000;
   dir_x += input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 5000;
   dir_y += input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 5000;

   x_coord = (x_coord + dir_x) & 31;
   y_coord = (y_coord + dir_y) & 31;

   if (rumble.set_rumble_state)
   {
      static bool old_start;
      static bool old_select;
      uint16_t strength_strong = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) ? 0x4000 : 0xffff;
      uint16_t strength_weak = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) ? 0x4000 : 0xffff;
      bool start = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
      bool select = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
      if (old_start != start)
         log_cb(RETRO_LOG_INFO, "Strong rumble: %s.\n", start ? "ON": "OFF");
      rumble.set_rumble_state(0, RETRO_RUMBLE_STRONG, start * strength_strong);

      if (old_select != select)
         log_cb(RETRO_LOG_INFO, "Weak rumble: %s.\n", select ? "ON": "OFF");
      rumble.set_rumble_state(0, RETRO_RUMBLE_WEAK, select * strength_weak);

      old_start = start;
      old_select = select;
   }
}

static void check_variables(void)
{

}

static void audio_callback(void)
{
   for (unsigned i = 0; i < 30000 / 60; i++, phase++)
   {
      int16_t val = 0x800 * sinf(2.0f * M_PI * phase * 300.0f / 30000.0f);
      audio_cb(val, val);
   }

   phase %= 100;
}

static void audio_set_state(bool enable)
{
   (void)enable;
}

void retro_run(void)
{
   update_input();
   NDS::RunFrame();
   memcpy(frame_buf, GPU::Framebuffer, sizeof(GPU::Framebuffer));
   for (int i = 0; i < 256*192*2; i++)
   {
       frame_buf[i]  = ((frame_buf[i] & 0x1F) << 11) | 
                       ((frame_buf[i] & 0x3E0) << 1) | 
                       ((frame_buf[i] & 0x7C00) >> 10);
   }
   video_cb(frame_buf, 256, 384, 0);
   if (!use_audio_cb)
      audio_callback();

   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();
}

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   snprintf(retro_game_path, sizeof(retro_game_path), "%s", info->path);
   NDS::Init();

   struct retro_audio_callback audio_cb = { audio_callback, audio_set_state };
   use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);

   check_variables();

   (void)info;
   return true;
}

void retro_unload_game(void)
{

}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   return false;
}

size_t retro_serialize_size(void)
{
   return false;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

