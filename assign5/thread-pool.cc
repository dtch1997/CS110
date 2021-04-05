/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
#include <thread>
#include <cassert>
#include <iostream>
#include <sstream>
#include "ostreamlock.h"
using namespace std;
using develop::ThreadPool;


enum LoggingLevel {DEBUG=0, INFO=1, WARNING=2, ERROR=3, SILENT=4};
const static char * kLoggingLevelNames[] = {"DBG", "INFO", "WARN", "ERR", "SIL"};
// Set the global logging level to SILENT for submission
const static LoggingLevel kGlobalLoggingLevel = SILENT;

void debugLog(LoggingLevel level,
        const std::string& group,
        const std::string& message) {
    if (level >= kGlobalLoggingLevel) {
        std::stringstream strbuf;
        strbuf << "[" << std::string(kLoggingLevelNames[level]) << "]\t[" << group << "] \t" << message  << std::endl;
        std::cerr << oslock << strbuf.str() << std::flush << osunlock;
    }
}

ThreadPool::ThreadPool(size_t numThreads) : 
    workers(numThreads), 
    availableWork(0),
    availableWorker(0),
    stopWorking(false),
    numAvailableWorkers(0)
{
    /* dispatcher thread
     * 
     * In this implementation, dispatcher is responsible for:
     *   - initializing workers
     *   - signalling workers to stop
     *   - waiting for all workers to terminate
     */
    dispatcher = std::thread([this]() {
        // Initialize worker threads
        for (size_t i = 0; i < workers.size(); i ++) {
            workers[i].thread = std::thread([this, i]() {
                while(true) {
                    markWorkerAsAvailable(i);   
                    workers[i].requested.wait(); 
                    if (workers[i].stopWorking) break;
                    workers[i].work();
                }
            });
        }

        // Main dispatcher loop
        while(true) {
            availableWork.wait(); 
            availableWorker.wait();
            if (stopWorking) break;
            int i = getAvailableWorker();
            assert(i >= 0);
            // Assign unavailable before popping work to prevent race condition
            markWorkerAsUnavailable(i);
            workers[i].work = popWork();
            workers[i].requested.signal();
        }

        // We only reach here when destructor has been called
        // Signal all the worker threads to stop
        for (size_t i = 0; i < workers.size(); i++) {
            workers[i].stopWorking = true;
            workers[i].requested.signal();
        }

        // Clean up worker threads before returning
        for (size_t i = 0; i < workers.size(); i++) {
            workers[i].thread.join();
        }

    });

}

ThreadPool::~ThreadPool() {
    wait();
    stopWorking = true;
    availableWork.signal();
    availableWorker.signal();
    dispatcher.join();
}

void ThreadPool::schedule(const std::function<void(void)>& thunk) {
    assert(!stopWorking);
    pushWork(thunk);
    debugLog(DEBUG, "Sched", "Signalling availableWork");
    availableWork.signal();
}

void ThreadPool::wait() {
    std::lock_guard<std::mutex> latch(workQueueMutex);
    if (workQueue.empty() && allWorkersAvailable()) return;
    allWorkFinished.wait(workQueueMutex, [this]() { 
        bool pred = workQueue.empty() && allWorkersAvailable();
        return pred;
    });
}

int ThreadPool::getAvailableWorker() {
    for (size_t i = 0; i < workers.size(); i++) {
        if (workers[i].available) return i;
    }
    //error
    return -1;
}

bool ThreadPool::allWorkersAvailable() {
    std::lock_guard<std::mutex> latch(numAvailableWorkersMutex);
    return numAvailableWorkers == (int) workers.size();
}

void ThreadPool::markWorkerAsUnavailable(size_t i) {
    assert(workers[i].available);
    assert(numAvailableWorkers.load() > 0);
    workers[i].available = false;
    std::lock_guard<std::mutex> latch(numAvailableWorkersMutex);
    numAvailableWorkers--;
}

void ThreadPool::markWorkerAsAvailable(size_t i) {
    assert(!workers[i].available);
    workers[i].available = true;
    std::lock_guard<std::mutex> latch(numAvailableWorkersMutex);
    numAvailableWorkers++;
    availableWorker.signal();
    // There could be multiple threads that call wait()
    allWorkFinished.notify_all();
}


/* Function: popWork
 * ----------------------------------
 * Atomically pop work from the queue
 */
const std::function<void(void)>& ThreadPool::popWork() {
    workQueueMutex.lock();
    const auto& work = workQueue.front();
    workQueue.pop();
    workQueueMutex.unlock();
    return work;
}

/* Function: pushWork
 * ----------------------------------
 * Atomically push work to the queue
 */
void ThreadPool::pushWork(const Work& work) {
    workQueueMutex.lock();
    workQueue.push(work);
    workQueueMutex.unlock();
}

