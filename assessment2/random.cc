// Hello world! Cplayground is an online sandbox that makes it easy to try out
// code.

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

const int kNumChildren = 1; // one child per byte
static char shared_memory[1];

void* create_shared_memory(size_t size) {
    // Assume this function creates memory 
    // that can be shared between processes.
    // The pointer that is returned is
    // available to all processes (parent and
    // children), and when one process modifies
    // the values in memory, all processes
    // immediately see the change.
    return (void *) shared_memory;
}

int main(int argc, char *argv[]) {
    printf("Starting generation of random numbers...\n");
    char *go = (char*) create_shared_memory(1); // need one shared byte
    *go = false;

    // TODO: complete this function
    
    int fds[2];
    pipe(fds);
    
    for (int i = 0; i < kNumChildren; i ++ ) {
        pid_t pid = fork(); 
        if (pid == 0) {
            // Close read end of pipe
            close(fds[0]);
            while (!(*go)) {};
            dprintf(fds[1], "%c", i);
            close(fds[1]);
            return 0;
        }
    }
    // Close write end of pipe
    close(fds[1]);
    sleep(1);
    *go = true;
    
    int num_bytes_read = 0;
    char buffer[kNumChildren];
    while (num_bytes_read < kNumChildren) {
        num_bytes_read += read(
            fds[0], 
            buffer + num_bytes_read, 
            kNumChildren - num_bytes_read
        );    
    }
    
    printf("Random bytes:\n");
    for (int i = 0; i < kNumChildren; i ++ ) {
        printf("%d,", buffer[i]);
    }

    
    
    return 0;
}
