/**
 * File: stsh.cc
 * -------------
 * Defines the entry point of the stsh executable.
 */

#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-parser/stsh-parse-exception.h"
#include "stsh-signal.h"
#include "stsh-job-list.h"
#include "stsh-job.h"
#include "stsh-process.h"
#include "stsh-parse-utils.h"
#include <cstring>
#include <iostream>
#include <string>
#include <cassert>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>  // for fork
#include <signal.h>  // for kill
#include <sstream>
#include <sys/wait.h>
#include "fork-utils.h" // this needs to be the last #include in the list
using namespace std;

// The builtin supported shell commands
static const string kSupportedBuiltins[] = {"quit", "exit", "fg", "bg", "slay", "halt", "cont", "jobs"};
static const size_t kNumSupportedBuiltins = sizeof(kSupportedBuiltins) / sizeof(kSupportedBuiltins[0]);

// the one piece of global data we need so signal handlers can access it
static STSHJobList joblist;

// Set the global logging level
enum LoggingLevel {DEBUG=0, INFO=1, WARNING=2, ERROR=3, SILENT=4};
const static char * kLoggingLevelNames[] = {"DBG", "INFO", "WARN", "ERR", "SIL"};
const static LoggingLevel kGlobalLoggingLevel = SILENT;
// Define error message length for STSHException
// It is set to 128 because we don't currently need anything longer
const static int kMaxErrorMessageLength = 128;

template<typename T>
void debugLog(LoggingLevel level,
         const std::string& group,
         const T& message) {
     if (level >= kGlobalLoggingLevel) {
         std::stringstream strbuf;
         strbuf << "[" << std::string(kLoggingLevelNames[level]) << "]\t[" << group << "] \t" << message  << std::endl;
         std::cerr << strbuf.str() << std::flush;
    }
}

static void blockSignal(int sig) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

static void unblockSignal(int sig) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

static void waitForForeground() {
    const std::string debugGroup = "WaitFG";
    sigset_t set;
    sigemptyset(&set);
    while(joblist.hasForegroundJob()) {
        debugLog(DEBUG, debugGroup, "Waiting for foreground job");
        sigsuspend(&set);
    }
}
/* System calls with built in error checking
 */ 

static pid_t fork_errchk(void) {
    pid_t child = fork();
    if (child < 0) throw STSHException("Forking failed");
    return child;
}

static int pipe2_errchk(int pipefds[], int flags) {
    int err = pipe2(pipefds, flags);
    if (err < 0) throw STSHException("Creating pipe failed");
    return err;
}

static int dup2_errchk(int oldfd, int newfd) {
    int err = dup2(oldfd, newfd);
    if (err < 0) throw STSHException("Duplicating file desciptor failed");
    return err;
}

static int execvp_errchk(const char * path, char * const argv[]) {
    int err = execvp(path, argv);
    if (err < 0) throw STSHException(std::string(path) + ": Command not found.");
    return err;
}

static int open_errchk(const char * pathname, int flags, mode_t mode = 0644) {
    int err = open(pathname, flags, mode);
    if (err < 0) {
        char err_msg[kMaxErrorMessageLength];
        sprintf(err_msg, "%s: open(%s,%d) failed", strerror(errno), pathname, flags);
        throw STSHException(err_msg);
    }
    return err;
}

static int close_errchk(int fd) {
    int err = close(fd);
    char err_msg[kMaxErrorMessageLength];
    sprintf(err_msg, "Closing file descriptor %d failed", fd);
    if (err < 0) throw STSHException(err_msg);
    return err;
}

static int setpgid_errchk(pid_t pid, pid_t pgid) {
    int err = setpgid(pid, pgid);
    char err_msg[kMaxErrorMessageLength];
    sprintf(err_msg, "%s: Setting pgid of process %d failed", strerror(errno), pid);
    if (err < 0) throw STSHException(err_msg);
    return err;
}

static int killpg_errchk(pid_t pgrp, int sig) {
    int err = killpg(pgrp, sig);
    if (err < 0) {
        char err_msg[kMaxErrorMessageLength];
        sprintf(err_msg, "%s: Sending signal %d to process group %d failed", strerror(errno), sig, pgrp);
        throw STSHException(err_msg);
    }
    return err;
}

