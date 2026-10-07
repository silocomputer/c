#include "lock.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char *lock_skip_ws(char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static void lock_trim(char *s) {
    char *start = lock_skip_ws(s);
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

static int lock_copy(char *dst, size_t dstsz, const char *src) {
    size_t n = strlen(src);
    if (n >= dstsz) {
        return -1;
    }
    memcpy(dst, src, n + 1);
    return 0;
}

/* git+https://host/path@ref → url + ref */
static int lock_parse_git(const char *field, char *url, size_t urlsz, char *ref, size_t refsz) {
    if (strncmp(field, "git+", 4) != 0) {
        return -1;
    }
    const char *rest = field + 4;
    const char *at = strrchr(rest, '@');
    /* Don't treat userinfo @ in https://user:pass@host as ref — only path @ref.
       Heuristic: @ after the host (after third slash) or if no :// before @. */
    if (at != NULL) {
        const char *scheme = strstr(rest, "://");
        if (scheme != NULL) {
            const char *path = strchr(scheme + 3, '/');
            if (path == NULL || at < path) {
                at = NULL; /* @ is in authority */
            }
        }
    }
    if (at != NULL) {
        size_t ulen = (size_t)(at - rest);
        if (ulen >= urlsz || ulen == 0) {
            return -1;
        }
        memcpy(url, rest, ulen);
        url[ulen] = '\0';
        if (lock_copy(ref, refsz, at + 1) != 0 || ref[0] == '\0') {
            return -1;
        }
    } else {
        if (lock_copy(url, urlsz, rest) != 0 || url[0] == '\0') {
            return -1;
        }
        ref[0] = '\0';
    }
    return 0;
}

static int lock_parse_line(Lock *lock, char *line, int lineno, const char *path) {
    lock_trim(line);
    if (line[0] == '\0') {
        return 0;
    }
    if (lock->npins >= LOCK_MAX_PINS) {
        fprintf(stderr, "c: %s:%d: too many pins\n", path, lineno);
        return -1;
    }

    char *fields[8];
    int nfields = 0;
    char *p = line;
    while (*p && nfields < 8) {
        p = lock_skip_ws(p);
        if (*p == '\0') {
            break;
        }
        fields[nfields++] = p;
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }
        if (*p) {
            *p++ = '\0';
        }
    }
    if (nfields < 1) {
        return 0;
    }

    LockPin *pin = &lock->pins[lock->npins];
    memset(pin, 0, sizeof(*pin));
    if (lock_copy(pin->name, sizeof(pin->name), fields[0]) != 0) {
        fprintf(stderr, "c: %s:%d: bad pin name\n", path, lineno);
        return -1;
    }

    for (int i = 1; i < nfields; i++) {
        if (strncmp(fields[i], "git+", 4) == 0) {
            if (lock_parse_git(fields[i], pin->git_url, sizeof(pin->git_url), pin->git_ref,
                               sizeof(pin->git_ref)) != 0) {
                fprintf(stderr, "c: %s:%d: bad git URL\n", path, lineno);
                return -1;
            }
            continue;
        }
        if (strncmp(fields[i], "h1:", 3) == 0) {
            if (lock_copy(pin->hash, sizeof(pin->hash), fields[i]) != 0) {
                fprintf(stderr, "c: %s:%d: bad hash\n", path, lineno);
                return -1;
            }
            continue;
        }
        if (pin->version[0] == '\0') {
            if (lock_copy(pin->version, sizeof(pin->version), fields[i]) != 0) {
                fprintf(stderr, "c: %s:%d: bad version\n", path, lineno);
                return -1;
            }
            continue;
        }
        /* extra field — ignore */
    }

    /* If git URL had no @ref, use version as ref. */
    if (pin->git_url[0] && pin->git_ref[0] == '\0' && pin->version[0]) {
        if (lock_copy(pin->git_ref, sizeof(pin->git_ref), pin->version) != 0) {
            return -1;
        }
    }

    lock->npins++;
    return 0;
}

int lock_load(Lock *lock, const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "c: cannot open %s\n", path);
        return -1;
    }
    memset(lock, 0, sizeof(*lock));

    char line[1024];
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
        if (lock_parse_line(lock, line, lineno, path) != 0) {
            rc = -1;
            break;
        }
    }
    fclose(f);
    return rc;
}
