#ifndef C_MOD_H
#define C_MOD_H

enum {
    MOD_MAX_NAME = 64,
    MOD_MAX_REQUIRES = 64
};

typedef struct Mod {
    char name[MOD_MAX_NAME];
    int level;
    int freestanding;
    char require[MOD_MAX_REQUIRES][MOD_MAX_NAME];
    int nrequire;
} Mod;

/* Load c.mod. Recognizes module/level/freestanding/require; ignores other keys. */
int mod_load(Mod *m, const char *path);

#endif
