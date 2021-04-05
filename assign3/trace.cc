/**
 * File: trace.cc
 * ----------------
 * Presents the implementation of the trace program, which traces the execution of another
 * program and prints out information about every single system call it makes.  For each system call,
 * trace prints:
 *
 *    + the name of the system call,
 *    + the values of all of its arguments, and
 *    + the system call return value
 */

#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h> // for fork, execvp
#include <string.h> // for memchr, strerror
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include "trace-options.h"
#include "trace-error-constants.h"
#include "trace-system-calls.h"
#include "trace-exception.h"
#include "fork-utils.h" // this has to be the last #include statement in this file
using namespace std;

const unsigned long scArgAddresses[6] = {
    RDI, RSI, RDX, R10, R8, R9
};
/*
 * This provided function can be used as-is for full trace to read a
 * string from the tracee's virtual address space.  It uses ptrace
 * and PTRACE_PEEKDATA to extract the sequence of characters in chunks
 * until finding a null terminator.  Returns the C++ string.
 *
 * pid is the PID of the tracee process, and addr is a char * read
 * from an argument register via PTRACE_PEEKUSER.
 */
static string readString(pid_t pid, unsigned long addr) {
  string str;
  while (true) {
    long ret = ptrace(PTRACE_PEEKDATA, pid, addr);
    const char *beginning = reinterpret_cast<const char *>(&ret);
    const char *end = reinterpret_cast<const char *>(memchr(beginning, 0, sizeof(long)));
    size_t numChars = end == NULL ? sizeof(long) : end - beginning;
    str += string(beginning, beginning + numChars);
    if (end != NULL) return str; // contains a '\0'
    addr += sizeof(long);
  }
}


static int continueAndGetRegister(pid_t pid, int *status, unsigned long reg, long* val) {
    // Continues to the first syscall and gets the specified register value
    while (true) {
        ptrace(PTRACE_SYSCALL, pid, 0, 0);
        waitpid(pid, status, 0);
        if (WIFEXITED(*status)) {
            return -1;
        }
        if (WIFSTOPPED(*status) && (WSTOPSIG(*status) == (SIGTRAP | 0x80))) {
            *val = ptrace(PTRACE_PEEKUSER, pid, reg * sizeof(long));
            return 0;
        }
    }
}

static int printSyscallInfo(pid_t pid, int* status, bool simple,
        const std::map<int, std::string>& systemCallNumbers,
        const std::map<std::string, systemCallSignature>& systemCallSignatures, 
        const std::map<int, std::string>& errorConstants) {
    // Prints info for one system call
    // In simple mode:
    // -- opcode followed by return value
    //
    // In full mode: 
    // -- function name, function signature, and return value
    long val = 0;
    std::string scName;

    int err = continueAndGetRegister(pid, status, ORIG_RAX, &val);
    if (simple) {
        cout << "syscall(" << val << ") = " << flush;
    } else {
        // Print function name
        scName = systemCallNumbers.at(val);
        cout << scName << "(";
        
        if (systemCallSignatures.find(scName) == systemCallSignatures.end()) {
            // system call signature not found
            cout << "<signature_information_missing>";
        } else {
            // Iteratively print function arguments
            systemCallSignature scSig = systemCallSignatures.at(scName);
            for (int i = 0; i < scSig.size(); i++) {
                scParamType scParam = scSig[i];
                // Logic to determine how to print
                long argval = ptrace(PTRACE_PEEKUSER, pid, scArgAddresses[i] * sizeof(long));
                switch (scParam) {
                    case SYSCALL_INTEGER: {
                        cout << (int) argval; 
                        break;
                    }
                    case SYSCALL_STRING: {
                        std::string str = readString(pid, (unsigned long)argval);
                        cout << '"' << str << '"';
                        break;
                    }
                    case SYSCALL_POINTER: {
                        if ((void*)argval == NULL) cout << "NULL";
                        else cout << (void*) argval;
                        break;
                    }
                    case SYSCALL_UNKNOWN_TYPE: {
                        cout << "<unknown_type>";
                        break;
                    }   
                }

                // If it's the last argument, don't print the comma
                cout << (i == scSig.size() - 1 ? "" : ", ");
            }
        }
        cout << ") = " << flush;  
    }

    err = continueAndGetRegister(pid, status, RAX, &val);
    if (err < 0) {
        cout << "<no return>" << endl;
        return -1;
    }

    if (simple) {
        cout << val;
    } else {
       if (scName == "brk" || scName == "mmap") cout << (void*) val;
       else if (val >= 0) cout << (int) val;
       else {
           cout << -1 << " ";
           int errorNum = abs(val);
           std::string errorConstant = errorConstants.at(errorNum);
           const char * errorMessage = strerror(errorNum);
           cout << errorConstant << " ";
           cout << "(" << std::string(errorMessage) << ")";
       }
    }
    cout << endl;
    return 0;
}



int main(int argc, char *argv[]) {
  bool simple = false, rebuild = false;
  int numFlags = processCommandLineFlags(simple, rebuild, argv);
  if (argc - numFlags == 1) {
    cout << "Nothing to trace... exiting." << endl;
    return 0;
  }

  pid_t pid = fork();
  if (pid == 0) {
    ptrace(PTRACE_TRACEME);
    raise(SIGSTOP);
    execvp(argv[numFlags + 1], argv + numFlags + 1);
    return 0;
  }

  int status; 
  waitpid(pid, &status, 0);
  assert(WIFSTOPPED(status));
  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

  std::map<int, std::string> systemCallNumbers;
  std::map<std::string, systemCallSignature> systemCallSignatures;
  std::map<int, std::string> errorConstants;
  if (!simple) {
    compileSystemCallData(systemCallNumbers, systemCallSignatures, rebuild);
    compileSystemCallErrorStrings(errorConstants);
  }

  while(true) {
    int err = printSyscallInfo(pid, &status, simple, systemCallNumbers, systemCallSignatures, errorConstants);
    if (err < 0) break;
  };

  cout << "Program exited normally with status " << WEXITSTATUS(status) << endl;
  return 0;
}
