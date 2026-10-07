#include <c/c.h>

#include "build.h"
#include "sync.h"
#include "test.h"
#include "usage.h"

#include <stdio.h>
#include <string.h>

static int cmd_version(void) {
    printf("c %d.%d.%d\n", C_VERSION_MAJOR, C_VERSION_MINOR, C_VERSION_PATCH);
    return C_EXIT_OK;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return C_EXIT_USAGE;
    }
    if (strcmp(argv[1], "version") == 0) {
        return cmd_version();
    }
    if (strcmp(argv[1], "build") == 0) {
        return cmd_build(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "sync") == 0) {
        return cmd_sync(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "test") == 0) {
        return cmd_test(argc - 1, argv + 1);
    }
    usage();
    return C_EXIT_USAGE;
}
