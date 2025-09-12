#include "rawinput.h"

#include <linux/input.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

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
    int handle; // The number * in /dev/input/event*
    int fd;

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

static struct pollfd pfds[MAX_DEVICES];

static int inotify_fd;
static struct pollfd inotify_pfd;

static int scan_devices(Device *devices);
static void handle_plug();
static void handle_evt(const struct input_event *evt, void *tag);

static int find_device_by_fd(int fd) {
    int i;
    for (i=0; i<device_count; ++i) {
        Device *d = &devices[i];

        if (d->fd == fd) {
            return i;
        }
    }

    return -1;
}

static int find_device_by_handle(int handle) {
    int i;
    for (i=0; i<device_count; ++i) {
        Device *d = &devices[i];

        if (d->handle == handle) {
            return i;
        }
    }

    return -1;
}

void raw_init() {
    device_count = scan_devices(devices);

    for (int i=0; i<device_count; ++i) {
        pfds[i].fd = devices[i].fd;
        pfds[i].events = POLLIN;
    }

    inotify_fd = inotify_init();
    inotify_add_watch(inotify_fd, "/dev/input/", IN_CREATE);

    inotify_pfd.fd = inotify_fd;
    inotify_pfd.events = POLLIN;
}

void raw_quit() {
    for (int i=0; i<device_count; ++i) {
        close(devices[i].fd);
    }
    close(inotify_fd);
    device_count = 0;
}

void raw_poll() {
    poll(&pfds[0], device_count, 0);
    for (int i=device_count-1; i>=0; --i) {

        if (pfds[i].revents & POLLIN) {
            int idx = find_device_by_fd(pfds[i].fd);
            if (idx < 0) {
//                breakpoint();
                continue;
            }


            if (!devices[idx].is_open) {
                continue;
            }


            void *tag = devices[idx].tag;

            struct input_event evts[64];
            ssize_t rb;
            rb = read(pfds[i].fd, &evts[0], sizeof(evts[0]) * 64);
            if (rb > 0) {
                size_t evt_cnt = rb / sizeof(evts[0]);
                for (int i=0; i<evt_cnt; ++i) {
                    handle_evt(&evts[i], tag);
                }
            }
        }
        else if (pfds[i].revents != 0) {
            // Device unplugged
            if (devices[i].is_open) {
                emit_unplug(devices[i].tag);
            }

            close(pfds[i].fd);
            pfds[i] = pfds[device_count-1];
            delete_device(i);

            //~ breakpoint();
        }
    }

    handle_plug();
}

static int scan_devices(Device *devices) {
    // We scan the devices by listing the contents of /dev/input
    DIR *dirp = opendir("/dev/input/");

    if (!dirp) {
        return 0;
    }

    struct dirent *d = 0;
    int i = 0;
    while ((d = readdir(dirp))) {
        if (i >= MAX_DEVICES) {
            // No more room for new devices
            break;
        }

        // Consider only character devices whose name starts with event
        if (d->d_type == DT_CHR && strncmp(d->d_name, "event", strlen("event")) == 0) {
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "/dev/input/%s", d->d_name);
            int fd = open(path, O_RDONLY);

            if (fd < 0) {
                continue;
            }

            // Get the number * in /dev/input/event*
            int handle = strtol(d->d_name + strlen("event"), NULL, 10);
            //dbg("opened path:%s fd:%d handle:%d\n", path, fd, handle);

            Device *d = &devices[i];
            d->fd = fd;
            d->handle = handle;

            d->is_open = false;
            d->tag = 0;
            ++i;
        }
    }

    closedir(dirp);
    return i;
}

