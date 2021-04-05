/**
 * File: subprocess-test.cc
 * ------------------------
 * Simple unit test framework in place to exercise functionality of the subprocess function.
 */

#include "subprocess.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/wait.h>
#include <ext/stdio_filebuf.h>
#include <vector>

using namespace __gnu_cxx; // __gnu_cxx::stdio_filebuf -> stdio_filebuf
using namespace std;

/**
 * File: publishWordsToChild
 * -------------------------
 * Algorithmically self-explanatory.  Relies on a g++ extension where iostreams can
 * be wrapped around file desriptors so that we can use operator<<, getline, endl, etc.
 */
static void publishWordsToChild(int to, const std::vector<string>& words) {
  stdio_filebuf<char> outbuf(to, std::ios::out);
  ostream os(&outbuf); // manufacture an ostream out of a write-oriented file descriptor so we can use C++ streams semantics (prettier!)
  for (const string& word: words) os << word << endl;
} // stdio_filebuf destroyed, destructor calls close on desciptor it owns
    
/**
 * File: ingestAndPublishWords
 * ---------------------------
 * Reads in everything from the provided file descriptor, which should be
 * the sorted content that the child process running /usr/bin/sort publishes to
 * its standard out.  Note that we once again rely on the same g++ extension that
 * allows us to wrap an iostream around a file descriptor so we have C++ stream semantics
 * available to us.
 */
static void ingestAndPublishWords(int from) {
  stdio_filebuf<char> inbuf(from, std::ios::in);
  istream is(&inbuf);
  size_t wordCount = 1;
  while (true) {
    string word;
    getline(is, word);
    if (is.fail()) break;
    cout << wordCount++ << ": ";
    cout << word << endl;
  }
} // stdio_filebuf destroyed, destructor calls close on desciptor it owns

/**
 * Function: waitForChildProcess
 * -----------------------------
 * Halts execution until the process with the provided id exits.
 */
static void waitForChildProcess(pid_t pid) {
  if (waitpid(pid, NULL, 0) != pid) {
    throw SubprocessException("Encountered a problem while waiting for subprocess's process to finish.");
  }
}


/*
 *Tests all 4 cases for subprocess.  Here's what *should* happen:
 * (true, true) - child reads from the parent, parent reads child output and prints it
 * (true, false) - child reads from the parent, child prints to stdout
 * (false, true) - child reads from file, parent reads child output and prints it
 * (false, false) - child reads from file, child prints to stdout
 */
static void defaultTest() {
    const std::vector<string> kWords = {"put", "a", "ring", "on", "it"};
    const string kSortExecutable = "/usr/bin/sort";
    const string kUserInputFile = "input/subprocess-default.txt";
    for (int supply = 0; supply < 2; supply++) {
        for (int ingest = 0; ingest < 2; ingest++) {
            string supplyStr = supply ? "true" : "false";
            string ingestStr = ingest ? "true" : "false";
            char *argv[] = {const_cast<char *>(kSortExecutable.c_str()), NULL};
            cout << "Testing supply=" << supplyStr << ", ingest=" << ingestStr << endl;
            if (!supply) {
                cout << "You must type the input, and type ctrl-D" << endl
                     << "on its own line to end input!" << endl;
            }
            subprocess_t child = subprocess(argv, supply, ingest);
            if (child.supplyfd != kNotInUse) publishWordsToChild(child.supplyfd, kWords);
            if (child.ingestfd != kNotInUse) ingestAndPublishWords(child.ingestfd);
            waitForChildProcess(child.pid);
            cout << "Done testing " << supplyStr << ", " << ingestStr << endl << endl;
        }
    }
}

/*  Uses subprocesses and sleeping to sort an array.
 *  Expected output: 1 \n 2 \n 3 \n 4
 */
static void sleepsortTest() {
    const string kExecutable = "./scripts/sleep_and_echo.sh";
    const int numbers[4] = {4,2,3,1};
    cout << "Sleep-sorting an array of numbers";
    cout << "Testing supply=true, ingest=false" << endl;
    subprocess_t children[4];
    char* argv[] = {const_cast<char*>(kExecutable.c_str()), NULL};
    for (int i = 0; i < 4; i ++) {
        children[i] = subprocess(argv, true, false);
        const std::vector<string> words = {std::to_string(numbers[i])};
        publishWordsToChild(children[i].supplyfd, words);
    }

    for (int i = 0; i < 4; i ++) {
        waitpid(-1, NULL, 0);
    }
}

/*  A parent arguing with its child over who knows more. 
 */
static void knowTest() {
    const string kExecutable = "./scripts/echo_you_know.sh";
    cout << "Constructing tower of knowledge" << endl;
    char* argv[] = {const_cast<char*>(kExecutable.c_str()), NULL};
    
    std::vector<string> line = {"I know"};
    for (int i = 0; i < 10; i ++) {
        subprocess_t child = subprocess(argv, true, true);
        stdio_filebuf<char> inbuf(child.ingestfd, std::ios::in);
        istream is(&inbuf);
        string word;
        publishWordsToChild(child.supplyfd, line);
        getline(is, word);
        cout << word << endl;
        line[0] = "I know " + word;
        cout << line[0] << endl;
        waitpid(child.pid, NULL, 0); 
    }
}

/*  Parent rolls multiple dice and returns the sum. 
 */
static void diceTest() {
    const string kExecutable = "./scripts/roll_dice.sh";
    cout << "Rolling a pair of dice" << endl;
    char* argv[] = {const_cast<char*>(kExecutable.c_str()), NULL};

    int diceSum = 0;
    for (int i = 0; i < 2; i ++) {
        subprocess_t child = subprocess(argv, true, true);
        stdio_filebuf<char> inbuf(child.ingestfd, std::ios::in);
        istream is(&inbuf);
        string word;
        getline(is, word);
        int diceResult = std::stoi(word);
        diceSum += diceResult;
        waitpid(-1, NULL, 0);
    }
    cout << "Rolled a " << diceSum << endl;

}

typedef void(*unitTest)(void);
const std::vector<unitTest> unitTests = {defaultTest, sleepsortTest, knowTest, diceTest};

/**
 * Function: main
 * --------------
 * Serves as the entry point for for the unit test.
 */
int main(int argc, char *argv[]) {
    for (auto& unitTest: unitTests) {
        try {
            unitTest();
        } catch (const SubprocessException& se) {
            cerr << "Problem encountered" << endl;
            cerr << "More details here: " << se.what() << endl;
            return 1;
        } catch (...) { // ... here means catch everything else
            cerr << "Unknown internal error." << endl;
            return 2;
        }
    }
}
