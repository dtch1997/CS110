/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#define _GNU_SOURCE 
#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include "fork-utils.h"  // this has to be the last #include'd statement in the file

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
    int pipefds[2];
    pipe2(pipefds, O_CLOEXEC);
    
    pids[0] = fork();
    if (pids[0] == 0) {
        close(pipefds[0]);
        dup2(pipefds[1], STDOUT_FILENO);
        close(pipefds[1]);
        execvp(argv1[0], argv1);
        return;
    }
    close(pipefds[1]);

    pids[1] = fork();
    if (pids[1] == 0) {
        dup2(pipefds[0], STDIN_FILENO);
        close(pipefds[0]);
        execvp(argv2[0], argv2);
        return;
    }
    close(pipefds[0]);
}
