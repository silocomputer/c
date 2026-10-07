#include "usage.h"

#include <stdio.h>

void usage(void) {
    fprintf(stderr,
            "c — C module driver\n"
            "\n"
            "usage:\n"
            "  c version\n"
            "  c sync\n"
            "  c test  [pkg...]\n"
            "  c build [-o name] [pkg...]\n"
            "\n"
            "build/test with no pkgs uses src/ (or .). default -o is the module name.\n");
}
