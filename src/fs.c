#include "fs.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum { FS_PATH_MAX = 512 };

int fs_mkdir_p(const char *path) {
    char tmp[FS_PATH_MAX];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "c: mkdir %s: %s\n", tmp, strerror(errno));
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "c: mkdir %s: %s\n", tmp, strerror(errno));
        return -1;
    }
    return 0;
}

int fs_mkdir_parent(const char *path) {
    char tmp[FS_PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        return -1;
    }
    char *slash = strrchr(tmp, '/');
    if (slash == NULL || slash == tmp) {
        return 0;
    }
    *slash = '\0';
    return fs_mkdir_p(tmp);
}