static int kill_errchk(pid_t pid, int sig) {
    int err = kill(pid, sig);
    if (err < 0) {
        char err_msg[kMaxErrorMessageLength];
        sprintf(err_msg, "%s: Sending signal %d to process %d failed", strerror(errno), sig, pid);
        throw STSHException(err_msg);
    }
    return err;
}

static int tcgetpgrp_errchk(int fd) {
    int err = tcgetpgrp(fd);
    if (err < 0) {
        char err_msg[kMaxErrorMessageLength];
        sprintf(err_msg, "%s: tcgetpgrp(%d) failed", strerror(errno), fd);
        throw STSHException(err_msg);
    }
    return err;
}
static int tcsetpgrp_errchk(int fd, pid_t pgrp) {
    int err = tcsetpgrp(fd, pgrp);
    if (err < 0) {
        char err_msg[kMaxErrorMessageLength];
        sprintf(err_msg, "%s: tcsetpgrp(%d,%d) failed", strerror(errno), fd, pgrp);
        throw STSHException(err_msg);
    }
    return err;
}

/* Helper functions for handling builtin commands
 */

static size_t numArguments(const command& c) {
    size_t numArguments = 0;
    for (int i = 0; i < sizeof(c.tokens) / sizeof(c.tokens[0]); i ++) {
        if (c.tokens[i] == NULL) return numArguments;
        numArguments += 1;
    }
    throw STSHException("Command.tokens field is not NULL-delimited");
}

/* Resume execution of a job.
 * Used in the 'fg' and 'bg' builtins
 */
static void resume(const command& c, const std::string& builtin_name, bool inForeground) {
    const std::string debugGroup = builtin_name;
    const std::string& usage = "Usage: " + builtin_name + " <jobid>.";
    if (numArguments(c) != 1) throw STSHException(usage); 
    size_t jobID = parseNumber(c.tokens[0], usage);
    // Block here to ensure that jobID remains valid until end of the execution block
    blockSignal(SIGCHLD);
    if (!joblist.containsJob(jobID)) throw STSHException(builtin_name + " " + std::to_string(jobID) + ": No such job.");
    STSHJob& job = joblist.getJob(jobID);
    bool jobIsRunning = false;
    for (const auto& p : job.getProcesses()) {
        if (p.getState() == kRunning) jobIsRunning = true;
    }
    debugLog(DEBUG, debugGroup, 
            "Found job " + std::to_string(jobID) + (jobIsRunning ? " running " : " stopped ") +
            " in the " +  (job.getState() == kForeground ? "foreground" : "background"));
    debugLog(DEBUG, debugGroup, std::string("Moving job to the ") + (inForeground ? "foreground" : "background"));

    job.setState(inForeground ? kForeground : kBackground);
    if (!jobIsRunning) killpg_errchk(job.getGroupID(), SIGCONT);
    if (inForeground) waitForForeground();
    unblockSignal(SIGCHLD);
}

/* Send a signal to the specified process. 
 * Used in the 'slay', 'halt', 'cont' builtins
 */
static void sendSignal(const command& c, const std::string builtin_name, int sig) {
    const std::string debugGroup = builtin_name;
    const std::string& usage = "Usage: " + builtin_name + " <jobid> <index> | <pid>.";
    const size_t numArgs = numArguments(c);
    STSHProcess process;
    // Block here to ensure that process found by parseProcess remains valid until end of the execution block
    blockSignal(SIGCHLD);
    // Parse input
    if (numArgs == 1) {
        pid_t pid = parseNumber(c.tokens[0], usage);
        if (!joblist.containsProcess(pid)) throw STSHException("No process with pid " + std::to_string(pid) + ".");
        process = joblist.getJobWithProcess(pid).getProcess(pid);
    }
    else if (numArgs == 2) {
        size_t jobID = parseNumber(c.tokens[0], usage);
        size_t index = parseNumber(c.tokens[0], usage);
        if (!joblist.containsJob(jobID)) throw STSHException("No job with id of " + std::to_string(jobID) + ".");
        STSHJob& job = joblist.getJob(jobID);
        if (index >= job.getProcesses().size()) throw STSHException("Job " + to_string(jobID) + " doesn't have a process at index " + to_string(index));
        process = job.getProcesses()[index];
    }
    else throw STSHException(usage);
    // Take the appropriate action. 
    if (sig == SIGSTOP && process.getState() == kStopped) return;
    if (sig == SIGCONT && process.getState() == kRunning) return;
    kill_errchk(process.getID(), sig);
    unblockSignal(SIGCHLD);
}

