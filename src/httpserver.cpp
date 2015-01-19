// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httpserver.h"

#include "chainparamsbase.h"
#include "httprpc.h"
#include "util.h"
#include "netbase.h"
#include "rpcprotocol.h" // For HTTP status codes
#include "sync.h"
#include "ui_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/thread.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

#include "json/json_spirit_writer_template.h"
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

/** HTTP request work item */
class HTTPWorkItem
{
private:
    boost::shared_ptr<HTTPRequest> req; // Replace with unique_ptr in C++11
    boost::function<void(HTTPRequest*)> func;
public:
    HTTPWorkItem()
    {
    }
    HTTPWorkItem(boost::shared_ptr<HTTPRequest> &req, const boost::function<void(HTTPRequest*)> &func):
        req(req),
        func(func)
    {
    }

    void operator () ()
    {
        func(req.get());
    }
};

/** Simple work queue for distributing work over multiple threads.
 * Work items are simply callable objects.
 */
template<typename WorkItem> class WorkQueue
{
private:
    /** Mutex protects entire object */
    CWaitableCriticalSection cs;
    CConditionVariable cond;
    std::deque<WorkItem> queue;
    bool running;
    size_t maxDepth;

public:
    WorkQueue(size_t maxDepth):
        running(true),
        maxDepth(maxDepth)
    {
    }
    ~WorkQueue()
    {
    }
    /** Enqueue a work item */
    bool Enqueue(WorkItem item)
    {
        boost::unique_lock<boost::mutex> lock(cs);
        if (queue.size() >= maxDepth)
            return false;
        queue.push_back(item);
        LogPrintf("Enqueued work (depth %i)\n", queue.size());
        cond.notify_one();
        return true;
    }
    /** Thread function */
    void Run()
    {
        while(running)
        {
            WorkItem i;
            {
                boost::unique_lock<boost::mutex> lock(cs);
                while (running && queue.empty())
                    cond.wait(lock);
                if (!running)
                    break;
                i = queue.front();
                queue.pop_front();
                LogPrintf("Dequeued work (depth %i)\n", queue.size());
            }
            i();
        }
    }
    /** Interrupt and exit loops */
    void Interrupt()
    {
        boost::unique_lock<boost::mutex> lock(cs);
        running = false;
        cond.notify_all();
    }

    /** Return current depth of queue */
    size_t Depth()
    {
        boost::unique_lock<boost::mutex> lock(cs);
        return queue.size();
    }
};

/** HTTP module state */
static struct event_base *eventBase = 0;
struct evhttp *eventHTTP = 0;
static std::vector<CSubNet> rpc_allow_subnets; //!< List of subnets to allow RPC connections from
static WorkQueue<HTTPWorkItem> *workQueue = 0;


static bool ClientAllowed(const CNetAddr &netaddr)
{
    if (!netaddr.IsValid())
        return false;
    BOOST_FOREACH(const CSubNet &subnet, rpc_allow_subnets)
        if (subnet.Match(netaddr))
            return true;
    return false;
}

static bool InitHTTPAllowList()
{
    rpc_allow_subnets.clear();
    rpc_allow_subnets.push_back(CSubNet("127.0.0.0/8")); // always allow IPv4 local subnet
    rpc_allow_subnets.push_back(CSubNet("::1")); // always allow IPv6 localhost
    if (mapMultiArgs.count("-rpcallowip"))
    {
        const std::vector<std::string>& vAllow = mapMultiArgs["-rpcallowip"];
        BOOST_FOREACH(std::string strAllow, vAllow)
        {
            CSubNet subnet(strAllow);
            if(!subnet.IsValid())
            {
                uiInterface.ThreadSafeMessageBox(
                    strprintf("Invalid -rpcallowip subnet specification: %s. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).", strAllow),
                    "", CClientUIInterface::MSG_ERROR);
                return false;
            }
            rpc_allow_subnets.push_back(subnet);
        }
    }
    std::string strAllowed;
    BOOST_FOREACH(const CSubNet &subnet, rpc_allow_subnets)
        strAllowed += subnet.ToString() + " ";
    LogPrint("http", "Allowing HTTP connections from: %s\n", strAllowed);
    return true;
}

static std::string EvbufferToString(struct evbuffer *buf)
{
    std::string strRequest;
    char cbuf[128];
    int n;
    /* As we just copy memory to memory, I doubt that this is the most efficient way possible. */
    strRequest.reserve(evbuffer_get_length(buf));
    while ((n = evbuffer_remove(buf, cbuf, sizeof(cbuf))) > 0)
        strRequest.append(cbuf, cbuf+n);
    return strRequest;
}

