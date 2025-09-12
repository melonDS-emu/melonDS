#include "rawinput.h"
 #include <stdbool.h>
 #include <string.h>  /* memset 用 */
typedef UINT u32;
typedef SHORT i16;
typedef USHORT u16;

static bool inited = false;

static Raw_On_Key_Up   on_key_up = 0;
static Raw_On_Key_Down on_key_down = 0;
static Raw_On_Rel      on_rel = 0;
static Raw_On_Plug     on_plug = 0;
static Raw_On_Unplug   on_unplug = 0;

static void* on_key_up_user_data;
static void* on_key_down_user_data;
static void* on_rel_user_data;
static void* on_plug_user_data;
static void* on_unplug_user_data;

static void emit_key_up(void* tag, Raw_Key key) {
    if (on_key_up) {
        on_key_up(tag, key, on_key_up_user_data);
    }
}

static void emit_key_down(void* tag, Raw_Key key) {
    if (on_key_down) {
        on_key_down(tag, key, on_key_down_user_data);
    }
}

static void emit_rel(void* tag, Raw_Axis axis, int delta) {
    if (on_rel) {
        on_rel(tag, axis, delta, on_rel_user_data);
    }
}

static void emit_plug(int idx) {
    if (on_plug) {
        on_plug(idx, on_plug_user_data);
    }
}

static void emit_unplug(void* tag) {
    if (on_unplug) {
        on_unplug(tag, on_unplug_user_data);
    }
}

void raw_on_key_up(Raw_On_Key_Up _on_key_up, void* user_data) {
    on_key_up = _on_key_up;
    on_key_up_user_data = user_data;
}

void raw_on_key_down(Raw_On_Key_Down _on_key_down, void* user_data) {
    on_key_down = _on_key_down;
    on_key_down_user_data = user_data;
}

void raw_on_rel(Raw_On_Rel _on_rel, void* user_data) {
    on_rel = _on_rel;
    on_rel_user_data = user_data;
}

void raw_on_plug(Raw_On_Plug _on_plug, void* user_data) {
    on_plug = _on_plug;
    on_plug_user_data = user_data;
}

void raw_on_unplug(Raw_On_Unplug _on_unplug, void* user_data) {
    on_unplug = _on_unplug;
    on_unplug_user_data = user_data;
}

typedef
struct Device {
    RAWINPUTDEVICELIST ridl;
    /*
        NOTE: I'm not gonna bother with this, the info I get in RID_DEVICE_INFO
        was flat out wrong for most (all?) of my devices I tested it with...
    */
    /* RID_DEVICE_INFO info; */

    bool is_open;
    void* tag;
} Device;

#define MAX_DEVICES 32
static Device devices[MAX_DEVICES];
static int device_count;

int raw_dev_cnt() {
    return device_count;
}

static void delete_device(int idx) {
    /* Swap & pop (quick unstable remove) */
    devices[idx] = devices[device_count-1];
    device_count--;
}

static int find_device_by_tag(void *tag) {
    int i;
    for (i=0; i<device_count; ++i) {
        Device *d = &devices[i];

        if (d->is_open && d->tag == tag) {
            return i;
        }
    }

    return -1;
}

void raw_open(int idx, void* tag) {
    Device *d;

    if (find_device_by_tag(tag) >= 0) {
        /* Tag already used */
        return;
    }

    if (idx < 0 || idx >= device_count) {
        /* Bad device index */
        return;
    }

    d = &devices[idx];
    if (d->is_open) {
        /* Device already open */
        return;
    }

    d->is_open = true;
    d->tag = tag;
}

void raw_close(void* tag) {
    int idx = find_device_by_tag(tag);
    if (idx >= 0) {
        devices[idx].is_open = false;
    }
}

static HWND msg_window = NULL;

static int find_device_by_handle(HANDLE h) {
    int i;
    for (i=0; i<device_count; ++i) {
        Device *d = &devices[i];

        if (d->ridl.hDevice == h) {
            return i;
        }
    }

    return -1;
}

