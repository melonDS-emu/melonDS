#include <cstdio>
#include "../test_generator.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        return -1;
    }

    if (!Teakra::Test::GenerateTestCasesToFile(argv[1])) {
        std::fprintf(stderr, "Unable to successfully generate all tests.\n");
        return -2;
    }

    return 0;
}
