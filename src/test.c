#include "test.h"

#include <c/c.h>

#include "fs.h"
#include "graph.h"
#include "mod.h"
#include "pkg.h"
#include "run.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

enum { TEST_MAX_ARGV = 64 + PKG_MAX_SOURCES, TEST_MAX_FILES = 64 };

static int test_is_test_c(const char *name) {
    size_t n = strlen(name);
    if (n < 8) {
        return 0;
    }
    if (strncmp(name, "test_", 5) != 0) {
        return 0;
    }
    return n > 2 && strcmp(name + n - 2, ".c") == 0;
}

static const char *test_basename(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

static int test_collect_pkg(char *const *dirs, int ndirs, char out[][PKG_MAX_PATH], int max,
                            int *nout) {
    *nout = 0;
    for (int i = 0; i < ndirs; i++) {
        char chunk[PKG_MAX_SOURCES][PKG_MAX_PATH];
        int nchunk = 0;
        if (pkg_list_c(dirs[i], chunk, PKG_MAX_SOURCES, &nchunk) != 0) {
            return -1;
        }
        for (int j = 0; j < nchunk; j++) {
            if (strcmp(test_basename(chunk[j]), "main.c") == 0) {
                continue;
            }
            if (*nout >= max) {
                fprintf(stderr, "c: test: too many sources\n");
                return -1;
            }
            memcpy(out[*nout], chunk[j], PKG_MAX_PATH);
            (*nout)++;
        }
    }
    return 0;
}

static int test_list_tests(char out[][PKG_MAX_PATH], int max, int *nout) {
    *nout = 0;
    DIR *d = opendir("tests");
    if (d == NULL) {
        return 0;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!test_is_test_c(ent->d_name)) {
            continue;
        }
        if (*nout >= max) {
            fprintf(stderr, "c: test: too many tests\n");
            closedir(d);
            return -1;
        }
        if (snprintf(out[*nout], PKG_MAX_PATH, "tests/%s", ent->d_name) >= PKG_MAX_PATH) {
            closedir(d);
            return -1;
        }
        (*nout)++;
    }
    closedir(d);
    return 0;
}

static int test_stem(const char *path, char *out, size_t outsz) {
    const char *base = test_basename(path);
    size_t n = strlen(base);
    if (n < 3 || strcmp(base + n - 2, ".c") != 0) {
        return -1;
    }
    if (n - 2 >= outsz) {
        return -1;
    }
    memcpy(out, base, n - 2);
    out[n - 2] = '\0';
    return 0;
}

int cmd_test(int argc, char **argv) {
    char *dirs[PKG_MAX_DIRS];
    int ndirs = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            fprintf(stderr, "c: test: unknown flag %s\n", argv[i]);
            return C_EXIT_USAGE;
        }
        if (ndirs >= PKG_MAX_DIRS) {
            fprintf(stderr, "c: test: too many packages\n");
            return C_EXIT_FAIL;
        }
        const char *p = argv[i];
        if (p[0] == '.' && p[1] == '/') {
            p += 2;
        }
        dirs[ndirs++] = (char *)p;
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

    char tests[TEST_MAX_FILES][PKG_MAX_PATH];
    int ntests = 0;
    if (test_list_tests(tests, TEST_MAX_FILES, &ntests) != 0) {
        return C_EXIT_FAIL;
    }
    if (ntests == 0) {
        printf("c: test: no tests/test_*.c\n");
        return C_EXIT_OK;
    }

    char sources[PKG_MAX_SOURCES][PKG_MAX_PATH];
    int nsources = 0;
    if (test_collect_pkg(dirs, ndirs, sources, PKG_MAX_SOURCES, &nsources) != 0) {
        return C_EXIT_FAIL;
    }
    for (int i = 0; i < g.nsources; i++) {
        if (nsources >= PKG_MAX_SOURCES) {
            fprintf(stderr, "c: test: too many sources\n");
            return C_EXIT_FAIL;
        }
        memcpy(sources[nsources], g.sources[i], PKG_MAX_PATH);
        nsources++;
    }

    if (fs_mkdir_p(".c/test") != 0) {
        return C_EXIT_FAIL;
    }

    int failed = 0;
    for (int t = 0; t < ntests; t++) {
        char stem[PKG_MAX_PATH];
        if (test_stem(tests[t], stem, sizeof(stem)) != 0) {
            return C_EXIT_FAIL;
        }
        char outbin[PKG_MAX_PATH];
        if (snprintf(outbin, sizeof(outbin), ".c/test/%s", stem) >= (int)sizeof(outbin)) {
            return C_EXIT_FAIL;
        }

        char *av[TEST_MAX_ARGV];
        int n = 0;
        av[n++] = "clang";
        av[n++] = "-std=c23";
        av[n++] = "-O2";
        av[n++] = "-Wall";
        av[n++] = "-Wextra";
        av[n++] = "-Iinclude";
        av[n++] = "-Isrc"; /* private headers for this tool's own tests */
        for (int i = 0; i < g.ninclude; i++) {
            av[n++] = "-I";
            av[n++] = g.include[i];
        }
        av[n++] = "-o";
        av[n++] = outbin;
        for (int i = 0; i < nsources; i++) {
            av[n++] = sources[i];
        }
        av[n++] = tests[t];
        av[n] = NULL;

        printf("c: test: build %s\n", tests[t]);
        int rc = run_cmd(av);
        if (rc != 0) {
            failed = 1;
            continue;
        }
        printf("c: test: run %s\n", outbin);
        char *rav[] = {outbin, NULL};
        rc = run_cmd(rav);
        if (rc != 0) {
            fprintf(stderr, "c: test: %s failed (exit %d)\n", stem, rc);
            failed = 1;
        }
    }

    if (failed) {
        return C_EXIT_FAIL;
    }
    printf("c: test: ok (%d)\n", ntests);
    return C_EXIT_OK;
}
