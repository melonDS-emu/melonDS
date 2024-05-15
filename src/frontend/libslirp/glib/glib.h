#ifndef GLIB_SHIM_H
#define GLIB_SHIM_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>

#ifdef _WIN32
#define G_OS_WIN32 1
#else
#define G_OS_UNIX 1
#endif

#define G_LITTLE_ENDIAN 0
#define G_BIG_ENDIAN 1
#define G_BYTE_ORDER G_LITTLE_ENDIAN

#define GUINT16_FROM_BE(n) ntohs(n)
#define GUINT16_TO_BE(n) htons(n)
#define GUINT32_FROM_BE(n) ntohl(n)
#define GUINT32_TO_BE(n) htonl(n)

#define GINT16_TO_BE(n) (int16_t) htons(n)
#define GINT16_FROM_BE(n) (int16_t) ntohs(n)
#define GINT32_TO_BE(n) (int32_t) htonl(n)
#define GINT32_FROM_BE(n) (int32_t) ntohl(n)

#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))

#define G_GNUC_PRINTF(x, y)

#define GLIB_CHECK_VERSION(x, y, z) 1
#define G_STATIC_ASSERT(...)
#define g_assert assert
#define G_UNLIKELY(x) __builtin_expect(x, 0)

#define g_return_if_fail(expr) \
    do {                       \
        if (!(expr))           \
            return;            \
    } while (false)

#define g_return_val_if_fail(expr, val) \
    do { \
        if (!(expr)) \
            return (val); \
    } while (false)

#define g_warn_if_reached() \
    do { \
        g_warning("g_warn_if_reached: Reached " __FILE__ ":%d", __LINE__); \
    } while (false)


#define g_warn_if_fail(expr) \
    do { \
        if (!(expr)) \
            g_warning("g_warn_if_fail: Expression '" #expr "' failed at " __FILE__ ":%d", __LINE__); \
    } while (false)

#define g_assert_not_reached() \
    do { \
        assert(false && "g_assert_not_reached"); \
        __builtin_unreachable(); \
    } while (false)

#define GLIB_SIZEOF_VOID_P 8
#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif
#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#ifndef TRUE
#define TRUE true
#endif
#ifndef FALSE
#define FALSE false
#endif

typedef bool gboolean;
typedef char gchar;
typedef int gint;
typedef size_t gsize;
typedef void* gpointer;

#define g_debug printf
#define g_warning printf
#define g_error printf
#define g_critical printf

#define g_new(type, count) (type*) malloc(sizeof(type) * count)
#define g_new0(type, count) (type*) calloc(count, sizeof(type))

#define g_malloc malloc
#define g_malloc0(size) calloc(1, size)
#define g_realloc realloc
#define g_free free

#define g_getenv(var) getenv(var)

typedef struct GString {
    gchar* str;
    gsize len;
    gsize allocated_len;
} GString;

typedef gchar** GStrv;

GString* g_string_new(gchar* initial);
gchar* g_string_free(GString* string, gboolean free_segment);
void g_string_append_printf(GString* gstr, const gchar* format, ...);
gchar* g_strstr_len(const gchar* haystack, int len, const gchar* needle);
gchar* g_strdup(const gchar* str);
#ifdef _MSC_VER
#define g_ascii_strcasecmp(a, b) stricmp(a, b)
#else
#define g_ascii_strcasecmp(a, b) strcasecmp(a, b)
#endif

#define g_str_has_prefix(str, pfx) (strncmp(str, pfx, strlen(pfx)) == 0)
#define g_snprintf snprintf
#define g_vsnprintf vsnprintf

gint g_strv_length(GStrv strings);
void g_strfreev(GStrv strings);

typedef struct GRand {} GRand;
gint g_rand_int_range(GRand* grand, gint min, gint max);
GRand* g_rand_new();
void g_rand_free(GRand* rand);

typedef struct GError {
    const gchar* message;
} GError;

void g_error_free(GError* error);
#define g_strerror(err) strerror(err)

typedef void (*GSpawnChildSetupFunc)(gpointer ptr);
typedef enum GSpawnFlags {
    G_SPAWN_SEARCH_PATH
} GSpawnFlags;

typedef gint GPid;

gboolean g_shell_parse_argv(const gchar* command_line, gint* argcp, gchar*** argvp, GError** error);

gboolean g_spawn_async_with_fds(const gchar *working_directory, gchar **argv,
                                gchar **envp, GSpawnFlags flags,
                                GSpawnChildSetupFunc child_setup,
                                gpointer user_data, GPid *child_pid, gint stdin_fd,
                                gint stdout_fd, gint stderr_fd, GError **error);

typedef struct { gchar* key; int value; } GDebugKey;
#define g_parse_debug_string(str, keys, nkeys) 0


#endif