static void handle_plug() {
    static char queue[MAX_DEVICES][PATH_MAX];
    static int queue_cnt = 0;

    // We use inotify to detect plugging new devices
    // (corresponds to creation of new event file in /dev/input)
    poll(&inotify_pfd, 1, 0);
    if (inotify_pfd.revents & POLLIN) {
        // Buffered read
		// https://stackoverflow.com/a/13297409
		char buffer[1024];
		ssize_t rb = read(inotify_fd, buffer, 1024);
		if (rb < 0) {
			;
		} else {
			char *end = &buffer[rb];
			char *ptr = &buffer[0];
			while (ptr < end) {
				struct inotify_event *e = (struct inotify_event*) ptr;

                if (strncmp(e->name, "event", strlen("event")) != 0) {
                    continue;
                }

                if (e->mask & IN_CREATE) {
                    // If register_device fails because of a permission error, we need to keep
                    // retrying-- my guess is that is because of the lag between the moment
                    // when the input subsystem creates the character device and the moment
                    // it sets the proper permissions.
                    //
                    // This retry loop can take a long while !!

                    // To prevent lag spikes we enqueue the device for registration and try to
                    // register devices from there only once during the poll loop iteration.
                    if (queue_cnt < MAX_DEVICES) {
                        snprintf(&queue[queue_cnt++][0], 512, "/dev/input/%s", e->name);
                    }
                }

				ptr += sizeof(*e) + e->len;
			}
		}
    } else if (inotify_pfd.revents != 0) {
        ;
    }

    // Try to get one new device from queue
    // ((( Even though I named this a queue, I actually use it like a stack )))

    if (queue_cnt > 0 && device_count < MAX_DEVICES) {
        const char *path = &queue[queue_cnt-1][0];
        int fd = open(path, O_RDONLY);

        if (fd < 0) {
            return;
        }

        // Get the number * in /dev/input/event*
        int handle = strtol(path + strlen("/dev/input/event"), NULL, 10);
        if (find_device_by_handle(handle) >= 0) {
            queue_cnt--;
            return;
        }

        int nexti = device_count;

        Device *d = &devices[nexti];
        d->fd = fd;
        d->handle = handle;

        d->is_open = false;
        d->tag = 0;

        pfds[nexti].fd = fd;
        pfds[nexti].events = POLLIN;

        device_count++;
        queue_cnt--;

        emit_plug(nexti);

        //breakpoint();
    }
}

