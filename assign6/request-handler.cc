/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
 */

#include "request-handler.h"
#include "response.h"
#include "client-socket.h"
#include "ostreamlock.h"
#include <socket++/sockstream.h> // for sockbuf, iosockstream
#include "watchset.h"
using namespace std;

static const int kNumMutexes = 997;
static const string kDefaultProtocol = "HTTP/1.0";
static const string kForwardedForKey = "x-forwarded-for";
static const string kIpDelimiter = ", ";

HTTPRequestHandler::HTTPRequestHandler() : requestMutexes(kNumMutexes), forwardToProxy(false) {
  handlers["GET"] = &HTTPRequestHandler::handleRequest;
  handlers["POST"] = &HTTPRequestHandler::handleRequest;
  handlers["HEAD"] = &HTTPRequestHandler::handleRequest;
  handlers["CONNECT"] = &HTTPRequestHandler::handleCONNECTRequest;

  strikeset.addToStrikeSet("blocked-domains.txt");
}

void HTTPRequestHandler::serviceRequest(const pair<int, string>& connection) throw() {
    sockbuf sb(connection.first);
    iosockstream ss(&sb);
    try {
        HTTPRequest request;
        request.ingestRequestLine(ss);
        request.ingestHeader(ss, connection.second);
        request.ingestPayload(ss);

        size_t index = hashRequest(request);
        std::lock_guard<std::mutex> guard(requestMutexes[index]);

        if (!strikeset.serverIsAllowed(request.getServer())) {
            handleError(ss, kDefaultProtocol, 403, "Forbidden Content");
            return;
        }

        if (containsProxyLoop(request)) {
            handleError(ss, kDefaultProtocol, 504, "Circular proxy chain detected");
            return;
        }

        auto found = handlers.find(request.getMethod());
        if (found == handlers.cend()) throw UnsupportedMethodExeption(request.getMethod());
        (this->*(found->second))(request, ss);
    } catch (const HTTPBadRequestException &bre) {
        handleBadRequestError(ss, bre.what());
    } catch (const UnsupportedMethodExeption& ume) {
        handleUnsupportedMethodError(ss, ume.what());
    } catch (...) {}
}


static void addHeaders(HTTPRequest& request) {
    request.addHeader("x-forwarded-proto", "http");
    if (request.containsName(kForwardedForKey)) {
        const std::string& previousIPs = request.getHeader().getValueAsString(kForwardedForKey);
        const std::string& currentIPs = previousIPs + ", " + request.getClientIPAddress();
        request.addHeader(kForwardedForKey, currentIPs);
    } else {
        request.addHeader(kForwardedForKey, request.getClientIPAddress());
    }
}

bool HTTPRequestHandler::containsProxyLoop(const HTTPRequest& request) {    
    if (!request.containsName(kForwardedForKey)) return false;
    std::string ipstr = request.getHeader().getValueAsString(kForwardedForKey);
    size_t pos = 0;
    while ((pos = ipstr.find(kIpDelimiter)) != std::string::npos) {
        std::string pastIP = ipstr.substr(0, pos);
        if (request.getClientIPAddress() == pastIP) return true;  
        ipstr.erase(0, pos + kIpDelimiter.length());
    }
    return false;
}

bool HTTPRequestHandler::connectingToServer() const {return true;}

static void forwardRequest(const HTTPRequest& request, class iosockstream& ss, bool toServer) {
    request.forward(ss, toServer);
}

static void sendResponse(const HTTPResponse& response, class iosockstream& ss) {
    ss << response;
    ss.flush();
}

static void getResponse(HTTPResponse& response, class iosockstream& ss, bool ignorePayload) { 
    response.ingestResponseHeader(ss);
    if (!ignorePayload) response.ingestPayload(ss);
}

int HTTPRequestHandler::setupClientSocket(const HTTPRequest& request) const {
    cout << oslock << "Creating client socket" << endl << osunlock;
    int client = createClientSocket(request.getServer(), request.getPort());
    if (client == kClientSocketError) {
        const std::string destinationType = forwardToProxy ? "proxy" : "server";
        throw HTTPServerException("Could not connect to " + destinationType + " " + request.getServer());
    }
    return client;
}

void HTTPRequestHandler::forwardRequestAndGetResponse(HTTPRequest& request, HTTPResponse& response) const {
    try {
        sockbuf sb(setupClientSocket(request)) ;
        iosockstream ss(&sb);
        cout << oslock << "Modifying request headers" << endl << osunlock;
        addHeaders(request);
        cout << oslock << "Sending " << request.getMethod() << " request to " << request.getURL() << endl << osunlock;
        forwardRequest(request, ss, !forwardToProxy);
        cout << oslock << "Waiting for response from " << request.getURL() << endl << osunlock;
        getResponse(response, ss, request.getMethod() == "HEAD");
    } catch(const HTTPServerException& e) {
        throw(e);
    }
}

