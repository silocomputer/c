#include "build.h"

#include <c/c.h>

#include "fs.h"
#include "graph.h"
#include "mod.h"
#include "pkg.h"
#include "run.h"

#include <stdio.h>
#include <string.h>

enum { BUILD_MAX_ARGV = 64 + PKG_MAX_SOURCES };

static const char *build_strip_dot_slash(const char *p) {
    if (p[0] == '.' && p[1] == '/') {
        return p + 2;
    }
    return p;
}

static int build_append_sources(char *const *dirs, int ndirs, char out[][PKG_MAX_PATH], int max,
                                int *nout) {
    for (int i = 0; i < ndirs; i++) {
        char chunk[PKG_MAX_SOURCES][PKG_MAX_PATH];
        int nchunk = 0;
        if (pkg_list_c(dirs[i], chunk, PKG_MAX_SOURCES, &nchunk) != 0) {
            return -1;
        }
        if (nchunk == 0) {
            fprintf(stderr, "c: build: no .c files in %s\n", dirs[i]);
            return -1;
        }
        for (int j = 0; j < nchunk; j++) {
            if (*nout >= max) {
                fprintf(stderr, "c: build: too many sources\n");
                return -1;
            }
            memcpy(out[*nout], chunk[j], PKG_MAX_PATH);
            (*nout)++;
        }
    }
    return 0;
}

int cmd_build(int argc, char **argv) {
    const char *out_name = NULL;
    char *dirs[PKG_MAX_DIRS];
    int ndirs = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "c: build: -o needs an argument\n");
                return C_EXIT_USAGE;
            }
            out_name = argv[++i];
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "c: build: unknown flag %s\n", argv[i]);
            return C_EXIT_USAGE;
        }
        if (ndirs >= PKG_MAX_DIRS) {
            fprintf(stderr, "c: build: too many packages\n");
            return C_EXIT_FAIL;
        }
        dirs[ndirs++] = (char *)build_strip_dot_slash(argv[i]);
    }

    Mod m;
    if (mod_load(&m, "c.mod") != 0) {
        return C_EXIT_FAIL;
    }

    Graph g;
    if (graph_resolve(&g, &m) != 0) {
        return C_EXIT_FAIL;
    }

    char defpkg[PKG_MAX_PATH];
    if (ndirs == 0) {
        if (pkg_default(defpkg, sizeof(defpkg)) != 0) {
            return C_EXIT_FAIL;
        }
        dirs[ndirs++] = defpkg;
    }

    if (out_name == NULL) {
        out_name = m.name;
    }

    char sources[PKG_MAX_SOURCES][PKG_MAX_PATH];
    int nsources = 0;
    if (build_append_sources(dirs, ndirs, sources, PKG_MAX_SOURCES, &nsources) != 0) {
        return C_EXIT_FAIL;
    }
    for (int i = 0; i < g.nsources; i++) {
        if (nsources >= PKG_MAX_SOURCES) {
            fprintf(stderr, "c: build: too many sources\n");
            return C_EXIT_FAIL;
        }
        memcpy(sources[nsources], g.sources[i], PKG_MAX_PATH);
        nsources++;
    }

    if (fs_mkdir_parent(out_name) != 0) {
        return C_EXIT_FAIL;
    }

    char *av[BUILD_MAX_ARGV];
    int n = 0;
    av[n++] = "clang";
    av[n++] = "-std=c23";
    av[n++] = "-O2";
    av[n++] = "-Wall";
    av[n++] = "-Wextra";
    av[n++] = "-Iinclude";
    for (int i = 0; i < g.ninclude; i++) {
        av[n++] = "-I";
        av[n++] = g.include[i];
    }
    if (m.freestanding) {
        av[n++] = "-ffreestanding";
        av[n++] = "-nostdlib";
    }
    av[n++] = "-o";
    av[n++] = (char *)out_name;
    for (int i = 0; i < nsources; i++) {
        av[n++] = sources[i];
    }
    av[n] = NULL;

    int rc = run_cmd(av);
    if (rc != 0) {
        return rc < 0 ? C_EXIT_FAIL : rc;
    }
    printf("c: built %s\n", out_name);
    return C_EXIT_OK;
}
