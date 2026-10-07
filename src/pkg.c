#include "pkg.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int pkg_is_dot_c(const char *name) {
    size_t n = strlen(name);
    return n > 2 && strcmp(name + n - 2, ".c") == 0;
}

static int pkg_dir_has_c(const char *dir) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        return 0;
    }
    int found = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (pkg_is_dot_c(ent->d_name)) {
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

int pkg_default(char *out, size_t outsz) {
    return pkg_default_in(".", out, outsz);
}

int pkg_default_in(const char *modroot, char *out, size_t outsz) {
    char srcpath[PKG_MAX_PATH];
    int n;
    if (strcmp(modroot, ".") == 0) {
        n = snprintf(srcpath, sizeof(srcpath), "src");
    } else {
        n = snprintf(srcpath, sizeof(srcpath), "%s/src", modroot);
    }
    if (n < 0 || n >= (int)sizeof(srcpath)) {
        return -1;
    }
    if (pkg_dir_has_c(srcpath)) {
        if (snprintf(out, outsz, "%s", srcpath) >= (int)outsz) {
            return -1;
        }
        return 0;
    }
    if (pkg_dir_has_c(modroot)) {
        if (snprintf(out, outsz, "%s", modroot) >= (int)outsz) {
            return -1;
        }
        return 0;
    }
    fprintf(stderr, "c: no default package under %s (need src/*.c or *.c)\n", modroot);
    return -1;
}

static int pkg_src_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int pkg_list_c(const char *dir, char out[][PKG_MAX_PATH], int max, int *nout) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "c: cannot open %s\n", dir);
        return -1;
    }

    *nout = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!pkg_is_dot_c(ent->d_name)) {
            continue;
        }
        if (*nout >= max) {
            fprintf(stderr, "c: too many sources in %s\n", dir);
            closedir(d);
            return -1;
        }
        int n;
        if (strcmp(dir, ".") == 0) {
            n = snprintf(out[*nout], PKG_MAX_PATH, "%s", ent->d_name);
        } else {
            n = snprintf(out[*nout], PKG_MAX_PATH, "%s/%s", dir, ent->d_name);
        }
        if (n < 0 || n >= PKG_MAX_PATH) {
            fprintf(stderr, "c: source path too long\n");
            closedir(d);
            return -1;
        }
        (*nout)++;
    }
    closedir(d);
    qsort(out, (size_t)*nout, PKG_MAX_PATH, pkg_src_cmp);
    return 0;
}