/**
 * Function: handleBuiltin
 * -----------------------
 * Examines the leading command of the provided pipeline to see if
 * it's a shell builtin, and if so, handles and executes it.  handleBuiltin
 * returns true if the command is a builtin, and false otherwise.
 */
static bool handleBuiltin(const pipeline& pipeline) {
  const string& command = pipeline.commands[0].command;
  auto iter = find(kSupportedBuiltins, kSupportedBuiltins + kNumSupportedBuiltins, command);
  if (iter == kSupportedBuiltins + kNumSupportedBuiltins) return false;
  size_t index = iter - kSupportedBuiltins;

  switch (index) {
  case 0:
  case 1: exit(0); break;
  case 2: resume(pipeline.commands[0], "fg", true); break;
  case 3: resume(pipeline.commands[0], "bg", false); break;
  case 4: sendSignal(pipeline.commands[0], "slay", SIGKILL); break;
  case 5: sendSignal(pipeline.commands[0], "halt", SIGSTOP); break;
  case 6: sendSignal(pipeline.commands[0], "cont", SIGCONT); break;
  case 7: cout << joblist; break;
  default: throw STSHException("Internal Error: Builtin command not supported."); // or not implemented yet
  }
  
  return true;
}
/*
 * Function
 * ------------
 * A helper function to update global job list after reaping children
 */

static void updateJobListHelper(STSHJobList& jobList, pid_t pid, STSHProcessState    state) {
    if (!jobList.containsProcess(pid)) return;
    STSHJob& job = jobList.getJobWithProcess(pid);
    assert(job.containsProcess(pid));
    STSHProcess& process = job.getProcess(pid);
    process.setState(state);
    jobList.synchronize(job);
}
/*
 * Performs the accounting required to update the global job list after 
 * any child changes state
 */

static void updateJobList(int sig) {
    std::string debugGroup = "UpdateJL";
    int status;
    while (true) {
        pid_t pid = waitpid(-1, &status, WNOHANG | WCONTINUED | WUNTRACED);
        if (pid <= 0) {
            debugLog(INFO, debugGroup, std::string("Job list updated"));
            return;
        }
        if (WIFEXITED(status)) {
            debugLog(INFO, debugGroup, std::string("Process ") + std::to_string(pid) + " exited normally with status " + std::to_string(WEXITSTATUS(status)));
            updateJobListHelper(joblist, pid, kTerminated);
        } 
        else if (WIFSIGNALED(status)) {
            debugLog(INFO, debugGroup, std::string("Process ") + std::to_string(pid) + " was terminated by signal " + strsignal(WTERMSIG(status)));
            updateJobListHelper(joblist, pid, kTerminated);
        } 
        else if (WIFSTOPPED(status)) {
            debugLog(INFO, debugGroup, std::string("Process ") + std::to_string(pid) + " was stopped");
            updateJobListHelper(joblist, pid, kStopped);
        } 
        else if (WIFCONTINUED(status)) {
            debugLog(INFO, debugGroup, std::string("Process ") + std::to_string(pid) + "was continued");
            updateJobListHelper(joblist, pid, kRunning);
        }
        else {
            debugLog(ERROR, debugGroup, std::string("Process status unknown"));
        }
    }
}

/* If foreground job exists, skips the shell and sends the signal to the foreground job
 * If no foreground job, do nothing (like sample soln).
 * Ref: https://edstem.org/us/courses/3597/discussion/256244
 */