/** HTTP request method as string - use for logging only */
static std::string RequestMethodString(HTTPRequest::RequestMethod m)
{
    switch (m) {
    case HTTPRequest::GET:  return "GET"; break;
    case HTTPRequest::POST: return "POST"; break;
    case HTTPRequest::HEAD: return "HEAD"; break;
    case HTTPRequest::PUT:  return "PUT"; break;
    default: return "unknown";
    }
}

/** HTTP request callback */
static void http_request_cb(struct evhttp_request *req, void *arg)
{
    // XXX should be std::unique_ptr when C++11
    boost::shared_ptr<HTTPRequest> hreq(new HTTPRequest(req));

    LogPrint("http", "Received a %s request for %s from %s\n",
        RequestMethodString(hreq->GetRequestMethod()), hreq->GetURI(), hreq->GetPeer().ToString());

    // Early reject unknown HTTP methods
    if (hreq->GetRequestMethod() == HTTPRequest::UNKNOWN)
    {
        hreq->WriteReply(HTTP_BADMETHOD);
        return;
    }

    // Early address-based allow check
    if (!ClientAllowed(hreq->GetPeer()))
    {
        hreq->WriteReply(HTTP_FORBIDDEN);
        return;
    }

    assert(workQueue);

    // XXX replace with registration function instead of direct dependency
    std::string strURI = hreq->GetURI();
    boost::function<void(HTTPRequest*)> func;
    if (strURI == "/") {
        func = HTTPReq_JSONRPC;
    } else if (strURI.substr(0, 5) == "/rest") {
        func = HTTPReq_REST;
    }

    // Dispatch to worker thread
    if (func) {
        if (!workQueue->Enqueue(HTTPWorkItem(hreq, func)))
            hreq->WriteReply(HTTP_INTERNAL, "Work queue depth exceeded");
    } else {
        hreq->WriteReply(HTTP_NOTFOUND);
    }
}

/** Event dispatcher thread */
static void ThreadHTTP(struct event_base *base, struct evhttp *http)
{
    RenameThread("bitcoin-http");
    LogPrint("http", "Entering http event loop\n");
    event_base_dispatch(base);
    /* Event loop will be interrupted by InterruptHTTPServer() */
    LogPrint("http", "Exited http event loop\n");
}

