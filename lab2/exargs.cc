/**
 * File: exargs.cc
 * ---------------
 * Provides a full version of the exargs executable, which simulates
 * a very constrained version of the xargs builtin.  Our exargs
 * tokenizes standard input around blanks and newlines, extends
 * the initial argument vector of its command to include
 * all of these extra tokens, executes the full command, waits
 * for the command to finish, and then returns 0 if everything
 * went smoothly and the command exited with code 0, and returns
 * 1 otherwise.
 *
 * There is nothing to be written here.  This is just included in the
 * lab2 folder so you have a working version of exargs.
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
  // TODO: your code here!
  return 0;
}