static int scan_devices(Device *devices) {
    /*
        NOTE: GetRawInputDeviceList will report some "extra" devices. I.e. when I have just a
        keyboard and mouse plugged in, it reports 2 mice and 2 keyboards for some reason...
        I guess it is detecting some other device as a keyboard; in any case
        these extra devices can be safely ignored.
    */

    RAWINPUTDEVICELIST ridls[MAX_DEVICES];
    u32 n;
    int i;

    if (GetRawInputDeviceList(NULL, &n, sizeof(ridls[0])) == (UINT)-1) { return 0; }
    n = n>MAX_DEVICES? MAX_DEVICES: n;
    GetRawInputDeviceList(ridls, &n, sizeof(ridls[0]));

    for (i = 0; i < (int)n; ++i) {
        Device* d = &devices[i];

        d->ridl = ridls[i];
        d->is_open = false;
        d->tag = 0;
    }

    return n;
}

static Raw_Key translate_windows_vk(u16 vk) {

    switch (vk) {
        /* Standard 3-button mouse */

    case VK_LBUTTON: return RK_LMB;
    case VK_RBUTTON: return RK_RMB;
    case VK_MBUTTON: return RK_MMB;

        /* ANSI standard keyboard - full form factor */

    case VK_BACK: return RK_BACKSPACE;
    case VK_TAB: return RK_TAB;
    case VK_RETURN: return RK_ENTER;
    case VK_PAUSE: return RK_PAUSE;
    case VK_CAPITAL: return RK_CAPS_LOCK;
    case VK_ESCAPE: return RK_ESC;
    case VK_SPACE: return RK_SPACE;
    case VK_PRIOR: return RK_PAGE_UP;
    case VK_NEXT: return RK_PAGE_DOWN;
    case VK_END: return RK_END;
    case VK_HOME: return RK_HOME;
    case VK_LEFT: return RK_LEFT;
    case VK_UP: return RK_UP;
    case VK_RIGHT: return RK_RIGHT;
    case VK_DOWN: return RK_DOWN;
    case VK_SNAPSHOT: return RK_PRINT_SCR;
    case VK_INSERT: return RK_INSERT;
    case VK_DELETE: return RK_DELETE;
    case '0': return RK_0;
    case '1': return RK_1;
    case '2': return RK_2;
    case '3': return RK_3;
    case '4': return RK_4;
    case '5': return RK_5;
    case '6': return RK_6;
    case '7': return RK_7;
    case '8': return RK_8;
    case '9': return RK_9;
    case 'A': return RK_A;
    case 'B': return RK_B;
    case 'C': return RK_C;
    case 'D': return RK_D;
    case 'E': return RK_E;
    case 'F': return RK_F;
    case 'G': return RK_G;
    case 'H': return RK_H;
    case 'I': return RK_I;
    case 'J': return RK_J;
    case 'K': return RK_K;
    case 'L': return RK_L;
    case 'M': return RK_M;
    case 'N': return RK_N;
    case 'O': return RK_O;
    case 'P': return RK_P;
    case 'Q': return RK_Q;
    case 'R': return RK_R;
    case 'S': return RK_S;
    case 'T': return RK_T;
    case 'U': return RK_U;
    case 'V': return RK_V;
    case 'W': return RK_W;
    case 'X': return RK_X;
    case 'Y': return RK_Y;
    case 'Z': return RK_Z;
    case VK_NUMPAD0: return RK_NUMPAD_0;
    case VK_NUMPAD1: return RK_NUMPAD_1;
    case VK_NUMPAD2: return RK_NUMPAD_2;
    case VK_NUMPAD3: return RK_NUMPAD_3;
    case VK_NUMPAD4: return RK_NUMPAD_4;
    case VK_NUMPAD5: return RK_NUMPAD_5;
    case VK_NUMPAD6: return RK_NUMPAD_6;
    case VK_NUMPAD7: return RK_NUMPAD_7;
    case VK_NUMPAD8: return RK_NUMPAD_8;
    case VK_NUMPAD9: return RK_NUMPAD_9;
    case VK_MULTIPLY: return RK_NUMPAD_STAR;
    case VK_ADD: return RK_NUMPAD_PLUS;
    case VK_SUBTRACT: return RK_NUMPAD_MINUS;
    case VK_DIVIDE: return RK_NUMPAD_SLASH;
    case VK_DECIMAL: return RK_NUMPAD_DOT;
    case VK_NUMLOCK: return RK_NUM_LOCK;
    case VK_F1: return RK_F1;
    case VK_F2: return RK_F2;
    case VK_F3: return RK_F3;
    case VK_F4: return RK_F4;
    case VK_F5: return RK_F5;
    case VK_F6: return RK_F6;
    case VK_F7: return RK_F7;
    case VK_F8: return RK_F8;
    case VK_F9: return RK_F9;
    case VK_F10: return RK_F10;
    case VK_F11: return RK_F11;
    case VK_F12: return RK_F12;
    case VK_SCROLL: return RK_SCROLL_LOCK;

    case VK_LSHIFT: return RK_LSHIFT;
    case VK_RSHIFT: return RK_RSHIFT;
    case VK_LCONTROL: return RK_LCTRL;
    case VK_RCONTROL: return RK_RCTRL;
    case VK_LMENU: return RK_LALT;
    case VK_RMENU: return RK_RALT;

    /* Looks like windows can't tell the difference between
    left/right ctrl/shift/alt on (some?) keyboards.*/
    case VK_SHIFT: return RK_LSHIFT;
    case VK_CONTROL: return RK_LCTRL;
    case VK_MENU: return RK_LALT;

    case VK_LWIN: return RK_LWIN;
    case VK_RWIN: return RK_RWIN;
    case VK_APPS: return RK_MENU;
    case VK_OEM_COMMA: return RK_COMMA;
    case VK_OEM_PERIOD: return RK_DOT;
    case VK_OEM_MINUS: return RK_MINUS;
    case VK_OEM_PLUS: return RK_EQUALS;
    case VK_OEM_2: return RK_SLASH;
    case VK_OEM_3: return RK_BACKTICK;
    case VK_OEM_4: return RK_LBRACKET;
    case VK_OEM_6: return RK_RBRACKET;
    case VK_OEM_5: return RK_BACKSLASH;
    case VK_OEM_1: return RK_SEMICOLON;
    case VK_OEM_7: return RK_QUOTE;

    default: return RK_UNKNOWN;
    }

}

