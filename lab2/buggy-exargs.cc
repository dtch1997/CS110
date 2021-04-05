/**
 * File: buggy-exargs.cc
 * ---------------
 * Provides a version of the exargs executable, which simulates
 * a very constrained version of the xargs builtin.  Our exargs
 * tokenizes standard input around blanks and newlines, extends
 * the initial argument vector of its command to include
 * all of these extra tokens, executes the full command, waits
 * for the command to finish, and then returns 0 if everything
 * went smoothly and the command exited with code 0, and returns
 * 1 otherwise.
 *
 * **THERE IS A MEMORY BUG** in this program!  The program
 * is designed to be examined using valgrind and gdb so we
 * can better learn how to use tools to find memory errors
 * in a multiprocessing scenario.
 */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include "unistd.h"
#include <sys/wait.h>
using namespace std;

/* Reads as much text as possible from the given stream
 * (e.g. cin, cout, or another input stream) and populates
 * the given vector with the tokens in that input, tokenized
 * by spaces and newlines.
 */
static void pullAllTokens(istream& in, vector<string>& tokens) {
  while (true) {
    string line;
    getline(in, line);
    if (in.fail()) break;
    istringstream iss(line);
    while (true) {      
      string token;
      getline(iss, token, ' ');
      if (iss.fail()) break;
      tokens.push_back(token);
    }
  }
}

/* Converts the given vector of strings to C strings and adds
 * them to the array of C strings starting at the given address.
 */
static void addCPPStringsToCStringArray(vector<string>& strings, char **stringArr) {
  transform(strings.cbegin(), strings.cend(), stringArr,
            [](const string& str) { return const_cast<char *>(str.c_str()); });
}

int main(int argc, char *argv[]) {
  vector<string> tokens;
  pullAllTokens(cin, tokens);
  pid_t pidOrZero = fork();
  if (pidOrZero == 0) {
    char **exargsv = NULL;
    memcpy(exargsv, argv + 1, (argc - 1) * sizeof(char *));
    addCPPStringsToCStringArray(tokens, exargsv + argc - 1);
    exargsv[argc + tokens.size() - 1] = NULL;
    execvp(exargsv[0], exargsv);
    cerr << exargsv[0] << ": command not found" << endl;
    exit(0);
  }
  
  int status;
  waitpid(pidOrZero, &status, 0);

  // trivia: if all of status is 0, then child exited normally with code 0
  return status == 0 ? 0 : 1; 
}
