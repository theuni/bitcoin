// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_THREADEDCONNMAN_H
#define BITCOIN_NET_THREADEDCONNMAN_H

#include "libbtcnet/handler.h"
#include "nodeman.h"
#include "chainparams.h"
#include <set>
#include <deque>
class CThreadedNode;
class CNode;
class CThreadedConnManager : public CConnectionHandler
{
protected:
    bool OnIncomingConnection(const CConnectionListener& listenconn, const CConnection& resolved_conn, uint64_t id, int incount, int totalincount);
    bool OnOutgoingConnection(const CConnection& conn, const CConnection& resolved_conn, uint64_t attemptId, uint64_t id);
    void OnConnectionFailure(const CConnection& conn, const CConnection& resolved_conn, uint64_t id, bool retry, uint64_t retryid);
    void OnReadyForFirstSend(const CConnection& conn, uint64_t id);
    void OnDisconnected(const CConnection& conn, uint64_t id, bool persistent, uint64_t retryid);
    void OnBindFailure(const CConnectionListener& listener, uint64_t id, bool retry, uint64_t retryid);
    void OnBind(const CConnectionListener& listener, uint64_t attemptId, uint64_t id);
    bool OnNeedIncomingListener(CConnectionListener&, uint64_t);
    void OnDnsFailure(const CConnection& conn);
    void OnWriteBufferFull(uint64_t id, size_t bufsize);
    void OnWriteBufferReady(uint64_t id, size_t bufsize);
    bool OnReceiveMessages(uint64_t id, CNodeMessages& msgs);
    void OnMalformedMessage(uint64_t id);

    bool OnNeedOutgoingConnections(std::vector<CConnection>&, int);
    void OnDnsResponse(const CConnection&, uint64_t, std::list<CConnection>&);
    void OnProxyConnected(uint64_t, const CConnection&);
    void OnProxyFailure(const CConnection&, const CConnection&, uint64_t, bool, uint64_t);
    void OnBytesRead(uint64_t id, size_t bytes, size_t total_bytes);
    void OnBytesWritten(uint64_t id, size_t bytes, size_t total_bytes);

public:
    CThreadedConnManager(const CChainParams& params, uint64_t nLocalServices, int nVersion, size_t max_queue_size);
    void AddSeed(const CConnection& conn);
    void AddAddress(const CConnection& conn);
    void AddListener(const CConnectionListener& listener);
    CConnection GetAddress();
    void Start();
    void Run();
    void Stop();
private:
    void SendMessages();
    std::map<uint64_t, CConnection> connections_connecting;
    bool m_stop;
    CThreadedNodeManager m_nodemanager;
    base_mutex_type m_mutex;
    condition_variable_type m_condvar;
    std::map<uint64_t, CNodeMessages> m_message_queue;
    std::set<uint64_t> m_ready_for_first_send;
    size_t m_max_queue_size;
    std::list<CConnectionListener> m_listeners;
    std::set<std::vector<unsigned char> > m_connected_groups;
    std::deque<CConnection> m_oneshots;
    std::deque<CConnection> m_added;
    std::vector<CDNSSeedData> m_seeds;
    int64_t m_start_time;
    int m_num_connections;
    std::map<uint64_t, CConnection> m_seed_queries;
    int m_outgoing_limit;
    int m_bind_limit;
    bool m_notify;
    CNetworkConfig m_config;
    int m_failed_binds;
    bool m_dns_seed_done;
    bool m_static_seed_done;
};

#endif // BITCOIN_NET_THREADEDCONNMAN_H
