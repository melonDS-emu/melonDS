#include <glib.h>
#include <stdlib.h>

#define MIN_NUMBER_OF_RUNS 1
#define EXIT_TEST_SKIP 77

extern int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);

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
