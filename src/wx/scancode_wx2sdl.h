/*
    Copyright 2016-2017 StapleButter

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/


// conversion from wxWdgets-provided platform-specific keycodes to SDL scancodes
// much of this is taken from the SDL source code


#ifdef __WXMSW__
static const SDL_Scancode windows_scancode_table[] =
{
	/*	0						1							2							3							4						5							6							7 */
	/*	8						9							A							B							C						D							E							F */
	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_ESCAPE,		SDL_SCANCODE_1,				SDL_SCANCODE_2,				SDL_SCANCODE_3,			SDL_SCANCODE_4,				SDL_SCANCODE_5,				SDL_SCANCODE_6,			/* 0 */
	SDL_SCANCODE_7,				SDL_SCANCODE_8,				SDL_SCANCODE_9,				SDL_SCANCODE_0,				SDL_SCANCODE_MINUS,		SDL_SCANCODE_EQUALS,		SDL_SCANCODE_BACKSPACE,		SDL_SCANCODE_TAB,		/* 0 */

	SDL_SCANCODE_Q,				SDL_SCANCODE_W,				SDL_SCANCODE_E,				SDL_SCANCODE_R,				SDL_SCANCODE_T,			SDL_SCANCODE_Y,				SDL_SCANCODE_U,				SDL_SCANCODE_I,			/* 1 */
	SDL_SCANCODE_O,				SDL_SCANCODE_P,				SDL_SCANCODE_LEFTBRACKET,	SDL_SCANCODE_RIGHTBRACKET,	SDL_SCANCODE_RETURN,	SDL_SCANCODE_LCTRL,			SDL_SCANCODE_A,				SDL_SCANCODE_S,			/* 1 */

	SDL_SCANCODE_D,				SDL_SCANCODE_F,				SDL_SCANCODE_G,				SDL_SCANCODE_H,				SDL_SCANCODE_J,			SDL_SCANCODE_K,				SDL_SCANCODE_L,				SDL_SCANCODE_SEMICOLON,	/* 2 */
	SDL_SCANCODE_APOSTROPHE,	SDL_SCANCODE_GRAVE,			SDL_SCANCODE_LSHIFT,		SDL_SCANCODE_BACKSLASH,		SDL_SCANCODE_Z,			SDL_SCANCODE_X,				SDL_SCANCODE_C,				SDL_SCANCODE_V,			/* 2 */

	SDL_SCANCODE_B,				SDL_SCANCODE_N,				SDL_SCANCODE_M,				SDL_SCANCODE_COMMA,			SDL_SCANCODE_PERIOD,	SDL_SCANCODE_SLASH,			SDL_SCANCODE_RSHIFT,		SDL_SCANCODE_PRINTSCREEN,/* 3 */
	SDL_SCANCODE_LALT,			SDL_SCANCODE_SPACE,			SDL_SCANCODE_CAPSLOCK,		SDL_SCANCODE_F1,			SDL_SCANCODE_F2,		SDL_SCANCODE_F3,			SDL_SCANCODE_F4,			SDL_SCANCODE_F5,		/* 3 */

	SDL_SCANCODE_F6,			SDL_SCANCODE_F7,			SDL_SCANCODE_F8,			SDL_SCANCODE_F9,			SDL_SCANCODE_F10,		SDL_SCANCODE_NUMLOCKCLEAR,	SDL_SCANCODE_SCROLLLOCK,	SDL_SCANCODE_HOME,		/* 4 */
	SDL_SCANCODE_UP,			SDL_SCANCODE_PAGEUP,		SDL_SCANCODE_KP_MINUS,		SDL_SCANCODE_LEFT,			SDL_SCANCODE_KP_5,		SDL_SCANCODE_RIGHT,			SDL_SCANCODE_KP_PLUS,		SDL_SCANCODE_END,		/* 4 */

	SDL_SCANCODE_DOWN,			SDL_SCANCODE_PAGEDOWN,		SDL_SCANCODE_INSERT,		SDL_SCANCODE_DELETE,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_NONUSBACKSLASH,SDL_SCANCODE_F11,		/* 5 */
	SDL_SCANCODE_F12,			SDL_SCANCODE_PAUSE,			SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_LGUI,			SDL_SCANCODE_RGUI,		SDL_SCANCODE_APPLICATION,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 5 */

	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_F13,		SDL_SCANCODE_F14,			SDL_SCANCODE_F15,			SDL_SCANCODE_F16,		/* 6 */
	SDL_SCANCODE_F17,			SDL_SCANCODE_F18,			SDL_SCANCODE_F19,			SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 6 */

	SDL_SCANCODE_INTERNATIONAL2,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL1,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN,	/* 7 */
	SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL4,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_INTERNATIONAL5,		SDL_SCANCODE_UNKNOWN,	SDL_SCANCODE_INTERNATIONAL3,		SDL_SCANCODE_UNKNOWN,		SDL_SCANCODE_UNKNOWN	/* 7 */
};
#endif
#ifdef __WXGTK__
#include <gdk/gdkkeysyms.h>
static const struct {
    int keysym;
    SDL_Scancode scancode;
} KeySymToSDLScancode[] = {
    { GDK_Return, SDL_SCANCODE_RETURN },
    { GDK_Escape, SDL_SCANCODE_ESCAPE },
    { GDK_BackSpace, SDL_SCANCODE_BACKSPACE },
    { GDK_Tab, SDL_SCANCODE_TAB },
    { GDK_Caps_Lock, SDL_SCANCODE_CAPSLOCK },
    { GDK_F1, SDL_SCANCODE_F1 },
    { GDK_F2, SDL_SCANCODE_F2 },
    { GDK_F3, SDL_SCANCODE_F3 },
    { GDK_F4, SDL_SCANCODE_F4 },
    { GDK_F5, SDL_SCANCODE_F5 },
    { GDK_F6, SDL_SCANCODE_F6 },
    { GDK_F7, SDL_SCANCODE_F7 },
    { GDK_F8, SDL_SCANCODE_F8 },
    { GDK_F9, SDL_SCANCODE_F9 },
    { GDK_F10, SDL_SCANCODE_F10 },
    { GDK_F11, SDL_SCANCODE_F11 },
    { GDK_F12, SDL_SCANCODE_F12 },
    { GDK_Print, SDL_SCANCODE_PRINTSCREEN },
    { GDK_Scroll_Lock, SDL_SCANCODE_SCROLLLOCK },
    { GDK_Pause, SDL_SCANCODE_PAUSE },
    { GDK_Insert, SDL_SCANCODE_INSERT },
    { GDK_Home, SDL_SCANCODE_HOME },
    { GDK_Prior, SDL_SCANCODE_PAGEUP },
    { GDK_Delete, SDL_SCANCODE_DELETE },
    { GDK_End, SDL_SCANCODE_END },
    { GDK_Next, SDL_SCANCODE_PAGEDOWN },
    { GDK_Right, SDL_SCANCODE_RIGHT },
    { GDK_Left, SDL_SCANCODE_LEFT },
    { GDK_Down, SDL_SCANCODE_DOWN },
    { GDK_Up, SDL_SCANCODE_UP },
    { GDK_Num_Lock, SDL_SCANCODE_NUMLOCKCLEAR },
    { GDK_KP_Divide, SDL_SCANCODE_KP_DIVIDE },
    { GDK_KP_Multiply, SDL_SCANCODE_KP_MULTIPLY },
    { GDK_KP_Subtract, SDL_SCANCODE_KP_MINUS },
    { GDK_KP_Add, SDL_SCANCODE_KP_PLUS },
    { GDK_KP_Enter, SDL_SCANCODE_KP_ENTER },
    { GDK_KP_Delete, SDL_SCANCODE_KP_PERIOD },
    { GDK_KP_End, SDL_SCANCODE_KP_1 },
    { GDK_KP_Down, SDL_SCANCODE_KP_2 },
    { GDK_KP_Next, SDL_SCANCODE_KP_3 },
    { GDK_KP_Left, SDL_SCANCODE_KP_4 },
    { GDK_KP_Begin, SDL_SCANCODE_KP_5 },
    { GDK_KP_Right, SDL_SCANCODE_KP_6 },
    { GDK_KP_Home, SDL_SCANCODE_KP_7 },
    { GDK_KP_Up, SDL_SCANCODE_KP_8 },
    { GDK_KP_Prior, SDL_SCANCODE_KP_9 },
    { GDK_KP_Insert, SDL_SCANCODE_KP_0 },
    { GDK_KP_Decimal, SDL_SCANCODE_KP_PERIOD },
    { GDK_KP_1, SDL_SCANCODE_KP_1 },
    { GDK_KP_2, SDL_SCANCODE_KP_2 },
    { GDK_KP_3, SDL_SCANCODE_KP_3 },
    { GDK_KP_4, SDL_SCANCODE_KP_4 },
    { GDK_KP_5, SDL_SCANCODE_KP_5 },
    { GDK_KP_6, SDL_SCANCODE_KP_6 },
    { GDK_KP_7, SDL_SCANCODE_KP_7 },
    { GDK_KP_8, SDL_SCANCODE_KP_8 },
    { GDK_KP_9, SDL_SCANCODE_KP_9 },
    { GDK_KP_0, SDL_SCANCODE_KP_0 },
    { GDK_KP_Decimal, SDL_SCANCODE_KP_PERIOD },
    { GDK_Hyper_R, SDL_SCANCODE_APPLICATION },
    { GDK_KP_Equal, SDL_SCANCODE_KP_EQUALS },
    { GDK_F13, SDL_SCANCODE_F13 },
    { GDK_F14, SDL_SCANCODE_F14 },
    { GDK_F15, SDL_SCANCODE_F15 },
    { GDK_F16, SDL_SCANCODE_F16 },
    { GDK_F17, SDL_SCANCODE_F17 },
    { GDK_F18, SDL_SCANCODE_F18 },
    { GDK_F19, SDL_SCANCODE_F19 },
    { GDK_F20, SDL_SCANCODE_F20 },
    { GDK_F21, SDL_SCANCODE_F21 },
    { GDK_F22, SDL_SCANCODE_F22 },
    { GDK_F23, SDL_SCANCODE_F23 },
    { GDK_F24, SDL_SCANCODE_F24 },
    { GDK_Execute, SDL_SCANCODE_EXECUTE },
    { GDK_Help, SDL_SCANCODE_HELP },
    { GDK_Menu, SDL_SCANCODE_MENU },
    { GDK_Select, SDL_SCANCODE_SELECT },
    { GDK_Cancel, SDL_SCANCODE_STOP },
    { GDK_Redo, SDL_SCANCODE_AGAIN },
    { GDK_Undo, SDL_SCANCODE_UNDO },
    { GDK_Find, SDL_SCANCODE_FIND },
    { GDK_KP_Separator, SDL_SCANCODE_KP_COMMA },
    { GDK_Sys_Req, SDL_SCANCODE_SYSREQ },
    { GDK_Control_L, SDL_SCANCODE_LCTRL },
    { GDK_Shift_L, SDL_SCANCODE_LSHIFT },
    { GDK_Alt_L, SDL_SCANCODE_LALT },
    { GDK_Meta_L, SDL_SCANCODE_LGUI },
    { GDK_Super_L, SDL_SCANCODE_LGUI },
    { GDK_Control_R, SDL_SCANCODE_RCTRL },
    { GDK_Shift_R, SDL_SCANCODE_RSHIFT },
    { GDK_Alt_R, SDL_SCANCODE_RALT },
    { GDK_ISO_Level3_Shift, SDL_SCANCODE_RALT },
    { GDK_Meta_R, SDL_SCANCODE_RGUI },
    { GDK_Super_R, SDL_SCANCODE_RGUI },
    { GDK_Mode_switch, SDL_SCANCODE_MODE },
    { GDK_period, SDL_SCANCODE_PERIOD },
    { GDK_comma, SDL_SCANCODE_COMMA },
    { GDK_slash, SDL_SCANCODE_SLASH },
    { GDK_backslash, SDL_SCANCODE_BACKSLASH },
    { GDK_minus, SDL_SCANCODE_MINUS },
    { GDK_equal, SDL_SCANCODE_EQUALS },
    { GDK_space, SDL_SCANCODE_SPACE },
    { GDK_grave, SDL_SCANCODE_GRAVE },
    { GDK_apostrophe, SDL_SCANCODE_APOSTROPHE },
    { GDK_bracketleft, SDL_SCANCODE_LEFTBRACKET },
    { GDK_bracketright, SDL_SCANCODE_RIGHTBRACKET },
};
#endif