static void sendToForeground(int sig) {
    std::string debugGroup = "SendToFg";
    debugLog(INFO, debugGroup, std::string("Handling signal ") + strsignal(sig));
    if (joblist.hasForegroundJob()) {
        debugLog(DEBUG, debugGroup, std::string("Handling signal ") + strsignal(sig));
        pid_t job_pgid = joblist.getForegroundJob().getGroupID();
        killpg_errchk(job_pgid, sig);
    } 
    debugLog(INFO, debugGroup, "Signal handled");
}

/**
 * Function: installSignalHandlers
 * -------------------------------
 * Installs user-defined signals handlers for four signals
 * (once you've implemented signal handlers for SIGCHLD, 
 * SIGINT, and SIGTSTP, you'll add more installSignalHandler calls) and 
 * ignores two others.
 *
 * installSignalHandler is a wrapper around a more robust version of the
 * signal function we've been using all quarter.  Check out stsh-signal.cc
 * to see how it works.
 */
static void installSignalHandlers() {
    installSignalHandler(SIGQUIT, [](int sig) { exit(0); });
    installSignalHandler(SIGTTIN, SIG_IGN);
    installSignalHandler(SIGTTOU, SIG_IGN);
    installSignalHandler(SIGCHLD, updateJobList);
    installSignalHandler(SIGINT, sendToForeground);
    installSignalHandler(SIGTSTP, sendToForeground);
}

static std::string toString(const command& c) {
    stringstream strbuf;
    strbuf << '"';
    strbuf << c.command << " ";
    for (int i = 0; i < kMaxArguments + 1; i++) {
        if (c.tokens[i] == NULL) break;
        strbuf << c.tokens[i];
    }
    strbuf << '"';
    return strbuf.str();
}

/*
 * A wrapper around execvp that works with the command struct. 
 */
static int executeCommand(const command& c) {
    char * argv[kMaxArguments + 2];
    argv[0] = (char*) c.command;
    for (int i = 1; i < kMaxArguments+2; i++) {
        argv[i] = c.tokens[i-1];
        if (argv[i] == NULL) break;
    }
    return execvp_errchk(c.command, argv);
}

/**
 * Initialize a process for one command. 
 * Function expects that, besides (0,1,2,input_fd, output_fd), no other fds are open. 
 * Also expects that STDIN, STDOUT have not been overwritten by dup2 yet
 *
 * @param input_fd: An open file descriptor to read from. Provide -1 if no rewiring needed. 
 * @param output_fd: An open file descriptor to write to. Provide -1 if no rewiring needed.
 *
 */
