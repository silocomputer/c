#ifndef C_RUN_H
#define C_RUN_H

/* Exec argv[0]…; wait. Returns process exit status, or -1 on spawn error. */
int run_cmd(char *const argv[]);

#endif
