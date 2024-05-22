//
// Created by nhp on 14-05-24.
//

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#include "glib.h"

GString* g_string_new(gchar* initial) {
    GString* str = g_new0(GString, 1);

    if (initial != NULL) {
        int len = strlen(initial);
        gchar* p = malloc(len);
        memcpy(p, initial, len);
        str->str = p;
        str->len = len;
        str->allocated_len = len;
    } else {
        gchar* p = malloc(64);
        str->str = p;
        str->len = 0;
        str->allocated_len = 64;
    }
    return str;
}

gchar* g_string_free(GString* str, gboolean free_segment) {
    char* seg = str->str;
    free(str);
    if (free_segment) {
        free(str->str);
        return NULL;
    }
    return seg;
}

void g_string_append_printf(GString* str, const gchar* format, ...) {
    va_list args;
    va_start(args, format);
    int need_len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (str->len + need_len + 1 < str->allocated_len) {
        gsize new_len = str->len + need_len + 1;
        gchar* newp = realloc(str->str, new_len);
        str->str = newp;
        str->allocated_len = new_len;
        str->len = new_len - 1;
    }

    gchar* temp = malloc(need_len + 1);
    va_start(args, format);
    vsnprintf(temp, need_len, format, args);
    va_end(args);

    strcat(str->str, temp);
    free(temp);
}

gchar* g_strstr_len(const gchar* haystack, int len, const gchar* needle) {
    if (len == -1) return strstr(haystack, needle);
    size_t needle_len = strlen(needle);
    for (int i = 0; i < len; i++) {
        size_t found = 0;
        for (int j = i; j < len; j++) {
            if (haystack[j] == needle[j - i]) found++;
            else break;
            if (found == needle_len) return (gchar*) haystack + j;
        }
    }
    return NULL;
}

gchar* g_strdup(const gchar* str) {
    if (str == NULL) return NULL;
    else return strdup(str);
}

int g_strv_length(GStrv strings) {
    gint count = 0;
    while (strings[count])
        count++;
    return count;
}

void g_strfreev(GStrv strings) {
    for (int i = 0; strings[i] != NULL; i++) {
        free(strings[i]);
    }
}

// This is not good but all we're using slirp for is beaming pokemans over the internet so it's probably okay
gint g_rand_int_range(GRand* grand, gint min, gint max) {
    double r = (double) rand();
    double range = (double) (max - min);
    double r2 = (r / (double) RAND_MAX) * range;
    return MIN(max, ((int) r2) + min);
}

GRand* g_rand_new() {
    return malloc(sizeof(GRand));
}

void g_rand_free(GRand* rand) {
    free(rand);
}

void g_error_free(GError* error) {
    free(error);
}

gboolean g_shell_parse_argv(const gchar* command_line, gint* argcp, gchar*** argvp, GError** error) {
    const gchar* message = "Unimplemented.";
    GError* err = malloc(sizeof(GError));
    err->message = message;
    *error = err;
    return false;
}

gboolean g_spawn_async_with_fds(const gchar *working_directory, gchar **argv,
                                gchar **envp, GSpawnFlags flags,
                                GSpawnChildSetupFunc child_setup,
                                gpointer user_data, GPid *child_pid, gint stdin_fd,
                                gint stdout_fd, gint stderr_fd, GError **error)
{
    return false;
}