void raw_init() {
    HINSTANCE instance = GetModuleHandleW(NULL);
    HWND hwnd = 0;

    if (inited) {
        return;
    }
    inited = true;

    device_count = scan_devices(devices);

    /* Register a window class (wide) */
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = L"dummy_class";
    RegisterClassW(&wc);

    /* Message-only window (wide) */
    msg_window = CreateWindowExW(0, L"dummy_class", L"dummy",
                                 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, instance, NULL);
    if (!msg_window) {
        return;
    }
    hwnd = msg_window;

    /* Register RAWINPUT targets: Keyboard(0x06) and Mouse(0x02) on GenericDesktop(0x01) */
    RAWINPUTDEVICE rids[2];
    DWORD flags = RIDEV_INPUTSINK; /* メッセージ専用ウィンドウで受け取るため必須 */
    /* 必要に応じてレガシー抑止やデバイス通知を追加:
       flags |= RIDEV_NOLEGACY;     // WM_KEYDOWN/WM_MOUSEMOVE等を抑止（用途に応じて）
       flags |= RIDEV_DEVNOTIFY;    // WM_INPUT_DEVICE_CHANGEを受け取る
    */

    rids[0].usUsagePage = 0x01;      /* Generic Desktop */
    rids[0].usUsage     = 0x06;      /* Keyboard */
    rids[0].dwFlags     = flags;
    rids[0].hwndTarget  = hwnd;

    rids[1].usUsagePage = 0x01;      /* Generic Desktop */
    rids[1].usUsage     = 0x02;      /* Mouse */
    rids[1].dwFlags     = flags;
    rids[1].hwndTarget  = hwnd;

    if (!RegisterRawInputDevices(rids, (UINT)(sizeof(rids)/sizeof(rids[0])), (UINT)sizeof(rids[0]))) {
        /* 失敗時はエラーコードをログに残すことを推奨 */
        /* DWORD err = GetLastError(); (必要なら記録) */
        return;
    }

}

void raw_quit() {
    if(!inited) {
        return;
    }

    device_count = 0;

    if (msg_window) {
        DestroyWindow(msg_window);
        msg_window = NULL;
    }
}

