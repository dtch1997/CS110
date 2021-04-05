/**
 * File: thread-pool.h
 * -------------------
 * Exports a ThreadPool abstraction, which manages a finite pool
 * of worker threads that collaboratively work through a sequence of tasks.
 * As each task is scheduled, the ThreadPool waits for at least
 * one worker thread to be free and then assigns that task to that worker.  
 * Threads are scheduled and served in a FIFO manner, and tasks need to
 * take the form of thunks, which are zero-argument thread routines.
 */

#ifndef _thread_pool_
#define _thread_pool_

#include <cstdlib>
#include <functional>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "semaphore.h"
// place additional #include statements here

namespace develop {

class ThreadPool {
 public:

/**
 * Constructs a ThreadPool configured to spawn up to the specified
 * number of threads.
 */
  ThreadPool(size_t numThreads);

/**
 * Destroys the ThreadPool class
 */
  ~ThreadPool();

/**
 * Schedules the provided thunk (which is something that can
 * be invoked as a zero-argument function without a return value)
 * to be executed by one of the ThreadPool's threads as soon as
 * all previously scheduled thunks have been handled.
 */
  void schedule(const std::function<void(void)>& thunk);

/**
 * Blocks and waits until all previously scheduled thunks
 * have been executed in full.
 */
  void wait();

 private:
  typedef std::function<void(void)> Work;
  struct Worker {
    semaphore requested;
    std::thread thread;
    Work work;
    bool available;
    bool stopWorking;

    Worker() : requested(0), available(false), stopWorking(false) {}
  };
  typedef struct Worker Worker;

  std::thread dispatcher;
  std::vector<Worker> workers;
  std::mutex workQueueMutex;
  std::queue<std::function<void(void)>> workQueue;
  semaphore availableWork;
  semaphore availableWorker;
  std::condition_variable_any allWorkFinished;
  std::atomic<bool> stopWorking;
  std::mutex numAvailableWorkersMutex;
  std::atomic<int> numAvailableWorkers;

  bool allWorkersAvailable();
  void markWorkerAsAvailable(size_t i);
  void markWorkerAsUnavailable(size_t i);
  int getAvailableWorker();
  const Work& popWork();
  void pushWork(const Work& work);

  ThreadPool(const ThreadPool& original) = delete;
  ThreadPool& operator=(const ThreadPool& rhs) = delete;
};

#endif

}
