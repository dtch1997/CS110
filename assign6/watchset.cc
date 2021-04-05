/**
 * File: watchset.cc
 * -----------------
 * Implements the ProxyWatchset class as defined in watchset.h.
 */

#include "watchset.h"
#include <sys/epoll.h>
#include <unistd.h>

ProxyWatchset::ProxyWatchset(size_t timeout) {
  this->timeout = timeout;
  watchset = epoll_create1(0);
}

ProxyWatchset::~ProxyWatchset() {
  close(watchset);
}

void ProxyWatchset::add(int fd) {
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = fd;
  epoll_ctl(watchset, EPOLL_CTL_ADD, fd, &event);
}

void ProxyWatchset::remove(int fd) {
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = fd;
  epoll_ctl(watchset, EPOLL_CTL_DEL, fd, &event);
}

int ProxyWatchset::wait() {
  struct epoll_event event;
  int numEvents = epoll_wait(watchset, &event, 1, /* timeout_in_ms = */ timeout * 1000);
  if (numEvents <= 0) return -1;
  return event.data.fd;
}
