#include "mod.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mod_clear(Mod *m) {
    memset(m, 0, sizeof(*m));
    m->level = -1;
    m->freestanding = -1;
}

static char *mod_skip_ws(char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static void mod_trim_inplace(char *s) {
    char *start = mod_skip_ws(s);
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
    for (char *p = s; *p; p++) {
        if (*p == '#') {
            *p = '\0';
            break;
        }
    }
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
}

static int mod_copy_field(char *dst, size_t dstsz, const char *src) {
    if (src == NULL || src[0] == '\0') {
        return -1;
    }
    size_t n = strlen(src);
    if (n >= dstsz) {
        return -1;
    }
    memcpy(dst, src, n + 1);
    return 0;
}

static int mod_parse_line(Mod *m, char *line, int lineno, const char *path) {
    mod_trim_inplace(line);
    if (line[0] == '\0') {
        return 0;
    }

    char *key = line;
    char *val = line;
    while (*val && !isspace((unsigned char)*val)) {
        val++;
    }
    if (*val) {
        *val++ = '\0';
        val = mod_skip_ws(val);
    }

    if (strcmp(key, "module") == 0) {
        if (mod_copy_field(m->name, sizeof(m->name), val) != 0) {
            fprintf(stderr, "c: %s:%d: bad module name\n", path, lineno);
            return -1;
        }
        return 0;
    }
    if (strcmp(key, "level") == 0) {
        if (val[0] == '\0') {
            fprintf(stderr, "c: %s:%d: missing level\n", path, lineno);
            return -1;
        }
        char *end = NULL;
        long lv = strtol(val, &end, 10);
        if (end == val || *end != '\0' || lv < 0 || lv > 100000) {
            fprintf(stderr, "c: %s:%d: bad level\n", path, lineno);
            return -1;
        }
        m->level = (int)lv;
        return 0;
    }
    if (strcmp(key, "freestanding") == 0) {
        if (strcmp(val, "0") == 0) {
            m->freestanding = 0;
        } else if (strcmp(val, "1") == 0) {
            m->freestanding = 1;
        } else {
            fprintf(stderr, "c: %s:%d: freestanding must be 0 or 1\n", path, lineno);
            return -1;
        }
        return 0;
    }
    if (strcmp(key, "require") == 0) {
        if (val[0] == '\0') {
            fprintf(stderr, "c: %s:%d: missing require name\n", path, lineno);
            return -1;
        }
        if (m->nrequire >= MOD_MAX_REQUIRES) {
            fprintf(stderr, "c: %s:%d: too many requires\n", path, lineno);
            return -1;
        }
        if (mod_copy_field(m->require[m->nrequire], MOD_MAX_NAME, val) != 0) {
            fprintf(stderr, "c: %s:%d: bad require name\n", path, lineno);
            return -1;
        }
        m->nrequire++;
        return 0;
    }

    /* kind / thread / alloc / unknown — ignored */
    return 0;
}

int mod_load(Mod *m, const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "c: cannot open %s\n", path);
        return -1;
    }

    mod_clear(m);

    char line[512];
    int lineno = 0;
    int rc = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        lineno++;
        size_t len = strlen(line);
        if (len == sizeof(line) - 1 && line[len - 1] != '\n') {
            fprintf(stderr, "c: %s:%d: line too long\n", path, lineno);
            rc = -1;
            break;
        }
        if (mod_parse_line(m, line, lineno, path) != 0) {
            rc = -1;
            break;
        }
    }
    fclose(f);

    if (rc != 0) {
        return -1;
    }
    if (m->name[0] == '\0') {
        fprintf(stderr, "c: %s: missing module name\n", path);
        return -1;
    }
    if (m->level < 0) {
        fprintf(stderr, "c: %s: missing level\n", path);
        return -1;
    }
    if (m->freestanding < 0) {
        fprintf(stderr, "c: %s: missing freestanding\n", path);
        return -1;
    }
    return 0;
}
