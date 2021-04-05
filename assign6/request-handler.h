/**
 * File: request-handler.h
 * -----------------------
 * Defines the HTTPRequestHandler class, which fully proxies and
 * services a single client request.  
 */

#ifndef _request_handler_
#define _request_handler_

#include <utility>
#include <string>
#include <map>
#include <mutex>
#include "strikeset.h"
#include "cache.h"
#include "request.h"

class HTTPRequestHandler {
 public:
  HTTPRequestHandler();
  void serviceRequest(const std::pair<int, std::string>& connection) throw();
  void clearCache();
  void setCacheMaxAge(long maxAge);
  void flagForwardToProxy(bool flag) {forwardToProxy = flag;}

 private:
  HTTPCache cache;
  HTTPStrikeSet strikeset;
  mutable std::vector<std::mutex> requestMutexes;
  bool forwardToProxy;
  
  typedef void (HTTPRequestHandler::*handlerMethod)(HTTPRequest& request, class iosockstream& ss);
  std::map<std::string, handlerMethod> handlers;

  void handleRequest(HTTPRequest& request, class iosockstream& ss);
  void handleCONNECTRequest(HTTPRequest& request, class iosockstream& ss);
  int setupClientSocket(const HTTPRequest& request) const;
  void forwardRequestAndGetResponse(HTTPRequest& request, HTTPResponse& response) const;
  void manageClientServerBridge(iosockstream& client, iosockstream& server);
  std::string buildTunnelString(iosockstream& from, iosockstream& to) const;

  void handleBadRequestError(class iosockstream& ss, const std::string& message) const;
  void handleUnsupportedMethodError(class iosockstream& ss, const std::string& message) const;
  void handleError(class iosockstream& ss, const std::string& protocol,
                   int responseCode, const std::string& message) const;

  static bool containsProxyLoop(const HTTPRequest& request);
  bool connectingToServer() const;

  size_t hashRequest(const HTTPRequest& request) const;

};

#endif
