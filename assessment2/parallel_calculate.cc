#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

using namespace std;

typedef struct someData {
    int data;
} someData;

int calculate(someData data) {
    this_thread::sleep_for(chrono::seconds(data.data));
    return data.data;
}

void getData(queue<someData> &dataQueue, int amount) {
    for (int i =0; i < amount; i ++ ) {
        someData data;
        data.data = rand() % 10;
        dataQueue.push(data);
    }
}

const int kNumThreads = 8;
const int kDataAmount = 16;

void parallelCalculate(queue<someData>& dataQueue, 
    queue<int>& printQueue,
    mutex& dataQueueMutex, 
    mutex& printQueueMutex,
    condition_variable_any& coordinator,
    int workerIndex) 
{
    while (true) {
        // TODO: complete this function
        dataQueueMutex.lock();
        if (dataQueue.empty()) {
            dataQueueMutex.unlock();
            return;
        }
        // Register worker for printing
        printQueueMutex.lock();
        printQueue.push(workerIndex);
        printQueueMutex.unlock();
        // Get the data to do calculation
        someData data = dataQueue.front();
        dataQueue.pop();
        dataQueueMutex.unlock();
        
        int result = calculate(data);
        
        printQueueMutex.lock();
        if (!(printQueue.front() == workerIndex)) {
            coordinator.wait(printQueueMutex, [&printQueue, workerIndex]() {
                return printQueue.front() == workerIndex;
            });
        }
        
        cout << result << endl;
        printQueue.pop();
        coordinator.notify_all();
        printQueueMutex.unlock();
    }
}

int main() {
    queue<someData> dataQueue;
    getData(dataQueue, kDataAmount);
    vector<thread> threads;
    
    // TODO: add your shared variables here
    mutex dataQueueMutex;
    queue<int> printQueue;
    mutex printQueueMutex;
    condition_variable_any coordinator;

    for (int i = 0; i < kNumThreads; i++) {
        threads.push_back(thread(
            parallelCalculate,
            std::ref(dataQueue), 
            std::ref(printQueue),
            std::ref(dataQueueMutex), 
            std::ref(printQueueMutex),
            std::ref(coordinator),
            i
        ));
    }

    // TODO: cleanup
    for (int i = 0; i < kNumThreads; i ++ ) {
        threads[i].join();
    }
    
    return 0;
}
