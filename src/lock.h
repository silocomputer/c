#ifndef C_LOCK_H
#define C_LOCK_H

enum {
    LOCK_MAX_PINS = 64,
    LOCK_MAX_NAME = 64,
    LOCK_MAX_FIELD = 128,
    LOCK_MAX_URL = 512
};

typedef struct LockPin {
    char name[LOCK_MAX_NAME];
    char version[LOCK_MAX_FIELD];
    char hash[LOCK_MAX_FIELD];
    char git_url[LOCK_MAX_URL]; /* https://… without git+ and without @ref */
    char git_ref[LOCK_MAX_FIELD];
} LockPin;

typedef struct Lock {
    LockPin pins[LOCK_MAX_PINS];
    int npins;
} Lock;

/* Load c.lock. Returns 0 on success. */
int lock_load(Lock *lock, const char *path);

#endif