SDL_Scancode scancode_wx2sdl(wxKeyEvent& event)
{
#ifdef __WXMSW__

    u32 wParam = event.GetRawKeyCode();
    u32 lParam = event.GetRawKeyFlags();

    SDL_Scancode code;
    char bIsExtended;
    int nScanCode = (lParam >> 16) & 0xFF;

    /* 0x45 here to work around both pause and numlock sharing the same scancode, so use the VK key to tell them apart */
    if (nScanCode == 0 || nScanCode == 0x45)
    {
        switch(wParam)
        {
        case VK_CLEAR: return SDL_SCANCODE_CLEAR;
        case VK_MODECHANGE: return SDL_SCANCODE_MODE;
        case VK_SELECT: return SDL_SCANCODE_SELECT;
        case VK_EXECUTE: return SDL_SCANCODE_EXECUTE;
        case VK_HELP: return SDL_SCANCODE_HELP;
        case VK_PAUSE: return SDL_SCANCODE_PAUSE;
        case VK_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;

        case VK_F13: return SDL_SCANCODE_F13;
        case VK_F14: return SDL_SCANCODE_F14;
        case VK_F15: return SDL_SCANCODE_F15;
        case VK_F16: return SDL_SCANCODE_F16;
        case VK_F17: return SDL_SCANCODE_F17;
        case VK_F18: return SDL_SCANCODE_F18;
        case VK_F19: return SDL_SCANCODE_F19;
        case VK_F20: return SDL_SCANCODE_F20;
        case VK_F21: return SDL_SCANCODE_F21;
        case VK_F22: return SDL_SCANCODE_F22;
        case VK_F23: return SDL_SCANCODE_F23;
        case VK_F24: return SDL_SCANCODE_F24;

        case VK_OEM_NEC_EQUAL: return SDL_SCANCODE_KP_EQUALS;
        case VK_BROWSER_BACK: return SDL_SCANCODE_AC_BACK;
        case VK_BROWSER_FORWARD: return SDL_SCANCODE_AC_FORWARD;
        case VK_BROWSER_REFRESH: return SDL_SCANCODE_AC_REFRESH;
        case VK_BROWSER_STOP: return SDL_SCANCODE_AC_STOP;
        case VK_BROWSER_SEARCH: return SDL_SCANCODE_AC_SEARCH;
        case VK_BROWSER_FAVORITES: return SDL_SCANCODE_AC_BOOKMARKS;
        case VK_BROWSER_HOME: return SDL_SCANCODE_AC_HOME;
        case VK_VOLUME_MUTE: return SDL_SCANCODE_AUDIOMUTE;
        case VK_VOLUME_DOWN: return SDL_SCANCODE_VOLUMEDOWN;
        case VK_VOLUME_UP: return SDL_SCANCODE_VOLUMEUP;

        case VK_MEDIA_NEXT_TRACK: return SDL_SCANCODE_AUDIONEXT;
        case VK_MEDIA_PREV_TRACK: return SDL_SCANCODE_AUDIOPREV;
        case VK_MEDIA_STOP: return SDL_SCANCODE_AUDIOSTOP;
        case VK_MEDIA_PLAY_PAUSE: return SDL_SCANCODE_AUDIOPLAY;
        case VK_LAUNCH_MAIL: return SDL_SCANCODE_MAIL;
        case VK_LAUNCH_MEDIA_SELECT: return SDL_SCANCODE_MEDIASELECT;

        case VK_OEM_102: return SDL_SCANCODE_NONUSBACKSLASH;

        case VK_ATTN: return SDL_SCANCODE_SYSREQ;
        case VK_CRSEL: return SDL_SCANCODE_CRSEL;
        case VK_EXSEL: return SDL_SCANCODE_EXSEL;
        case VK_OEM_CLEAR: return SDL_SCANCODE_CLEAR;

        case VK_LAUNCH_APP1: return SDL_SCANCODE_APP1;
        case VK_LAUNCH_APP2: return SDL_SCANCODE_APP2;

        default: return SDL_SCANCODE_UNKNOWN;
        }
    }

    if (nScanCode > 127)
        return SDL_SCANCODE_UNKNOWN;

    code = windows_scancode_table[nScanCode];

    bIsExtended = (lParam & (1 << 24)) != 0;
    if (!bIsExtended)
    {
        switch (code)
        {
        case SDL_SCANCODE_HOME:
            return SDL_SCANCODE_KP_7;
        case SDL_SCANCODE_UP:
            return SDL_SCANCODE_KP_8;
        case SDL_SCANCODE_PAGEUP:
            return SDL_SCANCODE_KP_9;
        case SDL_SCANCODE_LEFT:
            return SDL_SCANCODE_KP_4;
        case SDL_SCANCODE_RIGHT:
            return SDL_SCANCODE_KP_6;
        case SDL_SCANCODE_END:
            return SDL_SCANCODE_KP_1;
        case SDL_SCANCODE_DOWN:
            return SDL_SCANCODE_KP_2;
        case SDL_SCANCODE_PAGEDOWN:
            return SDL_SCANCODE_KP_3;
        case SDL_SCANCODE_INSERT:
            return SDL_SCANCODE_KP_0;
        case SDL_SCANCODE_DELETE:
            return SDL_SCANCODE_KP_PERIOD;
        case SDL_SCANCODE_PRINTSCREEN:
            return SDL_SCANCODE_KP_MULTIPLY;
        default:
            break;
        }
    }
    else
    {
        switch (code)
        {
        case SDL_SCANCODE_RETURN:
            return SDL_SCANCODE_KP_ENTER;
        case SDL_SCANCODE_LALT:
            return SDL_SCANCODE_RALT;
        case SDL_SCANCODE_LCTRL:
            return SDL_SCANCODE_RCTRL;
        case SDL_SCANCODE_SLASH:
            return SDL_SCANCODE_KP_DIVIDE;
        case SDL_SCANCODE_CAPSLOCK:
            return SDL_SCANCODE_KP_PLUS;
        default:
            break;
        }
    }

    return code;

#endif
#ifdef __WXGTK__

    int keysym = event.GetRawKeyCode();

    if (keysym == 0)
    {
        return SDL_SCANCODE_UNKNOWN;
    }

    if (keysym >= GDK_a && keysym <= GDK_z)
    {
        return (SDL_Scancode)(SDL_SCANCODE_A + (keysym - GDK_a));
    }
    if (keysym >= GDK_A && keysym <= GDK_Z)
    {
        return (SDL_Scancode)(SDL_SCANCODE_A + (keysym - GDK_A));
    }

    if (keysym == GDK_0)
    {
        return SDL_SCANCODE_0;
    }
    if (keysym >= GDK_1 && keysym <= GDK_9)
    {
        return (SDL_Scancode)(SDL_SCANCODE_1 + (keysym - GDK_1));
    }

    for (int i = 0; i < SDL_arraysize(KeySymToSDLScancode); ++i)
    {
        if (keysym == KeySymToSDLScancode[i].keysym)
        {
            return KeySymToSDLScancode[i].scancode;
        }
    }
    return SDL_SCANCODE_UNKNOWN;

#endif
    return SDL_SCANCODE_UNKNOWN;
}
