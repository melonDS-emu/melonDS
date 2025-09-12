#include "rawinput.h"

const char* raw_axis_str(Raw_Axis axis) {
    switch (axis) {
    case RA_X: return "X";
    case RA_Y: return "Y";
    case RA_WHEEL: return "WHEEL";
    default: return "UNKNOWN";
    }
}

const char* raw_key_str(Raw_Key key) {

    switch (key) {
    case RK_LMB: return "LMB";
    case RK_RMB: return "RMB";
    case RK_MMB: return "MMB";
    case RK_BACKSPACE: return "BACKSPACE";
    case RK_TAB: return "TAB";
    case RK_ENTER: return "ENTER";
    case RK_PAUSE: return "PAUSE";
    case RK_CAPS_LOCK: return "CAPS_LOCK";
    case RK_ESC: return "ESC";
    case RK_SPACE: return "SPACE";
    case RK_PAGE_UP: return "PAGE_UP";
    case RK_PAGE_DOWN: return "PAGE_DOWN";
    case RK_END: return "END";
    case RK_HOME: return "HOME";
    case RK_LEFT: return "LEFT";
    case RK_UP: return "UP";
    case RK_RIGHT: return "RIGHT";
    case RK_DOWN: return "DOWN";
    case RK_PRINT_SCR: return "PRINT_SCR";
    case RK_INSERT: return "INSERT";
    case RK_DELETE: return "DELETE";
    case RK_0: return "0";
    case RK_1: return "1";
    case RK_2: return "2";
    case RK_3: return "3";
    case RK_4: return "4";
    case RK_5: return "5";
    case RK_6: return "6";
    case RK_7: return "7";
    case RK_8: return "8";
    case RK_9: return "9";
    case RK_A: return "A";
    case RK_B: return "B";
    case RK_C: return "C";
    case RK_D: return "D";
    case RK_E: return "E";
    case RK_F: return "F";
    case RK_G: return "G";
    case RK_H: return "H";
    case RK_I: return "I";
    case RK_J: return "J";
    case RK_K: return "K";
    case RK_L: return "L";
    case RK_M: return "M";
    case RK_N: return "N";
    case RK_O: return "O";
    case RK_P: return "P";
    case RK_Q: return "Q";
    case RK_R: return "R";
    case RK_S: return "S";
    case RK_T: return "T";
    case RK_U: return "U";
    case RK_V: return "V";
    case RK_W: return "W";
    case RK_X: return "X";
    case RK_Y: return "Y";
    case RK_Z: return "Z";
    case RK_NUMPAD_0: return "NUMPAD_0";
    case RK_NUMPAD_1: return "NUMPAD_1";
    case RK_NUMPAD_2: return "NUMPAD_2";
    case RK_NUMPAD_3: return "NUMPAD_3";
    case RK_NUMPAD_4: return "NUMPAD_4";
    case RK_NUMPAD_5: return "NUMPAD_5";
    case RK_NUMPAD_6: return "NUMPAD_6";
    case RK_NUMPAD_7: return "NUMPAD_7";
    case RK_NUMPAD_8: return "NUMPAD_8";
    case RK_NUMPAD_9: return "NUMPAD_9";
    case RK_NUMPAD_STAR: return "NUMPAD_STAR";
    case RK_NUMPAD_PLUS: return "NUMPAD_PLUS";
    case RK_NUMPAD_MINUS: return "NUMPAD_MINUS";
    case RK_NUMPAD_SLASH: return "NUMPAD_SLASH";
    case RK_NUMPAD_DOT: return "NUMPAD_DOT";
    case RK_NUM_LOCK: return "NUM_LOCK";
    case RK_F1: return "F1";
    case RK_F2: return "F2";
    case RK_F3: return "F3";
    case RK_F4: return "F4";
    case RK_F5: return "F5";
    case RK_F6: return "F6";
    case RK_F7: return "F7";
    case RK_F8: return "F8";
    case RK_F9: return "F9";
    case RK_F10: return "F10";
    case RK_F11: return "F11";
    case RK_F12: return "F12";
    case RK_SCROLL_LOCK: return "SCROLL_LOCK";
    case RK_LSHIFT: return "LSHIFT";
    case RK_RSHIFT: return "RSHIFT";
    case RK_LCTRL: return "LCTRL";
    case RK_RCTRL: return "RCTRL";
    case RK_LALT: return "LALT";
    case RK_RALT: return "RALT";
    case RK_LWIN: return "LWIN";
    case RK_RWIN: return "RWIN";
    case RK_MENU: return "MENU";
    case RK_COMMA: return "COMMA";
    case RK_DOT: return "DOT";
    case RK_MINUS: return "MINUS";
    case RK_EQUALS: return "EQUALS";
    case RK_SLASH: return "SLASH";
    case RK_BACKTICK: return "BACKTICK";
    case RK_LBRACKET: return "LBRACKET";
    case RK_RBRACKET: return "RBRACKET";
    case RK_BACKSLASH: return "BACKSLASH";
    case RK_SEMICOLON: return "SEMICOLON";
    case RK_QUOTE: return "QUOTE";
    default: return "UNKNOWN";
    }

}