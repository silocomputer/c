#include "sync.h"

#include <c/c.h>

#include "fs.h"
#include "lock.h"
#include "run.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int sync_dir_is_git(const char *path) {
    char gitdir[512];
    if (snprintf(gitdir, sizeof(gitdir), "%s/.git", path) >= (int)sizeof(gitdir)) {
        return 0;
    }
    struct stat st;
    return stat(gitdir, &st) == 0;
}

static void sync_chomp(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

static int sync_rev_parse(const char *dir, const char *rev, char *out, size_t outsz) {
    char cmd[640];
    if (snprintf(cmd, sizeof(cmd), "git -C '%s' rev-parse '%s^{commit}' 2>/dev/null", dir, rev) >=
        (int)sizeof(cmd)) {
        return -1;
    }
    FILE *f = popen(cmd, "r");
    if (f == NULL) {
        return -1;
    }
    if (fgets(out, (int)outsz, f) == NULL) {
        pclose(f);
        return -1;
    }
    pclose(f);
    sync_chomp(out);
    return out[0] ? 0 : -1;
}

static int sync_one(const LockPin *pin) {
    if (strcmp(pin->name, "c") == 0 || strcmp(pin->name, "clang") == 0) {
        return 0;
    }
    if (pin->git_url[0] == '\0') {
        fprintf(stderr, "c: sync: %s: missing git+ URL\n", pin->name);
        return -1;
    }
    if (pin->git_ref[0] == '\0') {
        fprintf(stderr, "c: sync: %s: missing git ref\n", pin->name);
        return -1;
    }

    char dest[256];
    if (snprintf(dest, sizeof(dest), "deps/%s", pin->name) >= (int)sizeof(dest)) {
        return -1;
    }

    if (sync_dir_is_git(dest)) {
        char head[64] = {0};
        char want[64] = {0};
        if (sync_rev_parse(dest, "HEAD", head, sizeof(head)) == 0 &&
            sync_rev_parse(dest, pin->git_ref, want, sizeof(want)) == 0 &&
            strcmp(head, want) == 0) {
            printf("c: sync: %s already at %s\n", pin->name, pin->git_ref);
            return 0;
        }

        char *fetch_av[] = {"git", "-C", dest, "fetch", "--tags", "origin", NULL};
        int rc = run_cmd(fetch_av);
        if (rc != 0) {
            fprintf(stderr, "c: sync: warning: git fetch %s failed (%d)\n", pin->name, rc);
        }
        char *co_av[] = {"git", "-C", dest, "checkout", "--force", (char *)pin->git_ref, NULL};
        rc = run_cmd(co_av);
        if (rc != 0) {
            fprintf(stderr, "c: sync: checkout %s @ %s failed\n", pin->name, pin->git_ref);
            return -1;
        }
        printf("c: sync: updated %s -> %s\n", pin->name, pin->git_ref);
        return 0;
    }

    if (fs_mkdir_p("deps") != 0) {
        return -1;
    }

    char *clone_av[] = {"git", "clone", (char *)pin->git_url, dest, NULL};
    int rc = run_cmd(clone_av);
    if (rc != 0) {
        fprintf(stderr, "c: sync: clone %s failed\n", pin->name);
        return -1;
    }
    char *co_av[] = {"git", "-C", dest, "checkout", "--force", (char *)pin->git_ref, NULL};
    rc = run_cmd(co_av);
    if (rc != 0) {
        fprintf(stderr, "c: sync: checkout %s @ %s failed\n", pin->name, pin->git_ref);
        return -1;
    }
    printf("c: sync: cloned %s @ %s\n", pin->name, pin->git_ref);
    return 0;
}

int cmd_sync(int argc, char **argv) {
    (void)argc;
    (void)argv;

    FILE *f = fopen("c.lock", "r");
    if (f == NULL) {
        printf("c: sync: no c.lock (nothing to do)\n");
        return C_EXIT_OK;
    }
    fclose(f);

    Lock lock;
    if (lock_load(&lock, "c.lock") != 0) {
        return C_EXIT_FAIL;
    }

    int fetched = 0;
    for (int i = 0; i < lock.npins; i++) {
        LockPin *p = &lock.pins[i];
        if (strcmp(p->name, "c") == 0 || strcmp(p->name, "clang") == 0) {
            continue;
        }
        if (p->git_url[0] == '\0') {
            continue;
        }
        if (sync_one(p) != 0) {
            return C_EXIT_FAIL;
        }
        fetched++;
    }
    if (fetched == 0) {
        printf("c: sync: no module pins with git+ URLs\n");
    }
    return C_EXIT_OK;
}
