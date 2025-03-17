// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <httpserver.h>

#include <chainparamsbase.h>
#include <clientversion.h>
#include <common/args.h>
#include <common/messages.h>
#include <common/url.h>
#include <compat/compat.h>
#include <logging.h>
#include <netbase.h>
#include <node/interface_ui.h>
#include <rpc/protocol.h> // For HTTP status codes
#include <span.h>
#include <sync.h>
#include <util/check.h>
#include <util/signalinterrupt.h>
#include <util/strencodings.h>
#include <util/thread.h>
#include <util/threadnames.h>
#include <util/time.h>
#include <util/translation.h>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include <sys/types.h>
#include <sys/stat.h>

static constexpr auto SELECT_TIMEOUT{50ms};

using common::InvalidPortErrMsg;
using http_bitcoin::HTTPRequest;

/** HTTP request work item */
class HTTPWorkItem final : public HTTPClosure
{
public:
    HTTPWorkItem(std::unique_ptr<HTTPRequest> _req, const std::string &_path, const HTTPRequestHandler& _func):
        req(std::move(_req)), path(_path), func(_func)
    {
    }
    void operator()() override
    {
        func(req.get(), path);
    }

    std::unique_ptr<HTTPRequest> req;

private:
    std::string path;
    HTTPRequestHandler func;
};

/** Simple work queue for distributing work over multiple threads.
 * Work items are simply callable objects.
 */
template <typename WorkItem>
class WorkQueue
{
private:
    Mutex cs;
    std::condition_variable cond GUARDED_BY(cs);
    std::deque<std::unique_ptr<WorkItem>> queue GUARDED_BY(cs);
    bool running GUARDED_BY(cs){true};
    const size_t maxDepth;

public:
    explicit WorkQueue(size_t _maxDepth) : maxDepth(_maxDepth)
    {
    }
    /** Precondition: worker threads have all stopped (they have been joined).
     */
    ~WorkQueue() = default;
    /** Enqueue a work item */
    bool Enqueue(WorkItem* item) EXCLUSIVE_LOCKS_REQUIRED(!cs)
    {
        LOCK(cs);
        if (!running || queue.size() >= maxDepth) {
            return false;
        }
        queue.emplace_back(std::unique_ptr<WorkItem>(item));
        cond.notify_one();
        return true;
    }
    /** Thread function */
    void Run() EXCLUSIVE_LOCKS_REQUIRED(!cs)
    {
        while (true) {
            std::unique_ptr<WorkItem> i;
            {
                WAIT_LOCK(cs, lock);
                while (running && queue.empty())
                    cond.wait(lock);
                if (!running && queue.empty())
                    break;
                i = std::move(queue.front());
                queue.pop_front();
            }
            (*i)();
        }
    }
    /** Interrupt and exit loops */
    void Interrupt() EXCLUSIVE_LOCKS_REQUIRED(!cs)
    {
        LOCK(cs);
        running = false;
        cond.notify_all();
    }
};

struct HTTPPathHandler
{
    HTTPPathHandler(std::string _prefix, bool _exactMatch, HTTPRequestHandler _handler):
        prefix(_prefix), exactMatch(_exactMatch), handler(_handler)
    {
    }
    std::string prefix;
    bool exactMatch;
    HTTPRequestHandler handler;
};

/** HTTP module state */

static std::unique_ptr<http_bitcoin::HTTPServer> g_http_server{nullptr};
//! List of subnets to allow RPC connections from
static std::vector<CSubNet> rpc_allow_subnets;
//! Work queue for handling longer requests off the event loop thread
static std::unique_ptr<WorkQueue<HTTPClosure>> g_work_queue{nullptr};
//! Handlers for (sub)paths
static GlobalMutex g_httppathhandlers_mutex;
static std::vector<HTTPPathHandler> pathHandlers GUARDED_BY(g_httppathhandlers_mutex);

/** Check if a network address is allowed to access the HTTP server */
static bool ClientAllowed(const CNetAddr& netaddr)
{
    if (!netaddr.IsValid())
        return false;
    for(const CSubNet& subnet : rpc_allow_subnets)
        if (subnet.Match(netaddr))
            return true;
    return false;
}

/** Initialize ACL list for HTTP server */
static bool InitHTTPAllowList()
{
    rpc_allow_subnets.clear();
    rpc_allow_subnets.emplace_back(LookupHost("127.0.0.1", false).value(), 8);  // always allow IPv4 local subnet
    rpc_allow_subnets.emplace_back(LookupHost("::1", false).value());  // always allow IPv6 localhost
    for (const std::string& strAllow : gArgs.GetArgs("-rpcallowip")) {
        const CSubNet subnet{LookupSubNet(strAllow)};
        if (!subnet.IsValid()) {
            uiInterface.ThreadSafeMessageBox(
                Untranslated(strprintf("Invalid -rpcallowip subnet specification: %s. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).", strAllow)),
                "", CClientUIInterface::MSG_ERROR);
            return false;
        }
        rpc_allow_subnets.push_back(subnet);
    }
    std::string strAllowed;
    for (const CSubNet& subnet : rpc_allow_subnets)
        strAllowed += subnet.ToString() + " ";
    LogDebug(BCLog::HTTP, "Allowing HTTP connections from: %s\n", strAllowed);
    return true;
}

