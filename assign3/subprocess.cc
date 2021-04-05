/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"

#include <fcntl.h>
#include <unistd.h>
#include "fork-utils.h" // this has to be the very last #include statement in this .cc file!
using namespace std;

static pid_t fork_errchk(void) {
    pid_t child = fork();
    if (child < 0) throw SubprocessException("Forking failed");
    return child;
}

static int pipe2_errchk(int pipefds[], int flags) {
    int err = pipe2(pipefds, flags);
    if (err < 0) throw SubprocessException("Creating pipe failed");
    return err;
}

static int dup2_errchk(int oldfd, int newfd) {
    int err = dup2(oldfd, newfd);
    if (err < 0) throw SubprocessException("Duplicating file desciptor failed");
    return err;
}

static int execvp_errchk(const char * path, char * const argv[]) {
    int err = execvp(path, argv);
    if (err < 0) throw SubprocessException("Executing new program failed");
    return err;
}

static int close_errchk(int fd) {
    int err = close(fd);
    char err_msg[128];
    sprintf(err_msg, "Closing file descriptor %d failed", fd);
    if (err < 0) throw SubprocessException(err_msg);
    return err;
}

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {
    int child_to_parent_fds[2];
    int parent_to_child_fds[2];
    if (supplyChildInput) pipe2_errchk(parent_to_child_fds, O_CLOEXEC);
    if (ingestChildOutput) pipe2_errchk(child_to_parent_fds, O_CLOEXEC);
    
    pid_t child = fork_errchk();
    if (child == 0) {
        if (supplyChildInput) {
            // Child doesn't write to parent-to-child pipe
            close_errchk(parent_to_child_fds[1]);
            dup2_errchk(parent_to_child_fds[0], STDIN_FILENO);
            // Close duplicate fd
            close_errchk(parent_to_child_fds[0]);
        }
        if (ingestChildOutput) {
            // Child doesn't read from child-to-parent pipe
            close_errchk(child_to_parent_fds[0]);
            dup2_errchk(child_to_parent_fds[1], STDOUT_FILENO);
            // Close duplicate fd
            close_errchk(child_to_parent_fds[1]);
        }
        execvp_errchk(argv[0], argv);
        exit(0);
    }
    // Parent doesn't read from parent-to-child pipe
    if (supplyChildInput) close_errchk(parent_to_child_fds[0]);
    // Parent doesn't write to child-to-parent pipe
    if (ingestChildOutput) close_errchk(child_to_parent_fds[1]); 

    subprocess_t subprocess;
    subprocess.pid = child;
    subprocess.supplyfd = supplyChildInput ? parent_to_child_fds[1] : kNotInUse;
    subprocess.ingestfd = ingestChildOutput ? child_to_parent_fds[0] : kNotInUse;
    return subprocess;
}