static Raw_Key translate_linux_vk(__u16 vk) {
    //~ dbg("linux key: %u\n", vk);

    switch (vk) {
        /* Standard 3-button mouse */

    case BTN_LEFT: return RK_LMB;
    case BTN_RIGHT: return RK_RMB;
    case BTN_MIDDLE: return RK_MMB;

        /* ANSI standard keyboard - full form factor */

    case KEY_BACKSPACE: return RK_BACKSPACE;
    case KEY_TAB: return RK_TAB;
    case KEY_ENTER: return RK_ENTER;
    case KEY_PAUSE: return RK_PAUSE;
    case KEY_CAPSLOCK: return RK_CAPS_LOCK;
    case KEY_ESC: return RK_ESC;
    case KEY_SPACE: return RK_SPACE;
    case KEY_PAGEUP: return RK_PAGE_UP;
    case KEY_PAGEDOWN: return RK_PAGE_DOWN;
    case KEY_END: return RK_END;
    case KEY_HOME: return RK_HOME;
    case KEY_LEFT: return RK_LEFT;
    case KEY_UP: return RK_UP;
    case KEY_RIGHT: return RK_RIGHT;
    case KEY_DOWN: return RK_DOWN;
    case KEY_SYSRQ: return RK_PRINT_SCR;
    case KEY_INSERT: return RK_INSERT;
    case KEY_DELETE: return RK_DELETE;
    case KEY_0: return RK_0;
    case KEY_1: return RK_1;
    case KEY_2: return RK_2;
    case KEY_3: return RK_3;
    case KEY_4: return RK_4;
    case KEY_5: return RK_5;
    case KEY_6: return RK_6;
    case KEY_7: return RK_7;
    case KEY_8: return RK_8;
    case KEY_9: return RK_9;
    case KEY_A: return RK_A;
    case KEY_B: return RK_B;
    case KEY_C: return RK_C;
    case KEY_D: return RK_D;
    case KEY_E: return RK_E;
    case KEY_F: return RK_F;
    case KEY_G: return RK_G;
    case KEY_H: return RK_H;
    case KEY_I: return RK_I;
    case KEY_J: return RK_J;
    case KEY_K: return RK_K;
    case KEY_L: return RK_L;
    case KEY_M: return RK_M;
    case KEY_N: return RK_N;
    case KEY_O: return RK_O;
    case KEY_P: return RK_P;
    case KEY_Q: return RK_Q;
    case KEY_R: return RK_R;
    case KEY_S: return RK_S;
    case KEY_T: return RK_T;
    case KEY_U: return RK_U;
    case KEY_V: return RK_V;
    case KEY_W: return RK_W;
    case KEY_X: return RK_X;
    case KEY_Y: return RK_Y;
    case KEY_Z: return RK_Z;
    case KEY_KP0: return RK_NUMPAD_0;
    case KEY_KP1: return RK_NUMPAD_1;
    case KEY_KP2: return RK_NUMPAD_2;
    case KEY_KP3: return RK_NUMPAD_3;
    case KEY_KP4: return RK_NUMPAD_4;
    case KEY_KP5: return RK_NUMPAD_5;
    case KEY_KP6: return RK_NUMPAD_6;
    case KEY_KP7: return RK_NUMPAD_7;
    case KEY_KP8: return RK_NUMPAD_8;
    case KEY_KP9: return RK_NUMPAD_9;
    case KEY_KPASTERISK: return RK_NUMPAD_STAR;
    case KEY_KPPLUS: return RK_NUMPAD_PLUS;
    case KEY_KPMINUS: return RK_NUMPAD_MINUS;
    case KEY_KPSLASH: return RK_NUMPAD_SLASH;
    case KEY_KPDOT: return RK_NUMPAD_DOT;
    case KEY_NUMLOCK: return RK_NUM_LOCK;
    case KEY_F1: return RK_F1;
    case KEY_F2: return RK_F2;
    case KEY_F3: return RK_F3;
    case KEY_F4: return RK_F4;
    case KEY_F5: return RK_F5;
    case KEY_F6: return RK_F6;
    case KEY_F7: return RK_F7;
    case KEY_F8: return RK_F8;
    case KEY_F9: return RK_F9;
    case KEY_F10: return RK_F10;
    case KEY_F11: return RK_F11;
    case KEY_F12: return RK_F12;
    case KEY_SCROLLLOCK: return RK_SCROLL_LOCK;

    case KEY_LEFTSHIFT: return RK_LSHIFT;
    case KEY_RIGHTSHIFT: return RK_RSHIFT;
    case KEY_LEFTCTRL: return RK_LCTRL;
    case KEY_RIGHTCTRL: return RK_RCTRL;
    case KEY_LEFTALT: return RK_LALT;
    case KEY_RIGHTALT: return RK_RALT;

    case KEY_LEFTMETA: return RK_LWIN;
    case KEY_RIGHTMETA: return RK_RWIN;
    case KEY_COMPOSE: return RK_MENU;
    case KEY_COMMA: return RK_COMMA;
    case KEY_DOT: return RK_DOT;
    case KEY_MINUS: return RK_MINUS;
    case KEY_EQUAL: return RK_EQUALS;
    case KEY_SLASH: return RK_SLASH;
    case KEY_GRAVE: return RK_BACKTICK;
    case KEY_LEFTBRACE: return RK_LBRACKET;
    case KEY_RIGHTBRACE: return RK_RBRACKET;
    case KEY_BACKSLASH: return RK_BACKSLASH;
    case KEY_SEMICOLON: return RK_SEMICOLON;
    case KEY_APOSTROPHE: return RK_QUOTE;

    default: return RK_UNKNOWN;
    }

}

Raw_Axis translate_linux_axis(__u16 axis) {
    switch (axis) {
        case REL_X: return RA_X;
        case REL_Y: return RA_Y;
        case REL_WHEEL: return RA_WHEEL;
        default: return RA_UNKNOWN;
    }
}

static void handle_evt(const struct input_event *evt, void *tag) {
    switch (evt->type) {
        case EV_KEY:
            if (evt->value) {
                emit_key_down(tag, translate_linux_vk(evt->code));
            } else {
                emit_key_up(tag, translate_linux_vk(evt->code));
            }
            break;
        case EV_REL:
            emit_rel(tag, translate_linux_axis(evt->code), evt->value);
            break;
        default:
            return;
    }
}