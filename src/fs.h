#ifndef C_FS_H
#define C_FS_H

/* mkdir -p. Returns 0 on success. */
int fs_mkdir_p(const char *path);

/* mkdir parent of path if any. Returns 0 on success. */
int fs_mkdir_parent(const char *path);

#endif