/** HTTP request method as string - use for logging only */
std::string RequestMethodString(HTTPRequestMethod m)
{
    switch (m) {
    case HTTPRequestMethod::GET:
        return "GET";
    case HTTPRequestMethod::POST:
        return "POST";
    case HTTPRequestMethod::HEAD:
        return "HEAD";
    case HTTPRequestMethod::PUT:
        return "PUT";
    case HTTPRequestMethod::UNKNOWN:
        return "unknown";
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

static void MaybeDispatchRequestToWorker(std::unique_ptr<HTTPRequest> hreq)
{
    // Early address-based allow check
    if (!ClientAllowed(hreq->GetPeer())) {
        LogDebug(BCLog::HTTP, "HTTP request from %s rejected: Client network is not allowed RPC access\n",
                 hreq->GetPeer().ToStringAddrPort());
        hreq->WriteReply(HTTP_FORBIDDEN);
        return;
    }

    // Early reject unknown HTTP methods
    if (hreq->GetRequestMethod() == HTTPRequestMethod::UNKNOWN) {
        LogDebug(BCLog::HTTP, "HTTP request from %s rejected: Unknown HTTP request method\n",
                 hreq->GetPeer().ToStringAddrPort());
        hreq->WriteReply(HTTP_BAD_METHOD);
        return;
    }

    // Find registered handler for prefix
    std::string strURI = hreq->GetURI();
    std::string path;
    LOCK(g_httppathhandlers_mutex);
    std::vector<HTTPPathHandler>::const_iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::const_iterator iend = pathHandlers.end();
    for (; i != iend; ++i) {
        bool match = false;
        if (i->exactMatch)
            match = (strURI == i->prefix);
        else
            match = strURI.starts_with(i->prefix);
        if (match) {
            path = strURI.substr(i->prefix.size());
            break;
        }
    }

    // Dispatch to worker thread
    if (i != iend) {
        std::unique_ptr<HTTPWorkItem> item(new HTTPWorkItem(std::move(hreq), path, i->handler));
        assert(g_work_queue);
        if (g_work_queue->Enqueue(item.get())) {
            item.release(); /* if true, queue took ownership */
        } else {
            LogPrintf("WARNING: request rejected because http work queue depth exceeded, it can be increased with the -rpcworkqueue= setting\n");
            item->req->WriteReply(HTTP_SERVICE_UNAVAILABLE, "Work queue depth exceeded");
        }
    } else {
        hreq->WriteReply(HTTP_NOT_FOUND);
    }
}

static void RejectAllRequests(std::unique_ptr<http_bitcoin::HTTPRequest> hreq)
{
    LogDebug(BCLog::HTTP, "Rejecting request while shutting down\n");
    hreq->WriteReply(HTTP_SERVICE_UNAVAILABLE);
}

static std::vector<std::pair<std::string, uint16_t>> GetBindAddresses()
{
    uint16_t http_port{static_cast<uint16_t>(gArgs.GetIntArg("-rpcport", BaseParams().RPCPort()))};
    std::vector<std::pair<std::string, uint16_t>> endpoints;

    // Determine what addresses to bind to
    // To prevent misconfiguration and accidental exposure of the RPC
    // interface, require -rpcallowip and -rpcbind to both be specified
    // together. If either is missing, ignore both values, bind to localhost
    // instead, and log warnings.
    if (gArgs.GetArgs("-rpcallowip").empty() || gArgs.GetArgs("-rpcbind").empty()) { // Default to loopback if not allowing external IPs
        endpoints.emplace_back("::1", http_port);
        endpoints.emplace_back("127.0.0.1", http_port);
        if (!gArgs.GetArgs("-rpcallowip").empty()) {
            LogPrintf("WARNING: option -rpcallowip was specified without -rpcbind; this doesn't usually make sense\n");
        }
        if (!gArgs.GetArgs("-rpcbind").empty()) {
            LogPrintf("WARNING: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
        }
    } else { // Specific bind addresses
        for (const std::string& strRPCBind : gArgs.GetArgs("-rpcbind")) {
            uint16_t port{http_port};
            std::string host;
            if (!SplitHostPort(strRPCBind, port, host)) {
                LogError("%s\n", InvalidPortErrMsg("-rpcbind", strRPCBind).original);
                return {}; // empty
            }
            endpoints.emplace_back(host, port);
        }
    }
    return endpoints;
}

/** Simple wrapper to set thread name and run work queue */
static void HTTPWorkQueueRun(WorkQueue<HTTPClosure>* queue, int worker_num)
{
    util::ThreadRename(strprintf("httpworker.%i", worker_num));
    queue->Run();
}

void RegisterHTTPHandler(const std::string &prefix, bool exactMatch, const HTTPRequestHandler &handler)
{
    LogDebug(BCLog::HTTP, "Registering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
    LOCK(g_httppathhandlers_mutex);
    pathHandlers.emplace_back(prefix, exactMatch, handler);
}

void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch)
{
    LOCK(g_httppathhandlers_mutex);
    std::vector<HTTPPathHandler>::iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::iterator iend = pathHandlers.end();
    for (; i != iend; ++i)
        if (i->prefix == prefix && i->exactMatch == exactMatch)
            break;
    if (i != iend)
    {
        LogDebug(BCLog::HTTP, "Unregistering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
        pathHandlers.erase(i);
    }
}

namespace http_bitcoin {
using util::SplitString;

std::optional<std::string> HTTPHeaders::Find(const std::string key) const
{
    const auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    return it->second;
}

void HTTPHeaders::Write(const std::string key, const std::string value)
{
    // If present, append value to list
    const auto existing_value = Find(key);
    if (existing_value) {
        m_map[key] = existing_value.value() + ", " + value;
    } else {
        m_map[key] = value;
    }
}

void HTTPHeaders::Remove(const std::string key)
{
    m_map.erase(key);
}

bool HTTPHeaders::Read(util::LineReader& reader)
{
    // Headers https://httpwg.org/specs/rfc9110.html#rfc.section.6.3
    // A sequence of Field Lines https://httpwg.org/specs/rfc9110.html#rfc.section.5.2
    do {
        auto maybe_line = reader.ReadLine();
        if (!maybe_line) return false;
        const std::string& line = *maybe_line;

        // An empty line indicates end of the headers section https://www.rfc-editor.org/rfc/rfc2616#section-4
        if (line.length() == 0) break;

        // Header line must have at least one ":"
        // keys are not allowed to have delimiters like ":" but values are
        // https://httpwg.org/specs/rfc9110.html#rfc.section.5.6.2
        const size_t pos{line.find(':')};
        if (pos == std::string::npos) throw std::runtime_error("HTTP header missing colon (:)");

        // Whitespace is optional
        std::string key = util::TrimString(line.substr(0, pos));
        std::string value = util::TrimString(line.substr(pos + 1));
        Write(key, value);
    } while (true);

    return true;
}

std::string HTTPHeaders::Stringify() const
{
    std::string out;
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        out += it->first + ": " + it->second + "\r\n";
    }

    // Headers are terminated by an empty line
    out += "\r\n";

    return out;
}

std::string HTTPResponse::StringifyHeaders() const
{
    return strprintf("HTTP/%d.%d %d %s\r\n%s", m_version_major, m_version_minor, m_status, m_reason, m_headers.Stringify());
}

bool HTTPRequest::LoadControlData(LineReader& reader)
{
    auto maybe_line = reader.ReadLine();
    if (!maybe_line) return false;
    const std::string& request_line = *maybe_line;

    // Request Line aka Control Data https://httpwg.org/specs/rfc9110.html#rfc.section.6.2
    // Three words separated by spaces, terminated by \n or \r\n
    if (request_line.length() < MIN_REQUEST_LINE_LENGTH) throw std::runtime_error("HTTP request line too short");

    const std::vector<std::string> parts{SplitString(request_line, " ")};
    if (parts.size() != 3) throw std::runtime_error("HTTP request line malformed");
    m_method = parts[0];
    m_target = parts[1];

    if (parts[2].rfind("HTTP/") != 0) throw std::runtime_error("HTTP request line malformed");
    const std::vector<std::string> version_parts{SplitString(parts[2].substr(5), ".")};
    if (version_parts.size() != 2) throw std::runtime_error("HTTP request line malformed");
    auto major = ToIntegral<int>(version_parts[0]);
    auto minor = ToIntegral<int>(version_parts[1]);
    if (!major || !minor) throw std::runtime_error("HTTP request line malformed");
    m_version_major = major.value();
    m_version_minor = minor.value();

    return true;
}

bool HTTPRequest::LoadHeaders(LineReader& reader)
{
    return m_headers.Read(reader);
}

bool HTTPRequest::LoadBody(LineReader& reader)
{
    // https://httpwg.org/specs/rfc9112.html#message.body

    auto transfer_encoding_header = m_headers.Find("Transfer-Encoding");
    if (transfer_encoding_header && ToLower(transfer_encoding_header.value()) == "chunked") {
        // Transfer-Encoding: https://datatracker.ietf.org/doc/html/rfc7230.html#section-3.3.1
        // Chunked Transfer Coding: https://datatracker.ietf.org/doc/html/rfc7230.html#section-4.1
        // see evhttp_handle_chunked_read() in libevent http.c
        while (reader.Left() > 0) {
            auto maybe_chunk_size = reader.ReadLine();
            if (!maybe_chunk_size) return false;
            uint64_t chunk_size;

            if (!ParseUInt64Hex(maybe_chunk_size.value(), &chunk_size)) throw std::runtime_error("Invalid chunk size");

            bool last_chunk{chunk_size == 0};

            if (!last_chunk) {
                // We are still expecting more data for this chunk
                if (reader.Left() < chunk_size) {
                    return false;
                }
                // Pack chunk onto body
                m_body += reader.ReadLength(chunk_size);
            }

            // Even though every chunk size is explicitly declared,
            // they are still terminated by a CRLF we don't need.
            auto crlf = reader.ReadLine();
            if (!crlf || crlf.value().size() != 0) throw std::runtime_error("Improperly terminated chunk");

            if (last_chunk) return true;
        }

        // We read all the chunks but never got the last chunk, wait for client to send more
        return false;
    } else {
        // No Content-length or Transfer-Encoding header means no body, see libevent evhttp_get_body()
        auto content_length_value{m_headers.Find("Content-Length")};
        if (!content_length_value) return true;

        uint64_t content_length;
        if (!ParseUInt64(content_length_value.value(), &content_length)) throw std::runtime_error("Cannot parse Content-Length value");

        // Not enough data in buffer for expected body
        if (reader.Left() < content_length) return false;

        m_body = reader.ReadLength(content_length);

        return true;
    }
}

CService HTTPRequest::GetPeer() const
{
    return m_client->m_addr;
}

HTTPRequestMethod HTTPRequest::GetRequestMethod() const
{
    if (m_method == "GET") return HTTPRequestMethod::GET;
    if (m_method == "POST") return HTTPRequestMethod::POST;
    if (m_method == "HEAD") return HTTPRequestMethod::HEAD;
    if (m_method == "PUT") return HTTPRequestMethod::PUT;
    return HTTPRequestMethod::UNKNOWN;
}

std::optional<std::string> HTTPRequest::GetQueryParameter(const std::string& key) const
{
    return GetQueryParameterFromUri(GetURI(), key);
}

// See libevent http.c evhttp_parse_query_impl()
// and https://www.rfc-editor.org/rfc/rfc3986#section-3.4
std::optional<std::string> GetQueryParameterFromUri(const std::string& uri, const std::string& key)
{
    // Handle %XX encoding
    std::string decoded_uri{UrlDecode(uri)};

    // find query in URI
    size_t start = decoded_uri.find('?');
    if (start == std::string::npos) return std::nullopt;
    size_t end = decoded_uri.find('#', start);
    if (end == std::string::npos) {
        end = decoded_uri.length();
    }
    const std::string query{decoded_uri.substr(start + 1, end - start - 1)};
    // find requested parameter in query
    const std::vector<std::string> params{SplitString(query, "&")};
    for (const std::string& param : params) {
        size_t delim = param.find('=');
        if (key == param.substr(0, delim)) {
            if (delim == std::string::npos) {
                return "";
            } else {
                return param.substr(delim + 1);
            }
        }
    }
    return std::nullopt;
}

std::pair<bool, std::string> HTTPRequest::GetHeader(const std::string& hdr) const
{
    std::optional<std::string> found{m_headers.Find(hdr)};
    if (found.has_value()) {
        return std::make_pair(true, found.value());
    } else
        return std::make_pair(false, "");
}

void HTTPRequest::WriteHeader(const std::string& hdr, const std::string& value)
{
    m_response_headers.Write(hdr, value);
}

void HTTPRequest::WriteReply(HTTPStatusCode status, std::span<const std::byte> reply_body)
{
    HTTPResponse res;

    // Some response headers are determined in advance and stored in the request
    res.m_headers = std::move(m_response_headers);

    // Response version matches request version
    res.m_version_major = m_version_major;
    res.m_version_minor = m_version_minor;

    // Add response code and look up reason string
    res.m_status = status;
    res.m_reason = HTTPReason.find(status)->second;

    // See libevent evhttp_response_needs_body()
    // Response headers are different if no body is needed
    bool needs_body{status != HTTP_NO_CONTENT && (status < 100 || status >= 200)};

    bool keep_alive = false;
    // See libevent evhttp_make_header_response()
    // Expected response headers depend on protocol version
    if (m_version_major == 1) {
        // HTTP/1.0
        if (m_version_minor == 0) {
            auto connection_header{m_headers.Find("Connection")};
            if (connection_header && ToLower(connection_header.value()) == "keep-alive") {
                keep_alive = true;
                res.m_headers.Write("Connection", "keep-alive");
            }
        }

        // HTTP/1.1
        if (m_version_minor >= 1) {
            const int64_t now_seconds{TicksSinceEpoch<std::chrono::seconds>(NodeClock::now())};
            res.m_headers.Write("Date", FormatRFC7231DateTime(now_seconds));

            if (needs_body) {
                res.m_headers.Write("Content-Length", strprintf("%d", reply_body.size()));
            }
            keep_alive = true;
        }
    }

    if (needs_body && !res.m_headers.Find("Content-Type")) {
        // Default type from libevent evhttp_new_object()
        res.m_headers.Write("Content-Type", "text/html; charset=ISO-8859-1");
    }

    auto connection_header{m_headers.Find("Connection")};
    if (connection_header && ToLower(connection_header.value()) == "close") {
        // Might not exist already but we need to replace it, not append to it
        res.m_headers.Remove("Connection");
        res.m_headers.Write("Connection", "close");
        keep_alive = false;
    }

    // Serialize the response headers
    const std::string headers{res.StringifyHeaders()};
    const auto headers_bytes{std::as_bytes(std::span(headers.begin(), headers.end()))};

    // Fill the send buffer with the complete serialized response headers + body
    {
        LOCK(m_client->m_send_mutex);
        m_client->m_send_buffer.insert(m_client->m_send_buffer.end(), headers_bytes.begin(), headers_bytes.end());

        // We've been using std::span up until now but it is finally time to copy
        // data. The original data will go out of scope when WriteReply() returns.
        // This is analogous to the memcpy() in libevent's evbuffer_add()
        m_client->m_send_buffer.insert(m_client->m_send_buffer.end(), reply_body.begin(), reply_body.end());
    }

    LogDebug(
        BCLog::HTTP,
        "HTTPResponse (status code: %d size: %lld) added to send buffer for client %s (id=%lld)\n",
        status,
        headers_bytes.size() + reply_body.size(),
        m_client->m_origin,
        m_client->m_node_id);

    m_client->m_send_ready = true;
    m_client->m_refcount--;
    if (!keep_alive) {
        m_client->m_done = true;
    }
    m_client->m_server->Wake();
}

bool HTTPClient::ReadRequest(std::unique_ptr<HTTPRequest>& req)
{
    LineReader reader(m_recv_buffer, MAX_HEADERS_SIZE);

    if (!req->LoadControlData(reader)) return false;
    if (!req->LoadHeaders(reader)) return false;
    if (!req->LoadBody(reader)) return false;

    // Remove the bytes read out of the buffer.
    // If one of the above calls throws an error, the caller must
    // catch it and disconnect the client.
    m_recv_buffer.erase(
        m_recv_buffer.begin(),
        m_recv_buffer.begin() + (reader.it - reader.start));

    m_refcount++;
    return true;
}

void HTTPClient::ReceiveMsgBytes(std::span<const uint8_t> data)
{
    // Reset idle timeout
    m_idle_since = Now<SteadySeconds>();

    // Copy data from socket buffer to client receive buffer
    m_recv_buffer.insert(
        m_recv_buffer.end(),
        reinterpret_cast<const std::byte*>(data.data()),
        reinterpret_cast<const std::byte*>(data.data() + data.size()));

    // Try reading (potentially multiple) HTTP requests from the buffer
    while (m_recv_buffer.size() > 0) {
        // Create a new request object and try to fill it with data from the receive buffer
        auto req = std::make_unique<HTTPRequest>(shared_from_this());
        try {
            // Stop reading if we need more data from the client to parse a complete request
            if (!ReadRequest(req)) break;
        } catch (const std::runtime_error& e) {
            m_refcount++;
            LogDebug(
                BCLog::HTTP,
                "Error reading HTTP request from client %s (id=%lld): %s\n",
                m_origin,
                m_node_id,
                e.what());

            // We failed to read a complete request from the buffer
            m_recv_buffer.clear();
            m_done = true;
            req->WriteReply(HTTP_BAD_REQUEST);
            break;
        }

        // We read a complete request from the buffer into the queue
        LogDebug(
            BCLog::HTTP,
            "Received a %s request for %s from %s (id=%lld)\n",
            req->m_method,
            req->m_target,
            m_origin,
            m_node_id);

        // handle request
        m_server->m_request_dispatcher(std::move(req));
    }
}

std::pair<ssize_t, bool> HTTPClient::SendBytesFromBuffer()
{
    Assume(m_server);

    // Send as much data from this client's buffer as we can
    LOCK(m_send_mutex);
    ssize_t bytes_sent = 0;
    if (!m_send_buffer.empty()) {
        std::string err;
        // We don't intend to "send more" because http responses are usually small and we want the kernel to send them right away.
        bytes_sent = SendBytes(MakeUCharSpan(m_send_buffer), err);
        if (bytes_sent < 0) {
            LogDebug(
                BCLog::HTTP,
                "Error sending HTTP response data to client %s (id=%lld): %s\n",
                m_origin,
                m_node_id,
                err);
            m_send_ready = false;
            m_send_buffer.clear();
            m_done = true;
            return std::make_pair(0, false);
        }

        Assume(static_cast<size_t>(bytes_sent) <= m_send_buffer.size());
        m_send_buffer.erase(m_send_buffer.begin(), m_send_buffer.begin() + bytes_sent);
        if (m_send_buffer.empty()) {
            m_send_ready = false;
        }

        LogDebug(
            BCLog::HTTP,
            "Sent %d bytes to client %s (id=%lld)\n",
            bytes_sent,
            m_origin,
            m_node_id);
    }
    return std::make_pair(bytes_sent, m_send_ready.load());
}

ssize_t HTTPClient::SendBytes(std::span<const unsigned char> data,
                              std::string& errmsg) const
{
    if (data.empty()) {
        return 0;
    }

    int flags{MSG_NOSIGNAL | MSG_DONTWAIT};

    const ssize_t sent = m_sock->Send(reinterpret_cast<const char*>(data.data()), data.size(), flags);

    if (sent >= 0) {
        return sent;
    }

    const int err{WSAGetLastError()};
    if (err == WSAEWOULDBLOCK || err == WSAEMSGSIZE || err == WSAEINTR || err == WSAEINPROGRESS) {
        return 0;
    }
    errmsg = NetworkErrorString(err);
    return -1;
}

HTTPServer::HTTPServer(std::function<void(std::unique_ptr<HTTPRequest>)> func) : m_request_dispatcher(func)
{
    // Create a dummy pair of sockets that can be used to interrupt select/poll
    // by writing a byte to the send side.

#ifndef WIN32
    int wake_pair[2];
    auto ret = socketpair(AF_UNIX, SOCK_STREAM, 0, wake_pair);
    assert(!ret);
    m_wake_send = std::make_shared<Sock>(wake_pair[0]);
    m_wake_recv = std::make_shared<Sock>(wake_pair[1]);
#endif
};

void HTTPServer::DisconnectClients()
{
    const auto now{Now<SteadySeconds>()};
    for (auto it = m_connected_clients.begin(); it != m_connected_clients.end();) {
        bool timeout{now - (*it)->m_idle_since > m_rpcservertimeout};
        if ((*it)->m_disconnect || timeout) {
            LogDebug(BCLog::HTTP, "Disconnected HTTP client %s (id=%d)\n", (*it)->m_origin, (*it)->m_node_id);
            it = m_connected_clients.erase(it);
        } else {
            ++it;
        }
    }
    m_no_clients = m_connected_clients.size() == 0;
}


void HTTPServer::SocketHandler()
{
    Sock::EventsPerSock io_readiness;
    while (!m_listen.empty() || !m_connected_clients.empty()) {
        io_readiness.clear();
        io_readiness.reserve(m_listen.size() + m_connected_clients.size());

        // Check for the readiness of the already connected sockets and the
        // listening sockets in one call ("readiness" as in poll(2) or
        // select(2)). If none are ready, wait for a short while and return
        // empty sets.
        for (const auto& listen_socket : m_listen) {
            io_readiness.emplace(listen_socket, Sock::Events{Sock::RECV});
        }

        for (const auto& client : m_connected_clients) {
            bool select_recv = !client->m_send_ready;
            bool select_send = client->m_send_ready;
            Sock::Event event = (select_send ? Sock::SEND : 0) | (select_recv ? Sock::RECV : 0);
            io_readiness.emplace(client->m_sock, Sock::Events{event});
        }

            // WaitMany() may as well be a static method, the context of the first Sock in the vector is not relevant.
        [[maybe_unused]] bool ret = io_readiness.begin()->first->WaitMany(SELECT_TIMEOUT, io_readiness, m_wake_recv);

        // Service (send/receive) each of the already connected sockets.
        SocketHandlerConnected(m_connected_clients, io_readiness);

        // Accept new connections from listening sockets.
        SocketHandlerListening(io_readiness);

        DisconnectClients();
    }
}
void HTTPServer::SocketHandlerConnected(const std::vector<std::shared_ptr<HTTPClient>>& clients, const Sock::EventsPerSock& events_per_sock)
{
    for (auto& client : clients) {
        //
        // Receive
        //
        bool recvSet = false;
        bool sendSet = false;
        bool errorSet = false;
        {
            const auto it = events_per_sock.find(client->m_sock);
            if (it != events_per_sock.end()) {
                recvSet = it->second.occurred & Sock::RECV;
                sendSet = it->second.occurred & Sock::SEND;
                errorSet = it->second.occurred & Sock::ERR;
            }
        }

        // Send data
        if (sendSet) {
            auto [bytes_sent, data_left] = client->SendBytesFromBuffer();
            if (bytes_sent) {
                // If both receiving and (non-optimistic) sending were possible, we first attempt
                // sending. If that succeeds, but does not fully drain the send queue, do not
                // attempt to receive. This avoids needlessly queueing data if the remote peer
                // is slow at receiving data, by means of TCP flow control. We only do this when
                // sending actually succeeded to make sure progress is always made; otherwise a
                // deadlock would be possible when both sides have data to send, but neither is
                // receiving.
                if (data_left) {
                    recvSet = false;
                } else if (!client->m_refcount && (client->m_done || m_disconnect_all_clients)) {
                    client->m_disconnect = true;
                    continue;
                }
            }
        }
        if (recvSet || errorSet)
        {
            // typical socket buffer is 8K-64K
            uint8_t pchBuf[0x10000];
            int nBytes = 0;
            nBytes = client->m_sock->Recv(pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
            if (nBytes > 0)
            {
                client->ReceiveMsgBytes({pchBuf, (size_t)nBytes});
            }
            else if (nBytes == 0)
            {
                // socket closed gracefully
                client->m_disconnect = true;
            }
            else if (nBytes < 0)
            {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                {
                    LogDebug(BCLog::HTTP, "socket recv error, %s\n", NetworkErrorString(nErr));
                    client->m_disconnect = true;
                }
            }
        }
        if (!client->m_refcount && !client->m_send_ready && m_disconnect_all_clients) {
            client->m_disconnect = true;
        }
    }
}

NodeId HTTPServer::GetNewId()
{
    return m_next_id++;
}

void HTTPServer::AcceptConnection(const Sock& listen_socket) {
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    auto sock = listen_socket.Accept((struct sockaddr*)&sockaddr, &len);

    if (!sock) {
        const int nErr = WSAGetLastError();
        if (nErr != WSAEWOULDBLOCK) {
            LogPrintf("socket error accept failed: %s\n", NetworkErrorString(nErr));
        }
        return;
    }

    CService addr;
    if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr, len)) {
        LogPrintLevel(BCLog::HTTP, BCLog::Level::Warning, "Unknown socket family\n");
    }
    auto them = GetBindAddress(*sock);
    if (!sock->IsSelectable()) {
        LogPrintf("connection from %s dropped: non-selectable socket\n", them.ToStringAddrPort());
        return;
    }

    // According to the internet TCP_NODELAY is not carried into accepted sockets
    // on all platforms.  Set it again here just to be sure.
    const int on{1};
    if (sock->SetSockOpt(IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) == SOCKET_ERROR) {
        LogDebug(BCLog::HTTP, "connection from %s: unable to set TCP_NODELAY, continuing anyway\n",
                 them.ToStringAddrPort());
    }

    const NodeId node_id{GetNewId()};

    auto client = std::make_shared<HTTPClient>(node_id, addr);
    // Point back to the server
    client->m_server = this;
    // Set timeout
    client->m_idle_since = Now<SteadySeconds>();
    client->m_sock = std::move(sock);
    LogDebug(BCLog::HTTP, "HTTP Connection accepted from %s (id=%d)\n", client->m_origin, client->m_node_id);
    m_connected_clients.push_back(std::move(client));
    m_no_clients = false;
}

void HTTPServer::SocketHandlerListening(const Sock::EventsPerSock& events_per_sock)
{
    if (m_listen_interrupt) {
        m_listen.clear();
        m_listen_interrupt.reset();
    }
    for (const auto& listen_socket : m_listen) {
        const auto it = events_per_sock.find(listen_socket);
        if (it != events_per_sock.end() && it->second.occurred & Sock::RECV) {
            AcceptConnection(*listen_socket);
        }
    }
}
void HTTPServer::Wake()
{
#ifndef WIN32
    static constexpr std::byte wake_byte = std::byte('.');
    const ssize_t sent = m_wake_send->Send(&wake_byte, 1, MSG_NOSIGNAL);
    assert(sent == 1);
#endif
}

void HTTPServer::StartSocketsThreads()
{
    m_thread_socket_handler = std::thread(&util::TraceThread, "http", [this] { SocketHandler(); });
}
void HTTPServer::JoinSocketsThreads()
{
    if (m_thread_socket_handler.joinable()) {
        m_thread_socket_handler.join();
    }
}
bool HTTPServer::BindAndStartListening(const CService& addrBind, bilingual_str& strError)
{
    int nOne = 1;

    // Create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = Untranslated(strprintf("Bind address family for %s not supported", addrBind.ToStringAddrPort()));
        LogPrintLevel(BCLog::HTTP, BCLog::Level::Error, "%s\n", strError.original);
        return false;
    }

    std::unique_ptr<Sock> sock = CreateSock(addrBind.GetSAFamily(), SOCK_STREAM, IPPROTO_TCP);
    if (!sock) {
        strError = Untranslated(strprintf("Couldn't open socket for incoming connections (socket returned error %s)", NetworkErrorString(WSAGetLastError())));
        LogPrintLevel(BCLog::HTTP, BCLog::Level::Error, "%s\n", strError.original);
        return false;
    }

    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.
    if (sock->SetSockOpt(SOL_SOCKET, SO_REUSEADDR, (sockopt_arg_type)&nOne, sizeof(int)) == SOCKET_ERROR) {
        strError = Untranslated(strprintf("Error setting SO_REUSEADDR on socket: %s, continuing anyway", NetworkErrorString(WSAGetLastError())));
        LogPrintf("%s\n", strError.original);
    }

    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
        if (sock->SetSockOpt(IPPROTO_IPV6, IPV6_V6ONLY, (sockopt_arg_type)&nOne, sizeof(int)) == SOCKET_ERROR) {
            strError = Untranslated(strprintf("Error setting IPV6_V6ONLY on socket: %s, continuing anyway", NetworkErrorString(WSAGetLastError())));
            LogPrintf("%s\n", strError.original);
        }
#endif
#ifdef WIN32
        int nProtLevel = PROTECTION_LEVEL_UNRESTRICTED;
        if (sock->SetSockOpt(IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, (const char*)&nProtLevel, sizeof(int)) == SOCKET_ERROR) {
            strError = Untranslated(strprintf("Error setting IPV6_PROTECTION_LEVEL on socket: %s, continuing anyway", NetworkErrorString(WSAGetLastError())));
            LogPrintf("%s\n", strError.original);
        }
#endif
    }
    if (sock->Bind(reinterpret_cast<struct sockaddr*>(&sockaddr), len) == SOCKET_ERROR) {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. %s is probably already running."), addrBind.ToStringAddrPort(), CLIENT_NAME);
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %s)"), addrBind.ToStringAddrPort(), NetworkErrorString(nErr));
        LogPrintLevel(BCLog::HTTP, BCLog::Level::Error, "%s\n", strError.original);
        return false;
    }
    LogPrintf("Bound to %s\n", addrBind.ToStringAddrPort());

    // Listen for incoming connections
    if (sock->Listen(SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf(_("Listening for incoming connections failed (listen returned error %s)"), NetworkErrorString(WSAGetLastError()));
        LogPrintLevel(BCLog::HTTP, BCLog::Level::Error, "%s\n", strError.original);
        return false;
    }
    m_listen.emplace_back(std::move(sock));
    return true;
}

void HTTPServer::Interrupt()
{
    m_request_dispatcher = RejectAllRequests;
    m_listen_interrupt();
    Wake();
}


void HTTPServer::Stop()
{
    m_disconnect_all_clients = true;
    Wake();
    JoinSocketsThreads();
}

bool InitHTTPServer(const util::SignalInterrupt& interrupt)
{
    if (!InitHTTPAllowList())
        return false;

    // Create HTTPServer
    g_http_server = std::make_unique<HTTPServer>(MaybeDispatchRequestToWorker);

    g_http_server->m_rpcservertimeout = std::chrono::seconds(gArgs.GetIntArg("-rpcservertimeout", DEFAULT_HTTP_SERVER_TIMEOUT));

    // Bind HTTP server to specified addresses
    std::vector<std::pair<std::string, uint16_t>> endpoints{GetBindAddresses()};
    bool bind_success{false};
    for (std::vector<std::pair<std::string, uint16_t> >::iterator i = endpoints.begin(); i != endpoints.end(); ++i) {
        LogPrintf("Binding RPC on address %s port %i\n", i->first, i->second);
        const std::optional<CService> addr{Lookup(i->first, i->second, false)};
        if (addr) {
            if (addr->IsBindAny()) {
                LogPrintf("WARNING: the RPC server is not safe to expose to untrusted networks such as the public internet\n");
            }
            bilingual_str strError;
            if (!g_http_server->BindAndStartListening(addr.value(), strError)) {
                LogPrintf("Binding RPC on address %s failed: %s\n", addr->ToStringAddrPort(), strError.original);
            } else {
                bind_success = true;
            }
        } else {
            LogPrintf("Binding RPC on address %s port %i failed.\n", i->first, i->second);
        }
    }

    if (!bind_success) {
        LogPrintf("Unable to bind any endpoint for RPC server\n");
        return false;
    }

    LogDebug(BCLog::HTTP, "Initialized HTTP server\n");
    int workQueueDepth = std::max((long)gArgs.GetIntArg("-rpcworkqueue", DEFAULT_HTTP_WORKQUEUE), 1L);
    LogDebug(BCLog::HTTP, "creating work queue of depth %d\n", workQueueDepth);

    g_work_queue = std::make_unique<WorkQueue<HTTPClosure>>(workQueueDepth);

    return true;
}

static std::vector<std::thread> g_thread_http_workers;

void StartHTTPServer()
{
    int rpcThreads = std::max((long)gArgs.GetIntArg("-rpcthreads", DEFAULT_HTTP_THREADS), 1L);
    LogInfo("Starting HTTP server with %d worker threads\n", rpcThreads);
    g_http_server->StartSocketsThreads();

    for (int i = 0; i < rpcThreads; i++) {
        g_thread_http_workers.emplace_back(HTTPWorkQueueRun, g_work_queue.get(), i);
    }
}

void InterruptHTTPServer()
{
    LogDebug(BCLog::HTTP, "Interrupting HTTP server\n");
    if (g_http_server) {
        // Reject all new requests
        g_http_server->Interrupt();
    }
    if (g_work_queue) {
        // Stop workers, killing requests we haven't processed or responded to yet
        g_work_queue->Interrupt();
    }
}

void StopHTTPServer()
{
    LogDebug(BCLog::HTTP, "Stopping HTTP server\n");
    if (g_work_queue) {
        LogDebug(BCLog::HTTP, "Waiting for HTTP worker threads to exit\n");
        for (auto& thread : g_thread_http_workers) {
            thread.join();
        }
        g_thread_http_workers.clear();
    }
    if (g_http_server) {
        g_http_server->Stop();
    }
    LogDebug(BCLog::HTTP, "Stopped HTTP server\n");
}
} // namespace http_bitcoin
