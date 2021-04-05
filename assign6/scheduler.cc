/**
 * File: scheduler.cc
 * ------------------
 * Presents the implementation of the HTTPProxyScheduler class.
 */

#include "scheduler.h"
#include <utility>
using namespace std;

const int kNumThreads = 64;

HTTPProxyScheduler::HTTPProxyScheduler() : pool(kNumThreads) {}

void HTTPProxyScheduler::scheduleRequest(int clientfd, const string& clientIPAddress) throw () {
    pool.schedule([this, clientfd, &clientIPAddress]() {
        requestHandler.serviceRequest(make_pair(clientfd, clientIPAddress));
    });
}

void HTTPProxyScheduler::flagForwardToProxy(bool flag) { requestHandler.flagForwardToProxy(flag); }