static void startProcess(STSHJob& job, const command& c, int input_fd, int output_fd, bool inForeground = true) {
    std::string debugGroup = "StartProcess";
    debugLog(DEBUG, debugGroup, "Function called");
    stringstream strbuf;
    strbuf << "Job info: " << job;
    debugLog(DEBUG, debugGroup, strbuf.str());
    pid_t child = fork_errchk();
    if (child == 0) {
        unblockSignal(SIGCHLD); // Was blocked in parent
        debugGroup = "ChildProcess";
        if (input_fd == STDIN_FILENO && inForeground) {
            // Transfer control of terminal to job process group
            pid_t pid = getpid();
            pid_t pgid = job.getGroupID();
            // If this is the first process in the job, pgid will be 0 because memory isn't shared
            // In which case we know it should be pid instead
            // For all subsequent processes pgid will be correct
            if (pgid == 0) pgid = pid;
            debugLog(DEBUG, debugGroup, "Set pgid of process " + std::to_string(pid) + " to " + std::to_string(pgid));
            setpgid_errchk(pid, pgid);
            tcsetpgrp_errchk(STDIN_FILENO, pgid);
        }
        if (input_fd != STDIN_FILENO) {
            dup2_errchk(input_fd, STDIN_FILENO);
            close_errchk(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2_errchk(output_fd, STDOUT_FILENO);
            close_errchk(output_fd);
        }
        debugGroup = "ChildProcess";
        debugLog(DEBUG, debugGroup, "Executing command " +  toString(c));
        executeCommand(c);
        exit(0);
    }
    debugLog(INFO, debugGroup, "Started process " + std::to_string(child) + " executing command " + toString(c));
    job.addProcess(STSHProcess(child, c));
    setpgid_errchk(child, job.getGroupID());
}

/* Convenience functions for dealing with pipes in C
 */
typedef struct pipe_t {
    int fds[2];
    void open() {pipe2_errchk(fds, O_CLOEXEC);};
    int get_read_fd() const {return fds[0];};
    int get_write_fd() const {return fds[1];};
    void close_read() const {close_errchk(get_read_fd());};
    void close_write() const {close_errchk(get_write_fd());};
    void close() const {close_read(); close_write();};
} pipe_t;

/**
 * Function: createJob
 * -------------------
 * Creates a new job on behalf of the provided pipeline.
 */
static void createJob(const pipeline& p) {
    const std::string debugGroup = "CreateJob";
    STSHJob& job = joblist.addJob(p.background ? kBackground : kForeground);
    
    const size_t numProcesses = p.commands.size(); 
    const bool readFromStdin = (p.input.length() == 0);
    int fds[2];
    const int pipeline_input_fd = readFromStdin ? STDIN_FILENO : open_errchk(p.input.c_str(), O_RDONLY);
    blockSignal(SIGCHLD);
    pid_t orig_pgrp = -1;
    if (readFromStdin && !p.background) orig_pgrp = tcgetpgrp_errchk(pipeline_input_fd);
    int prev_input_fd = pipeline_input_fd;
    for (int i = 0; i < numProcesses; i ++ ) {
        int output_fd;
        if (i < numProcesses -1) {
            // If at least one more process in the pipeline, open a new pipe to write to that process
            pipe2_errchk(fds, O_CLOEXEC);
            output_fd = fds[1];
        } else {
            // Last process in pipeline, write to STDOUT or the file provided.
            output_fd = (p.output.length() == 0) ? STDOUT_FILENO : open_errchk(p.output.c_str(), O_WRONLY | O_CREAT | O_TRUNC); 
        }
        try {
            startProcess(job, p.commands[i], prev_input_fd, output_fd, !p.background);
        } catch (const STSHException& e) {
            debugLog(ERROR, debugGroup, e.what());
            // Clean up the open fds
            close_errchk(prev_input_fd);
            close_errchk(output_fd);
            // Throw exception that will be handled in main
            throw e;
        }
        if (prev_input_fd != STDIN_FILENO)close_errchk(prev_input_fd);
        if (output_fd != STDOUT_FILENO)close_errchk(output_fd);
        // Set the read end for the next process
        if (i < numProcesses -1) prev_input_fd = fds[0];
    }
    if (!p.background) {       
        if (!readFromStdin) waitForForeground();
        else {
            // Transfer control of terminal to job process group
            debugLog(DEBUG, debugGroup, "Caching original pgrp: " + std::to_string(orig_pgrp));
            debugLog(DEBUG, debugGroup, "Setting new pgrp to " + std::to_string(job.getGroupID()));
            tcsetpgrp_errchk(pipeline_input_fd, job.getGroupID());
            waitForForeground(); 
            tcsetpgrp_errchk(pipeline_input_fd, orig_pgrp);
        }
    }
    else {
        // Print some job info and return
        cout << "[" << job.getNum() << "]";
        for (const auto& p: job.getProcesses()) {
            cout << " " << p.getID(); 
        }
        cout << std::endl << std::flush;
    }
    unblockSignal(SIGCHLD);
}

/**
 * Function: main
 * --------------
 * Defines the entry point for a process running stsh.
 * The main function is little more than a read-eval-print
 * loop (i.e. a repl).  
 */
int main(int argc, char *argv[]) {
  std::string debugGroup = "Main";
  pid_t stshpid = getpid();
  installSignalHandlers();
  rlinit(argc, argv);
  while (true) {
    string line;
    if (!readline(line)) break;
    if (line.empty()) continue;
    try {
      pipeline p(line);
      bool builtin = handleBuiltin(p);
      if (!builtin) createJob(p);
    } catch (const STSHException& e) {
      cerr << e.what() << endl;
      if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
    }
  }

  return 0;
}
