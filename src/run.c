#include "run.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int run_cmd(char *const argv[]) {
    if (argv == NULL || argv[0] == NULL) {
        fprintf(stderr, "c: run_cmd: empty argv\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "c: fork: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "c: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "c: waitpid: %s\n", strerror(errno));
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "c: %s killed by signal %d\n", argv[0], WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return -1;
}
