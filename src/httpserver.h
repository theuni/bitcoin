// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HTTPSERVER_H
#define BITCOIN_HTTPSERVER_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include <netaddress.h>
#include <rpc/protocol.h>
#include <util/sock.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/time.h>
#include <util/translation.h>

namespace util {
class SignalInterrupt;
} // namespace util

/**
 * The default value for `-rpcthreads`. This number of threads will be created at startup.
 */
static const int DEFAULT_HTTP_THREADS=16;

/**
 * The default value for `-rpcworkqueue`. This is the maximum depth of the work queue,
 * we don't allocate this number of work queue items upfront.
 */
static const int DEFAULT_HTTP_WORKQUEUE=64;

static const int DEFAULT_HTTP_SERVER_TIMEOUT=30;

enum HTTPRequestMethod {
    UNKNOWN,
    GET,
    POST,
    HEAD,
    PUT
};

/** Event handler closure.
 */
class HTTPClosure
{
public:
    virtual void operator()() = 0;
    virtual ~HTTPClosure() = default;
};

namespace http_bitcoin {
using util::LineReader;
using NodeId = int64_t;

// shortest valid request line, used by libevent in evhttp_parse_request_line()
static const size_t MIN_REQUEST_LINE_LENGTH{strlen("GET / HTTP/1.0")};
// maximum size of http request (request line + headers)
// see https://github.com/bitcoin/bitcoin/issues/6425
static const size_t MAX_HEADERS_SIZE{8192};

class HTTPHeaders
{
public:
    std::optional<std::string> Find(const std::string key) const;
    void Write(const std::string key, const std::string value);
    void Remove(const std::string key);
    bool Read(util::LineReader& reader);
    std::string Stringify() const;

private:
    std::map<std::string, std::string, util::CaseInsensitiveComparator> m_map;
};

class HTTPResponse
{
public:
    int m_version_major;
    int m_version_minor;
    HTTPStatusCode m_status;
    std::string m_reason;
    HTTPHeaders m_headers;
    std::vector<std::byte> m_body;

    std::string StringifyHeaders() const;
};

class HTTPClient;

class HTTPRequest
{
public:
    std::string m_method;
    std::string m_target;
    // Default protocol version is used by error responses to unreadable requests
    int m_version_major{1};
    int m_version_minor{1};
    HTTPHeaders m_headers;
    std::string m_body;

    // Keep a pointer to the client that made the request so
    // we know who to respond to.
    std::shared_ptr<HTTPClient> m_client;
    explicit HTTPRequest(std::shared_ptr<HTTPClient> client) : m_client(client) {};
    // Null client for unit tests
    explicit HTTPRequest() : m_client(nullptr) {};

    // Readers return false if they need more data from the
    // socket to parse properly. They throw errors if
    // the data is invalid.
    bool LoadControlData(LineReader& reader);
    bool LoadHeaders(LineReader& reader);
    bool LoadBody(LineReader& reader);
    void SetKeepAlive();

    // These methods reimplement the API from http_libevent::HTTPRequest
    // for downstream JSONRPC and REST modules.
    std::string GetURI() const {return m_target;};
    CService GetPeer() const;
    HTTPRequestMethod GetRequestMethod() const;
    std::optional<std::string> GetQueryParameter(const std::string& key) const;
    std::pair<bool, std::string> GetHeader(const std::string& hdr) const;
    std::string ReadBody() const {return m_body;};
    void WriteHeader(const std::string& hdr, const std::string& value);
    bool CheckKeepAlive() const;
    // Response headers may be set in advance before response body is known
    HTTPHeaders m_response_headers;
    void WriteReply(HTTPStatusCode status, std::span<const std::byte> reply_body = {});
    void WriteReply(HTTPStatusCode status, const char* reply_body)
    {
        auto reply_body_view = std::string_view(reply_body);
        std::span<const std::byte> byte_span(reinterpret_cast<const std::byte*>(reply_body_view.data()), reply_body_view.size());
        WriteReply(status, byte_span);
    }
    void WriteReply(HTTPStatusCode status, const std::string& reply_body)
    {
        std::span<const std::byte> byte_span{reinterpret_cast<const std::byte*>(reply_body.data()), reply_body.size()};
        WriteReply(status, byte_span);
    }
};

std::optional<std::string> GetQueryParameterFromUri(const std::string& uri, const std::string& key);

class HTTPServer;

class HTTPClient : public std::enable_shared_from_this<HTTPClient>
{
    ssize_t SendBytes(std::span<const unsigned char> data, std::string& errmsg) const;

public:
    NodeId m_node_id;
    std::shared_ptr<Sock> m_sock;
    // Remote address of connected client
    CService m_addr;
    // IP:port of connected client, cached for logging purposes
    std::string m_origin;
    // Pointer back to the server so we can call Server I/O methods from the client
    // Ok to remain null for unit tests.
    HTTPServer* m_server;