static void handle_msg(const MSG *msg) {
    if (msg->message != WM_INPUT) {
        /* DEVNOTIFYを使うならここでWM_INPUT_DEVICE_CHANGEも処理可 */
        return;
    }

    HRAWINPUT raw_input_handle = (HRAWINPUT)msg->lParam;
    RAWINPUT input;
    u32 size = sizeof(RAWINPUT);
    if ((u32)-1 == GetRawInputData(raw_input_handle, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER))) {
        return;
    }

    int idx = find_device_by_handle(input.header.hDevice);
    if (idx < 0) {
        /*
            Error: If we are receiving messages about
            this device, we must have it in the devices
            array.
        */
        return;
    }
    if (!devices[idx].is_open) {
        return; /* Not open -> Not emitting event */
    }

    void *tag = devices[idx].tag;

    switch (input.header.dwType) {
    case RIM_TYPEKEYBOARD:
        /*
            TODO: I should switch to using MakeCode here
            instead of VKey, since the latter seems to be
            affected by keyboard layout and other "software"
            things.
        */
        if (input.data.keyboard.Flags & RI_KEY_BREAK) {
            emit_key_up(tag, translate_windows_vk(input.data.keyboard.VKey));
        }
        else {
            emit_key_down(tag, translate_windows_vk(input.data.keyboard.VKey));
        }
        break;
    case RIM_TYPEMOUSE:
        if (input.data.mouse.usFlags & MOUSE_MOVE_RELATIVE) {
            if (input.data.mouse.lLastX) {
                emit_rel(tag, RA_X, input.data.mouse.lLastX);
            }
            if (input.data.mouse.lLastY) {
                emit_rel(tag, RA_Y, input.data.mouse.lLastY);
            }
        }
        if (input.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
            emit_key_down(tag, RK_LMB);
        }
        if (input.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
            emit_key_up(tag, RK_LMB);
        }
        if (input.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
            emit_key_down(tag, RK_RMB);
        }
        if (input.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
            emit_key_up(tag, RK_RMB);
        }
        /* XBUTTON1/2 */
        if (input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) emit_key_down(tag, RK_XBUTTON1);
        if (input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)   emit_key_up  (tag, RK_XBUTTON1);
        if (input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) emit_key_down(tag, RK_XBUTTON2);
        if (input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)   emit_key_up  (tag, RK_XBUTTON2);


        if (input.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
            emit_key_down(tag, RK_MMB);
        }
        if (input.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
            emit_key_up(tag, RK_MMB);
        }
        if (input.data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
            emit_rel(tag, RA_WHEEL, (i16)(input.data.mouse.usButtonData) / WHEEL_DELTA);
        }

        break;
    default:
        break;
    }
}

static void handle_plug() {
    /*
        I am aware of RIDEV_DEVNOTIFY, but that says on MSDN that
        it doesn't support XP so we do it like this:
        Rescan devices and compare with old device array to detect
        when a device is (un)plugged.
    */

    int rescan_count;
    Device rescan[MAX_DEVICES];
    int n = device_count;
    int i, j;

    rescan_count = scan_devices(rescan);

    /* Iterate in reverse here because we'll be deleting */
    for (i = n - 1; i >= 0; --i) {
        Device* dold = &devices[i];
        int is_unplug = 1;
        for (j=0; j<rescan_count; ++j) {
            if(rescan[j].ridl.hDevice == dold->ridl.hDevice) {
                is_unplug = 0;
                break;
            }
        }
        if (is_unplug) {
            emit_unplug(dold->tag);
            delete_device(i);
        }
    }

    for (i=0; i<rescan_count; ++i) {
        Device *dnew = &rescan[i];
        int idx = find_device_by_handle(dnew->ridl.hDevice);
        if (idx < 0) {
            int newi = device_count;
            if (newi < MAX_DEVICES) {
                /* New device plugged in */
                devices[newi] = *dnew;
                device_count++;
                emit_plug(newi);
            } else {
                /* No more room for new devices */
                ;
            }
        }
    }
}

void raw_poll() {
    MSG msg;

    if (!inited)
        return;

    /* DEVNOTIFYも使うなら下のように範囲を広げる */
    while (PeekMessage(&msg, msg_window, WM_INPUT, WM_INPUT_DEVICE_CHANGE, PM_REMOVE)) {
        handle_msg(&msg);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    handle_plug();
}
