#ifdef _WIN32
/* as defined in sdkddkver.h */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 /* Vista */
#endif
#include <ws2tcpip.h>
#endif

#include <glib.h>
#include <stdlib.h>
#include "../src/libslirp.h"
#include "helper.h"

#define MIN_NUMBER_OF_RUNS 1
#define EXIT_TEST_SKIP 77

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char **argv)
{
    int i, j;

    for (i = 1; i < argc; i++) {
        GError *err = NULL;
        char *name = argv[i];
        char *buf;
        size_t size;

        if (!g_file_get_contents(name, &buf, &size, &err)) {
            g_warning("Failed to read '%s': %s", name, err->message);
            g_clear_error(&err);
            return EXIT_FAILURE;
        }

        g_print("%s...\n", name);
        for (j = 0; j < MIN_NUMBER_OF_RUNS; j++) {
            if (LLVMFuzzerTestOneInput((void *)buf, size) == EXIT_TEST_SKIP) {
                return EXIT_TEST_SKIP;
            }
        }
        g_free(buf);
    }

    return EXIT_SUCCESS;
}
