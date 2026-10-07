#ifndef C_PKG_H
#define C_PKG_H

#include <stddef.h>

enum {
    PKG_MAX_PATH = 256,
    PKG_MAX_SOURCES = 512,
    PKG_MAX_DIRS = 64
};

/* Default package dir: "src" if it has .c, else "." if it has .c. */
int pkg_default(char *out, size_t outsz);

/* Like pkg_default, but under module root (e.g. deps/plat → deps/plat/src). */
int pkg_default_in(const char *modroot, char *out, size_t outsz);

/* List *.c in dir into out[] as "dir/file.c" (or "file.c" when dir is "."). */
int pkg_list_c(const char *dir, char out[][PKG_MAX_PATH], int max, int *nout);

#endif