    // In lieu of an intermediate transport class like p2p uses,
    // we copy data from the socket buffer to the client object
    // and attempt to read HTTP requests from here.
    std::vector<std::byte> m_recv_buffer{};

    // Response data destined for this client.
    // Written to directly by http worker threads
    Mutex m_send_mutex;
    std::vector<std::byte> m_send_buffer GUARDED_BY(m_send_mutex);
    // Set true by worker threads after writing a response to m_send_buffer.
    std::atomic_bool m_send_ready{false};

    std::atomic_bool m_done{false};
    bool m_prevent_disconnect = false;
    // Flag this client for disconnection on next loop
    bool m_disconnect{false};
    std::atomic<int> m_refcount{0};

    // Timestamp of last receive activity, used for -rpcservertimeout
    SteadySeconds m_idle_since;

    explicit HTTPClient(NodeId node_id, CService addr) : m_node_id(node_id), m_addr(addr)
    {
        m_origin = addr.ToStringAddrPort();
    };

    // Try to read an HTTP request from the receive buffer
    bool ReadRequest(std::unique_ptr<HTTPRequest>& req);

    // Push data from m_send_buffer to the connected socket via m_server
    std::pair<ssize_t, bool> SendBytesFromBuffer() EXCLUSIVE_LOCKS_REQUIRED(!m_send_mutex);
    void CloseSocketDisconnect();
    void ReceiveMsgBytes(std::span<const uint8_t> data);

    // Disable copies (should only be used as shared pointers)
    HTTPClient(const HTTPClient&) = delete;
    HTTPClient& operator=(const HTTPClient&) = delete;
};

class HTTPServer
{
private:
    void CloseConnectionInternal(std::shared_ptr<HTTPClient>& client);
    void SocketHandler();
    void SocketHandlerConnected(const std::vector<std::shared_ptr<HTTPClient>>& nodes, const Sock::EventsPerSock& events_per_sock);
    void SocketHandlerListening(const Sock::EventsPerSock& events_per_sock);
    NodeId GetNewId();
    void AcceptConnection(const Sock& listen_socket);
    // Close underlying connections where flagged
    void DisconnectClients();


    std::thread m_thread_socket_handler;
    std::vector<std::shared_ptr<Sock>> m_listen;
    NodeId m_next_id{0};

    std::shared_ptr<Sock> m_wake_send;
    std::shared_ptr<Sock> m_wake_recv;

    //! Connected clients with live HTTP connections
    std::vector<std::shared_ptr<HTTPClient>> m_connected_clients;
    CThreadInterrupt m_listen_interrupt;
    std::atomic_bool m_disconnect_all_clients{false};
    void JoinSocketsThreads();

public:
    explicit HTTPServer(std::function<void(std::unique_ptr<HTTPRequest>)> func);
    std::atomic_bool m_no_clients{true};

    // What to do with HTTP requests once received, validated and parsed
    std::function<void(std::unique_ptr<HTTPRequest>)> m_request_dispatcher;

    // Idle timeout after which clients are disconnected
    std::chrono::seconds m_rpcservertimeout{DEFAULT_HTTP_SERVER_TIMEOUT};

    void StartSocketsThreads();
    bool BindAndStartListening(const CService& addrBind, bilingual_str& strError);
    void Wake();
    void Interrupt();
    void Stop();
};

/** Initialize HTTP server.
 * Call this before RegisterHTTPHandler or EventBase().
 */
bool InitHTTPServer(const util::SignalInterrupt& interrupt);
/** Start HTTP server.
 * This is separate from InitHTTPServer to give users race-condition-free time
 * to register their handlers between InitHTTPServer and StartHTTPServer.
 */
void StartHTTPServer();
/** Interrupt HTTP server threads */
void InterruptHTTPServer();
/** Stop HTTP server */
void StopHTTPServer();
} // namespace http_bitcoin

/** Handler for requests to a certain HTTP path */
typedef std::function<bool(http_bitcoin::HTTPRequest* req, const std::string&)> HTTPRequestHandler;
/** Register handler for prefix.
 * If multiple handlers match a prefix, the first-registered one will
 * be invoked.
 */
void RegisterHTTPHandler(const std::string &prefix, bool exactMatch, const HTTPRequestHandler &handler);
/** Unregister handler for prefix */
void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch);

#endif // BITCOIN_HTTPSERVER_H
