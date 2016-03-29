// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONNMAN_H
#define BITCOIN_CONNMAN_H

#include "streams.h"
#include "libbtcnet/handler.h"
#include "libbtcnet/connection.h"

#include <deque>
#include <stdint.h>
#include <condition_variable>
#include <mutex>
#include <atomic>

class CAddress;
class CTransaction;
class CNetAddr;
class CSubNet;
class CService;
class proxyType;
class CChainParams;
class uint256;

class CAddrMan;
class CScheduler;
class CNode;
class CNodeStats;
class CFeeRate;

namespace boost
{
class thread_group;
} // namespace boost

struct CDNSSeedData;
class CConnman final : public CConnectionHandler
{
public:
    enum NumConnections {
        CONNECTIONS_NONE = 0,
        CONNECTIONS_IN = (1U << 0),
        CONNECTIONS_OUT = (1U << 1),
        CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
    };

    CConnman(const CChainParams& params, uint64_t nLocalServices, int nVersion, size_t max_queue_size, int max_outbound);
    ~CConnman() final;
    void Run();
    void Interrupt();
    void Stop();
    bool AddNode(std::string node, bool fOneShot);
    bool RemoveAddedNode(std::string node);
    bool DisconnectNode(std::string node);
    void RelayAddress(const CAddress& addr, int nRelayNodes);
    void RelayInventory(int height, int nBlockEstimate, const std::vector<uint256>& vHashes);
    void RelayTransaction(const CTransaction& tx, CFeeRate feerate);
    void QueuePing();
    size_t GetNodeCount(NumConnections num);
    void GetNodeStats(std::vector<CNodeStats>& vstats);
    void ResetNodeTimes(uint64_t t);
    void GetAddedNodes(std::list<std::string>& added);
    void ThreadMessageHandler();
    void DisconnectAddress(const CNetAddr& addr);
    void DisconnectSubnet(const CSubNet& subnet);

    void ForEachNode(std::function<void(CNode*)>&& func);
    void ForEachNodeIf(std::function<void(CNode*)>&& func, std::function<bool(CNode*)>&& pred);

protected:
    std::list<CConnection> OnNeedOutgoingConnections(int need_count) final;
    void OnDnsResponse(const CConnection& conn, std::list<CConnection> results) final;
    bool OnIncomingConnection(ConnID id, const CConnection& listenconn, const CConnection& resolved_conn) final;
    bool OnOutgoingConnection(ConnID id, const CConnection& conn, const CConnection& resolved_conn) final;
    bool OnConnectionFailure(const CConnection& conn, const CConnection& resolved, bool retry) final;
    bool OnDisconnected(ConnID id, bool persistent) final;
    void OnBindFailure(const CConnection& listener) final;
    bool OnDnsFailure(const CConnection& conn, bool retry) final;
    void OnWriteBufferFull(ConnID id, size_t bufsize) final;
    void OnWriteBufferReady(ConnID id, size_t bufsize) final;
    bool OnReceiveMessages(ConnID id, std::list<std::vector<unsigned char> > msgs, size_t totalsize) final;
    void OnMalformedMessage(ConnID id) final;
    void OnReadyForFirstSend(ConnID id) final;
    bool OnProxyFailure(const CConnection& conn, bool retry) final;
    void OnBytesRead(ConnID id, size_t bytes, size_t total_bytes) final;
    void OnBytesWritten(ConnID id, size_t bytes, size_t total_bytes) final;
    void OnPingTimeout(ConnID id) final;
    void OnShutdown() final;
    void OnStartup() final;

private:
    void AddDNSSeeds();
    void RelayTransaction(const CTransaction& tx, CFeeRate feerate, const CDataStream& ss);
    bool AttemptToEvictConnection(bool fPreferNewConnection);
    std::shared_ptr<CNode> FindNode(const CNetAddr& ip);
    std::shared_ptr<CNode> FindNode(const CSubNet& subNet);
    std::shared_ptr<CNode> FindNode(const std::string& addrName);
    std::shared_ptr<CNode> FindNode(const CService& addr);
    std::shared_ptr<CNode> FindNode(ConnID id);

    void NodeFinished(ConnID id);

    static CProxy CreateProxy(const proxyType& proxy);
    static CConnection CreateConnection(const CService& serv, const CConnectionOptions& opts, const CNetworkConfig& config);
    static CConnection CreateConnection(const std::string& strAddr, int port, const CConnectionOptions& opts, const CNetworkConfig& config);
    static CConnectionOptions::Family GetConnectionFamily();

    size_t m_max_queue_size;
    std::set<std::vector<unsigned char> > m_connection_groups;
    std::list<CConnection> m_connection_queue;
    std::list<CConnection> m_resolve_for_external_ip;
    std::list<CConnection> m_binds;
    int64_t m_start_time;
    int m_num_connections;
    int m_num_connecting;
    int m_outgoing_limit;
    CNetworkConfig m_config;
    bool m_dns_seed_done;
    bool m_static_seed_done;
    const CChainParams& m_params;
    std::mutex m_mutex_shutdown;
    std::condition_variable m_cond_shutdown;
    bool m_shutdown;
    bool m_started;
    std::vector<std::string> m_removed_added_nodes;

    std::atomic<int> m_incoming_connected;
    std::atomic<int> m_outgoing_connected;

    std::map<ConnID, std::shared_ptr<CNode> > m_nodes;
    std::mutex m_nodes_mutex;
    std::map<ConnID, std::weak_ptr<CNode> > m_nodes_disconnected;

    std::condition_variable m_message_wake_cond;
    std::mutex m_message_wake_mutex;
    bool m_message_wake;

    std::list<std::string> m_added_nodes;
    std::mutex m_added_nodes_mutex;
};
extern std::unique_ptr<CConnman> g_connman;
#endif // BITCOIN_CONNMAN_H
