/**
 * File: subprocess.h
 * ------------------
 * Exports a custom data type to bundle everything needed to spawn a new process,
 * manage that process, and optionally publish to its stdin and/or read from its stdout.
 *
 * See subprocess-test.cc for an example client program.
 */

#pragma once
#include <unistd.h> // for pid_t
#include <set>      // for set, obvi
#include "subprocess-exception.h"

/**
 * Constant: kNotInUse
 * -------------------
 * Constant residing in a descriptor field within a subprocess_t when
 * the descriptor isn't being used.
 */
 
static const int kNotInUse = -1;

/**
 * Type: subprocess_t
 * ------------------
 * Bundles information related to the child process created
 * by the subprocess function below.
 * 
 *  pid: the id of the child process created by a call to subprocess
 *  supplyfd: the descriptor where one pipes text to the child's stdin (or kNotInUse if child hasn't rewired its stdin)
 *  ingestfd: the descriptor where the text a child pushes to stdout shows up (or kNotInUse if child hasn't rewired its stdout)
 *
 */

struct subprocess_t {
  pid_t pid;
  int supplyfd;
  int ingestfd;
};
 
/**
 * Function: subprocess
 * --------------------
 * Creates a new process running the executable identified via argv[0].
 *
 *   argv: the NULL-terminated argument vector that should be passed to the new process's main function
 *   supplyChildInput: true if the parent process would like to pipe content to the new process's stdin, false otherwise
 *   ingestChildOutput: true if the parent would like the child's stdout to be pushed to the parent, false otheriwse
 */
subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException);

