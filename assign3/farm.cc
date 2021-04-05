#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include <sstream>
#include "subprocess.h"
#include "fork-utils.h"  // this has to be the last #include'd statement in the file

using namespace std;

struct worker {
  worker() {}
  // This is defining a constructor for worker using something called an initialization list.
  worker(const char *argv[]) : sp(subprocess(const_cast<char **>(argv), true, false)), available(false) {}
  subprocess_t sp;
  bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

static void blockSIGCHLD() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

static void unblockSIGCHLD() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

// Global variable to turn debugging on or off
const static bool DEBUG = false;
// Logging function. Logging should occur atomically
static void debugLog(const std::string& group, const std::string& message) {
    if (DEBUG) {
        std::stringstream stream;
        stream << "[" << group << "]" << " " << message << endl;
        cerr << stream.str() << flush;
    }
}

static void markWorkersAsAvailable(int sig) {
    std::string debugGroup = "MARK_AVAIL";
    // No need to block SIGCHLD manually as OS already does that for signal handlers
    pid_t pid;
    // While loop to resolve multiple signals at once
    int status;
    while (true) {
        pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
        if (pid <= 0) return;
        // Look for the worker with this pid 
        bool workerFound = false;
        for (auto & worker : workers) {
            if (worker.sp.pid != pid) continue;
            // Now pid matches worker.pid
            if (WIFSTOPPED(status)) {
                // Prepare worker for new input
                debugLog(debugGroup, "Marking worker " + std::to_string(pid) + " as available");
                assert(!worker.available);
                assert(numWorkersAvailable < workers.size());
                worker.available = true;
                numWorkersAvailable++;
                debugLog(debugGroup, std::to_string(numWorkersAvailable) + " workers available.");
            } else if (WIFEXITED(status)){
                // Clean up program
                debugLog(debugGroup, "Terminating worker " + std::to_string(pid));
            }
            workerFound = true;
            break;
        }
        assert(workerFound);
    }
}

static const char *kWorkerArguments[] = {"./factor.py", "--self-halting", NULL};

static void spawnAllWorkers() {
    cout << "There are this many CPUs: " << kNumCPUs << ", numbered 0 through " << kNumCPUs - 1 << "." << endl;
    for (size_t i = 0; i < kNumCPUs; i++) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        workers[i] = worker(kWorkerArguments);
        sched_setaffinity(workers[i].sp.pid, sizeof(cpu_set_t), &cpuset);
        cout << "Worker " << workers[i].sp.pid << " is set to run on CPU " << i << "." << endl;
    }
}

static size_t getAvailableWorker() {
    std::string debugGroup = "GET_AVAIL";
    sigset_t set;
    sigemptyset(&set);
    // Idle until a worker becomes available 

    blockSIGCHLD();
    while (numWorkersAvailable == 0) {
        // Race condition: If all the signal handlers terminate here, then sigsuspend will never wake up.
        sigsuspend(&set);
    }
    unblockSIGCHLD();

    // Now at least one worker is available
    debugLog(debugGroup, "Available worker found.");
    for (size_t i = 0; i < workers.size(); i++) {
        if (workers[i].available) {
            return i;     
        }
    }
    // Should never reach here
    assert(false);
}

static void writeStringToFileDescriptor(int fd, const std::string& str) {
    // Writes a C++ string to a file descriptor
    size_t bytes_written = 0;
    while (bytes_written < str.length()) {
        bytes_written += write(fd, str.c_str() + bytes_written, str.length() - bytes_written);
    }
}

static void assignWorkerToTask(size_t index) {
    blockSIGCHLD();
    assert(workers[index].available);
    assert(numWorkersAvailable > 0);
    workers[index].available = false;
    numWorkersAvailable -= 1;
    unblockSIGCHLD();
}

static void broadcastNumbersToWorkers() {
    std::string debugGroup = "BROADCAST";
    while (true) {
        string line;
        getline(cin, line);
        if (cin.fail()) break;
        size_t endpos;
        /* long long num = */ stoll(line, &endpos);
        if (endpos != line.size()) break;
        debugLog(debugGroup, "Received valid input " + line);
        size_t index = getAvailableWorker();
        assert (0 <= index && index < workers.size());    
        debugLog(debugGroup, "Assigning worker " + std::to_string(workers[index].sp.pid) + " to task");
        assignWorkerToTask(index);
        writeStringToFileDescriptor(workers[index].sp.supplyfd, line + "\n");
        kill(workers[index].sp.pid, SIGCONT);
    }
}

static void waitForAllWorkers() {
    std::string debugGroup = "WAITALL";
    sigset_t set;
    sigemptyset(&set);
    blockSIGCHLD();
    while (numWorkersAvailable < workers.size()) {
        debugLog(debugGroup, std::to_string(numWorkersAvailable) + " of " + std::to_string(workers.size()) + " workers currently available");
        sigsuspend(&set);
    }
    unblockSIGCHLD();
    return;
}

static void closeAllWorkers() {
    // Send EOF to each subprocess by closing the write-end of the pipe
    for (auto worker: workers) {
        close(worker.sp.supplyfd);
        kill(worker.sp.pid, SIGCONT);
    }
}

int main(int argc, char *argv[]) {
  signal(SIGCHLD, markWorkersAsAvailable);
  std::string debugGroup = "MAIN";
  debugLog(debugGroup, "Spawning all workers");
  spawnAllWorkers();
  debugLog(debugGroup, "Broadcasting numbers to workers");
  broadcastNumbersToWorkers();
  debugLog(debugGroup, "Waiting for all workers to finish");
  waitForAllWorkers();
  debugLog(debugGroup, "Closing all workers");
  closeAllWorkers();
  signal(SIGCHLD, SIG_DFL);
  debugLog(debugGroup, "Program finished");
  return 0;
}
