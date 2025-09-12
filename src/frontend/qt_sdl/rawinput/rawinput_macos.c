// macOS stub (C”Å): RawInput–¢ŽÀ‘•‚Å‚àƒŠƒ“ƒN‚ð’Ê‚·‚¾‚¯
// RawInput–¢ŽÀ‘•
#include "rawinput.h"
#include <stdbool.h>

static bool s_inited = false;

int  raw_dev_cnt(void) { return 0; }
void raw_open(int /*idx*/, void* /*tag*/) {}
void raw_close(void* /*tag*/) {}

void raw_init(void) { s_inited = true; }
void raw_quit(void) { s_inited = false; }
void raw_poll(void) {}

void raw_on_key_up(Raw_On_Key_Up  /*cb*/, void* /*ud*/) {}
void raw_on_key_down(Raw_On_Key_Down/*cb*/, void* /*ud*/) {}
void raw_on_rel(Raw_On_Rel     /*cb*/, void* /*ud*/) {}
void raw_on_plug(Raw_On_Plug    /*cb*/, void* /*ud*/) {}
void raw_on_unplug(Raw_On_Unplug  /*cb*/, void* /*ud*/) {}
