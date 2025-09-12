#ifndef RAWINPUT_H
#define RAWINPUT_H

#ifdef _WIN32
#include <windows.h>
#endif

typedef enum Raw_Axis {
    RA_UNKNOWN,

    RA_X,
    RA_Y,
    RA_WHEEL,

    RA_COUNT = 64 /* Enough space for axes added in future */
} Raw_Axis;

typedef enum Raw_Key {

    RK_UNKNOWN,

    /* Standard 3-button mouse */

    RK_LMB,
    RK_RMB,
    RK_MMB,

    /* ANSI standard keyboard - full form factor */

    RK_BACKSPACE,
    RK_TAB,
    RK_ENTER,
    RK_PAUSE,
    RK_CAPS_LOCK,
    RK_ESC,
    RK_SPACE,
    RK_PAGE_UP,
    RK_PAGE_DOWN,
    RK_END,
    RK_HOME,
    RK_LEFT,
    RK_UP,
    RK_RIGHT,
    RK_DOWN,
    RK_PRINT_SCR,
    RK_INSERT,
    RK_DELETE,
    RK_0,
    RK_1,
    RK_2,
    RK_3,
    RK_4,
    RK_5,
    RK_6,
    RK_7,
    RK_8,
    RK_9,
    RK_A,
    RK_B,
    RK_C,
    RK_D,
    RK_E,
    RK_F,
    RK_G,
    RK_H,
    RK_I,
    RK_J,
    RK_K,
    RK_L,
    RK_M,
    RK_N,
    RK_O,
    RK_P,
    RK_Q,
    RK_R,
    RK_S,
    RK_T,
    RK_U,
    RK_V,
    RK_W,
    RK_X,
    RK_Y,
    RK_Z,
    RK_NUMPAD_0,
    RK_NUMPAD_1,
    RK_NUMPAD_2,
    RK_NUMPAD_3,
    RK_NUMPAD_4,
    RK_NUMPAD_5,
    RK_NUMPAD_6,
    RK_NUMPAD_7,
    RK_NUMPAD_8,
    RK_NUMPAD_9,
    RK_NUMPAD_STAR,
    RK_NUMPAD_PLUS,
    RK_NUMPAD_MINUS,
    RK_NUMPAD_SLASH,
    RK_NUMPAD_DOT,
    RK_NUM_LOCK,
    RK_F1,
    RK_F2,
    RK_F3,
    RK_F4,
    RK_F5,
    RK_F6,
    RK_F7,
    RK_F8,
    RK_F9,
    RK_F10,
    RK_F11,
    RK_F12,
    RK_SCROLL_LOCK,
    RK_LSHIFT,
    RK_RSHIFT,
    RK_LCTRL,
    RK_RCTRL,
    RK_LALT,
    RK_RALT,
    RK_LWIN,
    RK_RWIN,
    RK_MENU,
    RK_COMMA,
    RK_DOT,
    RK_MINUS,
    RK_EQUALS,
    RK_SLASH,
    RK_BACKTICK,
    RK_LBRACKET,
    RK_RBRACKET,
    RK_BACKSLASH,
    RK_SEMICOLON,
    RK_QUOTE,

    RK_COUNT = 512  /* Enough space for virtual keys added in future */

} Raw_Key;

typedef void (*Raw_On_Key_Up)(void* tag, Raw_Key key, void *user_data);
typedef void (*Raw_On_Key_Down)(void* tag, Raw_Key key, void *user_data);
typedef void (*Raw_On_Rel)(void* tag, Raw_Axis axis, int delta, void *user_data);
typedef void (*Raw_On_Unplug)(void* tag, void *user_data);
typedef void (*Raw_On_Plug)(int idx, void *user_data);

void raw_init();
void raw_quit();

int  raw_dev_cnt();
void raw_open(int idx, void* tag);
void raw_close(void* tag);

void raw_poll();

const char* raw_key_str(Raw_Key key);
const char* raw_axis_str(Raw_Axis axis);

void raw_on_key_up(Raw_On_Key_Up on_key_up, void* user_data);
void raw_on_key_down(Raw_On_Key_Down on_key_down, void* user_data);
void raw_on_rel(Raw_On_Rel on_rel, void* user_data);
void raw_on_plug(Raw_On_Plug on_plug, void* user_data);
void raw_on_unplug(Raw_On_Unplug on_unplug, void* user_data);

#endif