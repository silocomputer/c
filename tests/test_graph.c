#include "graph.h"
#include "mod.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_file(const char *path, const char *body) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        return -1;
    }
    fputs(body, f);
    fclose(f);
    return 0;
}

static void cleanup(void) {
    remove("deps/low/src/low.c");
    remove("deps/low/c.mod");
    remove("deps/high/c.mod");
    rmdir("deps/low/src");
    rmdir("deps/low");
    rmdir("deps/high");
}

int main(void) {
    cleanup();
    mkdir("deps", 0755);
    mkdir("deps/low", 0755);
    mkdir("deps/low/src", 0755);
    mkdir("deps/high", 0755);
    (void)errno;

    if (write_file("deps/low/c.mod", "module low\nlevel  0\nfreestanding 0\n") != 0 ||
        write_file("deps/low/src/low.c", "int low_x;\n") != 0 ||
        write_file("deps/high/c.mod", "module high\nlevel  2\nfreestanding 0\n") != 0) {
        fprintf(stderr, "setup failed\n");
        cleanup();
        return 1;
    }

    Mod root = {0};
    snprintf(root.name, sizeof(root.name), "root");
    root.level = 1;
    root.freestanding = 0;
    snprintf(root.require[0], MOD_MAX_NAME, "low");
    root.nrequire = 1;

    Graph g;
    if (graph_resolve(&g, &root) != 0) {
        fprintf(stderr, "expected low require to succeed\n");
        cleanup();
        return 1;
    }
    if (g.nmods != 1 || g.nsources < 1) {
        fprintf(stderr, "unexpected graph size mods=%d sources=%d\n", g.nmods, g.nsources);
        cleanup();
        return 1;
    }

    snprintf(root.require[0], MOD_MAX_NAME, "high");
    if (graph_resolve(&g, &root) == 0) {
        fprintf(stderr, "expected upward require to fail\n");
        cleanup();
        return 1;
    }

    cleanup();
    return 0;
}