/** Bind HTTP server to specified addresses */
static bool HTTPBindAddresses(struct evhttp *http)
{
    int defaultPort = GetArg("-rpcport", BaseParams().RPCPort());
    int nBound = 0;
    std::vector< std::pair< std::string, uint16_t > > endpoints;

    // Determine what addresses to bind to
    if (!mapArgs.count("-rpcallowip")) // Default to loopback if not allowing external IPs
    {
        endpoints.push_back(std::make_pair("::1", defaultPort));
        endpoints.push_back(std::make_pair("127.0.0.1", defaultPort));
        if (mapArgs.count("-rpcbind"))
        {
            LogPrintf("WARNING: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
        }
    } else if (mapArgs.count("-rpcbind")) // Specific bind address
    {
        const std::vector<std::string>& vbind = mapMultiArgs["-rpcbind"];
        for (std::vector<std::string>::const_iterator i = vbind.begin(); i != vbind.end(); ++i)
        {
            int port = defaultPort;
            std::string host;
            SplitHostPort(*i, port, host);
            endpoints.push_back(std::make_pair(host, port));
        }
    } else { // No specific bind address specified, bind to any
        endpoints.push_back(std::make_pair("::", defaultPort));
        endpoints.push_back(std::make_pair("0.0.0.0", defaultPort));
    }

    // Bind addresses
    for (std::vector< std::pair< std::string, uint16_t > >::iterator i = endpoints.begin(); i != endpoints.end(); ++i)
    {
        LogPrint("http", "Binding RPC on address %s port %i\n", i->first, i->second);
        if (evhttp_bind_socket(http, i->first.empty() ? NULL : i->first.c_str(), i->second) == 0) {
            nBound += 1;
        } else {
            LogPrintf("Binding RPC on address %s port %i failed.\n", i->first, i->second);
        }
    }
    return nBound > 0;
}

/** Simple wrapper to set thread name and run work queue */
static void HTTPWorkQueueRun(WorkQueue<HTTPWorkItem> *queue)
{
    RenameThread("bitcoin-httpworker");
    queue->Run();
}

bool StartHTTPServer(boost::thread_group& threadGroup)
{
    struct evhttp *http = 0;
    struct event_base *base = 0;

    if (!InitHTTPAllowList())
        return false;

#ifdef WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif

    base = event_base_new(); // XXX RAII
    if (!base) {
        LogPrintf("Couldn't create an event_base: exiting\n");
        return false;
    }

    /* Create a new evhttp object to handle requests. */
    http = evhttp_new(base); // XXX RAII
    if (!http) {
        LogPrintf("couldn't create evhttp. Exiting.\n");
        event_base_free(base);
        return false;
    }

    evhttp_set_timeout(http, GetArg("-rpctimeout", 30));
    evhttp_set_max_body_size(http, MAX_SIZE);
    evhttp_set_gencb(http, http_request_cb, NULL);

    if (!HTTPBindAddresses(http))
    {
        LogPrintf("Unable to bind any endpoint for RPC server\n");
        evhttp_free(http);
        event_base_free(base);
        return false;
    }

    LogPrint("http", "Starting HTTP server\n");
    int workQueueDepth = std::max(GetArg("-rpcworkqueue", 16), 1L);
    workQueue = new WorkQueue<HTTPWorkItem>(workQueueDepth);

    threadGroup.create_thread(boost::bind(&ThreadHTTP, base, http));

    int rpcThreads = std::max(GetArg("-rpcthreads", 4), 1L);
    for (int i = 0; i < rpcThreads; i++)
        threadGroup.create_thread(boost::bind(&HTTPWorkQueueRun, workQueue));

    eventBase = base;
    eventHTTP = http;
    return true;
}

void InterruptHTTPServer()
{
    LogPrint("http", "Interrupting HTTP server\n");
    if (eventBase)
        event_base_loopbreak(eventBase);
    if (workQueue)
        workQueue->Interrupt();
}

void StopHTTPServer()
{
    LogPrint("http", "Stopping HTTP server\n");
    delete workQueue;
    if (eventHTTP)
    {
        evhttp_free(eventHTTP);
        eventHTTP = 0;
    }
    if (eventBase)
    {
        event_base_free(eventBase);
        eventBase = 0;
    }
}

struct event_base *EventBase()
{
    return eventBase;
}

HTTPRequest::HTTPRequest(struct evhttp_request* req):
    req(req),
    replySent(false)
{
}
HTTPRequest::~HTTPRequest()
{
    if (!replySent)
    {
        // Keep track of whether reply was sent to avoid request leaks
        LogPrintf("%s: Unhandled request\n", __func__);
        WriteReply(HTTP_INTERNAL, "Unhandled request");
    }
    // evhttpd cleans up the request, as long as a reply was sent.
}

std::pair<bool,std::string> HTTPRequest::GetHeader(const std::string &hdr)
{
    const struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
    assert(headers);
    const char *val = evhttp_find_header(headers, hdr.c_str());
    if (val)
        return std::make_pair(true, val);
    else
        return std::make_pair(false, "");
}

std::string HTTPRequest::ReadBody()
{
    assert(evhttp_request_get_input_buffer(req));
    return EvbufferToString(evhttp_request_get_input_buffer(req));
}

void HTTPRequest::WriteHeader(const std::string &hdr, const std::string &value)
{
    struct evkeyvalq *headers = evhttp_request_get_output_headers(req);
    assert(headers);
    evhttp_add_header(headers, hdr.c_str(), value.c_str());
}

void HTTPRequest::WriteReply(int nStatus, const std::string &strReply)
{
    struct evbuffer *evb = evbuffer_new(); // XXX: RAII
    evbuffer_add(evb, strReply.data(), strReply.size());
    evhttp_send_reply(req, nStatus, NULL, evb);
    evbuffer_free(evb);
    replySent = true;
}

CService HTTPRequest::GetPeer()
{
    evhttp_connection *con = evhttp_request_get_connection(req);
    CService peer;
    if (con)
    {
        // evhttp retains ownership over returned address string
        const char *address = "";
        uint16_t port = 0;
        evhttp_connection_get_peer(con, (char **)&address, &port);
        peer = CService(address, port);
    }
    return peer;
}

std::string HTTPRequest::GetURI()
{
    return evhttp_request_get_uri(req);
}

HTTPRequest::RequestMethod HTTPRequest::GetRequestMethod()
{
    switch (evhttp_request_get_command(req)) {
    case EVHTTP_REQ_GET: return GET; break;
    case EVHTTP_REQ_POST: return POST; break;
    case EVHTTP_REQ_HEAD: return HEAD; break;
    case EVHTTP_REQ_PUT: return PUT; break;
    default: return UNKNOWN; break;
    }
}