void HTTPRequestHandler::handleRequest(HTTPRequest& request, class iosockstream& ss) {
    cout << oslock << "Handling " << request.getMethod() << " request" << endl << osunlock;
    HTTPResponse response;
    if (cache.containsCacheEntry(request, response)) {
        sendResponse(response, ss);
        return;
    }
    try {
        forwardRequestAndGetResponse(request, response);
    } catch(const HTTPServerException& e) {
        handleError(ss, kDefaultProtocol, 504, e.what());
    }
    
    if (cache.shouldCache(request, response)) cache.cacheEntry(request, response);
    cout << oslock << "Sending response to client" << endl << osunlock;
    sendResponse(response, ss);
    
}

void HTTPRequestHandler::handleCONNECTRequest(HTTPRequest& request, class iosockstream& clientStream) {
    cout << oslock << "Handling CONNECT request" << endl << osunlock;
    sockbuf sb(setupClientSocket(request));
    iosockstream serverStream(&sb);
    handleError(clientStream, kDefaultProtocol, 200, "OK");
    manageClientServerBridge(clientStream, serverStream);
}
/**
 * Used when handling CONNECT request (for HTTPS sites)
 */

void HTTPRequestHandler::manageClientServerBridge(iosockstream& client, iosockstream& server) {
  ProxyWatchset watchset;
  int clientfd = client.rdbuf()->sd();
  int serverfd = server.rdbuf()->sd();
  watchset.add(clientfd);
  watchset.add(serverfd);
  map<int, pair<iosockstream *, iosockstream *>> streams;
  streams[clientfd] = make_pair(&client, &server);
  streams[serverfd] = make_pair(&server, &client);
  //cout << oslock << buildTunnelString(client, server) << "Establishing HTTPS tunnel" << endl << osunlock;
  while (!streams.empty()) {
    int fd = watchset.wait();
    if (fd == -1) break;
    iosockstream& from = *streams[fd].first;
    iosockstream& to = *streams[fd].second;
    char buffer[1 << 8];
    from.read(buffer, 1);
    if (from.eof() || from.fail() || from.gcount() == 0) { watchset.remove(fd); streams.erase(fd); break; }
    to.write(buffer, 1);
    //cout << oslock << buildTunnelString(from, to) << "Read first byte." << endl << osunlock;
    while (true) {
      size_t numBytesRead = from.readsome(buffer, sizeof(buffer));
      if (from.eof() || from.fail()) { watchset.remove(fd); streams.erase(fd); break; };
      if (numBytesRead == 0) break;
      //cout << oslock << buildTunnelString(from, to) << "Read " << numBytesRead << " more bytes." << endl << osunlock;
      to.write(buffer, numBytesRead);
    }
    to.flush();
  }
  //cout << oslock << buildTunnelString(client, server) << "Tearing down HTTPS tunnel." << endl << osunlock;
}

string HTTPRequestHandler::buildTunnelString(iosockstream& from, iosockstream& to) const {
  return "[" + to_string(from.rdbuf()->sd()) + " --> " + to_string(to.rdbuf()->sd()) + "]: ";
}

/**
 * Responds to the client with code 400 and the supplied message.
 */
void HTTPRequestHandler::handleBadRequestError(iosockstream& ss, const string& message) const {
  handleError(ss, kDefaultProtocol, 400, message);
}

/**
 * Responds to the client with code 405 and the provided message.
 */
void HTTPRequestHandler::handleUnsupportedMethodError(iosockstream& ss, const string& message) const {
  handleError(ss, kDefaultProtocol, 405, message);
}

/**
 * Generic error handler used when our proxy server
 * needs to invent a response because of some error.
 */
void HTTPRequestHandler::handleError(iosockstream& ss, const string& protocol,
				     int responseCode, const string& message) const {
    HTTPResponse response;
    response.setProtocol(protocol);
    response.setResponseCode(responseCode);
    response.setPayload(message);
    ss << response << flush;
}

void HTTPRequestHandler::clearCache() {
    cache.clear();    
}

void HTTPRequestHandler::setCacheMaxAge(long maxAge) {
    cache.setMaxAge(maxAge);
}

size_t HTTPRequestHandler::hashRequest(const HTTPRequest& request) const {
    return cache.hashRequest(request) % requestMutexes.size();
}

