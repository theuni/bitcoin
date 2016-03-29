// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "connman.h"

#include "net.h"

#include "addrman.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "clientversion.h"
#include "consensus/consensus.h"
#include "crypto/common.h"
#include "hash.h"
#include "primitives/transaction.h"
#include "ui_interface.h"
#include "utilstrencodings.h"

#include <boost/thread.hpp>


std::unique_ptr<CConnman> g_connman;
//! Convert the pnSeeds6 array into usable address objects.
static std::vector<CAddress> convertSeed6(const std::vector<SeedSpec6>& vSeedsIn)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    std::vector<CAddress> vSeedsOut;
    vSeedsOut.reserve(vSeedsIn.size());
    for (std::vector<SeedSpec6>::const_iterator i(vSeedsIn.begin()); i != vSeedsIn.end(); ++i) {
        struct in6_addr ip;
        memcpy(&ip, i->addr, sizeof(ip));
        CAddress addr(CService(ip, i->port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
    return vSeedsOut;
}

std::shared_ptr<CNode> CConnman::FindNode(const CNetAddr& ip)
{
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    for (auto&& it : m_nodes) {
        auto pnode(it.second);
        if ((CNetAddr)pnode->addr == ip)
            return (pnode);
    }
    return NULL;
}

std::shared_ptr<CNode> CConnman::FindNode(const CSubNet& subNet)
{
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    for (auto&& it : m_nodes) {
        auto pnode(it.second);
        if (subNet.Match((CNetAddr)pnode->addr))
            return pnode;
    }
    return NULL;
}

std::shared_ptr<CNode> CConnman::FindNode(const std::string& addrName)
{
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    for (auto&& it : m_nodes) {
        auto pnode(it.second);
        if (pnode->addrName == addrName)
            return pnode;
    }
    return NULL;
}

std::shared_ptr<CNode> CConnman::FindNode(const CService& addr)
{
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    for (auto&& it : m_nodes) {
        auto pnode(it.second);
        if ((CService)pnode->addr == addr)
            return pnode;
    }
    return NULL;
}

static bool ReverseCompareNodeMinPingTime(const std::shared_ptr<CNode>& a, const std::shared_ptr<CNode>& b)
{
    return a->nMinPingUsecTime > b->nMinPingUsecTime;
}

static bool ReverseCompareNodeTimeConnected(const std::shared_ptr<CNode>& a, const std::shared_ptr<CNode>& b)
{
    return a->nTimeConnected > b->nTimeConnected;
}

class CompareNetGroupKeyed
{
    std::vector<unsigned char> vchSecretKey;

public:
    CompareNetGroupKeyed()
    {
        vchSecretKey.resize(32, 0);
        GetRandBytes(vchSecretKey.data(), vchSecretKey.size());
    }

    bool operator()(const std::shared_ptr<CNode>& a, const std::shared_ptr<CNode>& b)
    {
        std::vector<unsigned char> vchGroupA, vchGroupB;
        CSHA256 hashA, hashB;
        std::vector<unsigned char> vchA(32), vchB(32);

        vchGroupA = a->addr.GetGroup();
        vchGroupB = b->addr.GetGroup();

        hashA.Write(begin_ptr(vchGroupA), vchGroupA.size());
        hashB.Write(begin_ptr(vchGroupB), vchGroupB.size());

        hashA.Write(begin_ptr(vchSecretKey), vchSecretKey.size());
        hashB.Write(begin_ptr(vchSecretKey), vchSecretKey.size());

        hashA.Finalize(begin_ptr(vchA));
        hashB.Finalize(begin_ptr(vchB));

        return vchA < vchB;
    }
};

bool CConnman::AttemptToEvictConnection(bool fPreferNewConnection)
{
    std::vector<std::shared_ptr<CNode> > vEvictionCandidates;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);

        for (auto&& it : m_nodes) {
            auto node(it.second);
            if (node->fWhitelisted)
                continue;
            if (!node->fInbound)
                continue;
            if (node->fDisconnect)
                continue;
            vEvictionCandidates.push_back(node);
        }
    }

    if (vEvictionCandidates.empty())
        return false;

    // Protect connections with certain characteristics

    // Deterministically select 4 peers to protect by netgroup.
    // An attacker cannot predict which netgroups will be protected.
    static CompareNetGroupKeyed comparerNetGroupKeyed;
    std::sort(vEvictionCandidates.begin(), vEvictionCandidates.end(), comparerNetGroupKeyed);
    vEvictionCandidates.erase(vEvictionCandidates.end() - std::min(4, static_cast<int>(vEvictionCandidates.size())), vEvictionCandidates.end());

    if (vEvictionCandidates.empty())
        return false;

    // Protect the 8 nodes with the best ping times.
    // An attacker cannot manipulate this metric without physically moving nodes closer to the target.
    std::sort(vEvictionCandidates.begin(), vEvictionCandidates.end(), ReverseCompareNodeMinPingTime);
    vEvictionCandidates.erase(vEvictionCandidates.end() - std::min(8, static_cast<int>(vEvictionCandidates.size())), vEvictionCandidates.end());

    if (vEvictionCandidates.empty())
        return false;

    // Protect the half of the remaining nodes which have been connected the longest.
    // This replicates the existing implicit behavior.
    std::sort(vEvictionCandidates.begin(), vEvictionCandidates.end(), ReverseCompareNodeTimeConnected);
    vEvictionCandidates.erase(vEvictionCandidates.end() - static_cast<int>(vEvictionCandidates.size() / 2), vEvictionCandidates.end());

    if (vEvictionCandidates.empty())
        return false;

    // Identify the network group with the most connections and youngest member.
    // (vEvictionCandidates is already sorted by reverse connect time)
    std::vector<unsigned char> naMostConnections;
    unsigned int nMostConnections = 0;
    int64_t nMostConnectionsTime = 0;
    std::map<std::vector<unsigned char>, std::vector<std::shared_ptr<CNode> > > mapAddrCounts;
    for (const auto& node : vEvictionCandidates) {
        mapAddrCounts[node->addr.GetGroup()].push_back(node);
        int64_t grouptime = mapAddrCounts[node->addr.GetGroup()][0]->nTimeConnected;
        size_t groupsize = mapAddrCounts[node->addr.GetGroup()].size();

        if (groupsize > nMostConnections || (groupsize == nMostConnections && grouptime > nMostConnectionsTime)) {
            nMostConnections = groupsize;
            nMostConnectionsTime = grouptime;
            naMostConnections = node->addr.GetGroup();
        }
    }

    // Reduce to the network group with the most connections
    vEvictionCandidates = mapAddrCounts[naMostConnections];

    // Do not disconnect peers if there is only one unprotected connection from their network group.
    if (vEvictionCandidates.size() <= 1)
        // unless we prefer the new connection (for whitelisted peers)
        if (!fPreferNewConnection)
            return false;

    // Disconnect from the network group with the most connections
    vEvictionCandidates[0]->fDisconnect = true;

    return true;
}

void CConnman::ThreadMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    bool fSleep = false;
    while (true) {
        {
            std::vector<std::weak_ptr<CNode> > m_nodesCopy;
            if (fSleep) {
                std::unique_lock<std::mutex> lock(m_message_wake_mutex);
                m_message_wake_cond.wait_for(lock, std::chrono::milliseconds(100), [this] {return m_message_wake; });
                m_message_wake = false;
            }
            {
                std::lock_guard<std::mutex> lock(m_nodes_mutex);
                for (auto&& i : m_nodes)
                    m_nodesCopy.emplace_back(i.second);
            }
            fSleep = true;
            for (auto&& weaknode : m_nodesCopy) {
                auto pnode = weaknode.lock();
                if (!pnode)
                    continue;
                if (pnode->fDisconnect) {
                    g_connman->CloseConnection(pnode->id, false);
                    continue;
                }

                // Receive messages
                {
                    TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                    if (lockRecv) {
                        if (!GetNodeSignals().ProcessMessages(pnode.get())) {
                            pnode->vRecvMsg.clear();
                            g_connman->CloseConnection(pnode->id, false);
                        }

                        if (!pnode->fSendFull) {
                            if (!pnode->vRecvGetData.empty() || (!pnode->vRecvMsg.empty() && pnode->vRecvMsg[0].complete()))
                                fSleep = false;
                        }
                    }
                }
                boost::this_thread::interruption_point();

                // Send messages
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend)
                        GetNodeSignals().SendMessages(pnode.get());
                }
            }
            boost::this_thread::interruption_point();
        }
    }
}

CProxy CConnman::CreateProxy(const proxyType& proxy)
{
    CProxyAuth auth;
    if (proxy.randomize_credentials) {
        const std::string& username = strprintf("%i", insecure_rand());
        const std::string& password = strprintf("%i", insecure_rand());
        auth = CProxyAuth(username, password);
    }
    sockaddr_storage sproxy;
    socklen_t sproxy_size = sizeof(sproxy);
    memset(&sproxy, 0, sproxy_size);
    proxy.proxy.GetSockAddr((sockaddr*)&sproxy, &sproxy_size);
    return CProxy((sockaddr*)&sproxy, sproxy_size, CProxy::SOCKS5_FORCE_DOMAIN_TYPE, auth);
}

CConnection CConnman::CreateConnection(const CService& serv, const CConnectionOptions& opts, const CNetworkConfig& config)
{
    if(IsLimited(serv))
        return CConnection();

    CProxy socks5;
    proxyType proxy;

    if (GetProxy(serv.GetNetwork(), proxy))
        socks5 = CreateProxy(proxy);

    if (serv.IsTor())
        return CConnection(opts, config, socks5, serv.ToStringIP(), serv.GetPort());
    else {
        sockaddr_storage saddr;
        socklen_t saddr_size = sizeof(saddr);
        memset(&saddr, 0, saddr_size);
        serv.GetSockAddr((sockaddr*)&saddr, &saddr_size);
        return CConnection(opts, config, socks5, (sockaddr*)&saddr, saddr_size);
    }
}

CConnection CConnman::CreateConnection(const std::string& strAddr, int port, const CConnectionOptions& opts, const CNetworkConfig& config)
{
    CService serv(strAddr, port, false);

    CProxy socks5;
    proxyType proxy;

    if(serv.IsValid() && IsLimited(serv))
        return CConnection();
    else if (serv.IsValid() && GetProxy(serv.GetNetwork(), proxy))
        socks5 = CreateProxy(proxy);
    else if(!serv.IsValid() && GetNameProxy(proxy))
        socks5 = CreateProxy(proxy);
    return CConnection(opts, config, socks5, strAddr, port);
}

CConnectionOptions::Family CConnman::GetConnectionFamily()
{
    int family_flags = CConnectionOptions::NONE;
    if(!IsLimited(NET_IPV4))
        family_flags |= CConnectionOptions::IPV4;
    if(!IsLimited(NET_IPV6))
        family_flags |= CConnectionOptions::IPV6;
    if(!IsLimited(NET_TOR))
        family_flags |= CConnectionOptions::ONION_PROXY;
    return static_cast<CConnectionOptions::Family>(family_flags);
}

CConnman::CConnman(const CChainParams& params, uint64_t nLocalServices, int nVersion, size_t max_queue_size, int max_outbound)
    : CConnectionHandler(true), m_max_queue_size(max_queue_size), m_outgoing_limit(max_outbound), m_dns_seed_done(false), m_static_seed_done(false), m_params(params), m_shutdown(false), m_started(false)
{
    m_num_connecting = 0;
    m_num_connections = 0;
    m_incoming_connected = 0;
    m_outgoing_connected = 0;

    m_dns_seed_done = !GetBoolArg("-dnsseed", true);

    const CMessageHeader::MessageStartChars& chars = m_params.MessageStart();

    m_config.header_msg_size_offset = 16;
    m_config.header_msg_size_size = 4;
    m_config.header_size = 24;
    m_config.chunk_size = 0;
    m_config.message_max_size = 2 * 1024 * 1024 + m_config.header_size;
    m_config.message_start = std::vector<unsigned char>(chars, chars + sizeof(chars));
    m_config.protocol_version = nVersion;
    m_config.protocol_handshake_version = 209;
    m_config.service_flags = nLocalServices;

    CConnectionOptions opts;
    opts.nFamily = GetConnectionFamily();
    opts.fWhitelisted = false;
    opts.fPersistent = true;
    opts.doResolve = fNameLookup ? CConnectionOptions::RESOLVE_CONNECT : CConnectionOptions::NO_RESOLVE;
    opts.nRetries = -1;
    opts.nConnTimeout = 5;
    opts.nMaxSendBuffer = m_max_queue_size;
    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        for (const auto& strAddr : mapMultiArgs["-connect"]) {
            if (!strAddr.empty())
                m_connection_queue.push_back(CreateConnection(strAddr, m_params.GetDefaultPort(), opts, m_config));
        }
        m_outgoing_limit = (int)m_connection_queue.size();
    }

    if (mapArgs.count("-addnode")) {
        if (mapMultiArgs["-addnode"].size() > 0) {
            std::list<std::string> added;
            for (const auto& strAddr : mapMultiArgs["-addnode"]) {
                if (!strAddr.empty()) {
                    added.push_back(strAddr);
                    m_connection_queue.push_back(CreateConnection(strAddr, m_params.GetDefaultPort(), opts, m_config));
                }
            }
            std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
            m_added_nodes = std::move(added);
        }
    }

    if (mapArgs.count("-seednode")) {
        CConnectionOptions oneshot_opts = opts;
        oneshot_opts.fOneShot = true;
        oneshot_opts.fPersistent = false;
        for (const auto& strAddr : mapMultiArgs["-seednode"]) {
            if (!strAddr.empty())
                m_connection_queue.push_back(CreateConnection(strAddr, m_params.GetDefaultPort(), oneshot_opts, m_config));
        }
    }

    if (mapArgs.count("-externalip")) {
        CConnectionOptions external_opts = opts;
        external_opts.doResolve = CConnectionOptions::RESOLVE_ONLY;
        external_opts.fPersistent = false;
        for (const auto& strAddr : mapMultiArgs["-externalip"]) {
            CConnection conn(CreateConnection(strAddr, m_params.GetDefaultPort(), external_opts, m_config));
            if (conn.IsSet()) {
                if (!conn.IsDNS()) {
                    AddLocal(CService(strAddr, GetListenPort(), false), LOCAL_MANUAL);
                } else if (fNameLookup) {
                    LogPrintf("Resolving external ip: %s\n", conn.ToString());
                    m_resolve_for_external_ip.push_back(conn);
                    m_connection_queue.push_back(conn);
                }
            }
        }
    }

    CConnectionOptions bindopts;
    bindopts.fWhitelisted = false;
    bindopts.fPersistent = false;
    bindopts.doResolve = CConnectionOptions::NO_RESOLVE;
    bindopts.nRetries = 0;
    bindopts.nMaxSendBuffer = m_max_queue_size;

    int listenport = GetArg("-port", m_params.GetDefaultPort());
    fListen = GetBoolArg("-listen", DEFAULT_LISTEN);
    if (fListen) {
        if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
            for (const auto& strBind : mapMultiArgs["-bind"])
                m_binds.emplace_back(bindopts, m_config, strBind, listenport);
            for (const auto& strBind : mapMultiArgs["-whitebind"]) {
                CConnectionOptions whitebindopts = bindopts;
                whitebindopts.fWhitelisted = true;
                m_binds.emplace_back(whitebindopts, m_config, strBind, listenport);
            }
        } else {
            sockaddr_in sock_any;
            memset(&sock_any, 0, sizeof(sock_any));
            sock_any.sin_family = AF_INET;
            sock_any.sin_addr.s_addr = INADDR_ANY;
            sock_any.sin_port = htons(listenport);
            m_binds.emplace_back(bindopts, m_config, (sockaddr*)&sock_any, sizeof(sock_any));

            sockaddr_in6 sock_any6;
            memset(&sock_any6, 0, sizeof(sock_any6));
            sock_any6.sin6_family = AF_INET6;
            sock_any6.sin6_addr = in6addr_any;
            sock_any6.sin6_port = htons(listenport);
            m_binds.emplace_back(bindopts, m_config, (sockaddr*)&sock_any6, sizeof(sock_any6));
        }
    }

    if (GetBoolArg("-forcednsseed", false))
        AddDNSSeeds();
}

CConnman::~CConnman()
{
}

bool CConnman::AddNode(std::string node, bool fOneTry)
{
    {
        std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
        for (auto it(m_added_nodes.begin()); it != m_added_nodes.end(); ++it) {
            if (*it == node)
                return false;
        }
        if (!fOneTry)
            m_added_nodes.push_back(node);
    }

    CConnectionOptions opts;
    opts.doResolve = fNameLookup ? CConnectionOptions::RESOLVE_CONNECT : CConnectionOptions::NO_RESOLVE;
    opts.nMaxSendBuffer = m_max_queue_size;
    opts.fOneShot = false;
    opts.fPersistent = !fOneTry;
    opts.nRetries = fOneTry ? 0 : -1;
    opts.nFamily = GetConnectionFamily();

    LogPrintf("addnode: %s. whitelisted: %i. retries: %i. persistent: %i\n", node, opts.fWhitelisted, opts.nRetries, opts.fPersistent);
    m_connection_queue.push_back(CreateConnection(node, m_params.GetDefaultPort(), opts, m_config));
    return true;
}

bool CConnman::RemoveAddedNode(std::string node)
{
    {
        std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
        for (auto it(m_added_nodes.begin()); it != m_added_nodes.end(); ++it) {
            if (*it == node) {
                m_added_nodes.erase(it);
                m_removed_added_nodes.push_back(std::move(node));
                return true;
            }
        }
        return false;
    }
}

bool CConnman::DisconnectNode(std::string node)
{
    ConnID id = -1;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        for (auto&& it : m_nodes) {
            auto pnode(it.second);
            if (pnode->addrName == node) {
                id = pnode->id;
                break;
            }
        }
    }
    if (id != -1) {
        CloseConnection(id, true);
        return true;
    }
    return false;
}

void CConnman::DisconnectAddress(const CNetAddr& addr)
{
    auto func = [this](CNode* pnode) {
        CloseConnection(pnode->id, true);
    };
    auto pred = [&addr](CNode* pnode) {
        return pnode->addr == addr;
    };
    ForEachNodeIf(func, pred);
}

void CConnman::DisconnectSubnet(const CSubNet& subNet)
{
    auto func = [this](CNode* pnode) {
        CloseConnection(pnode->id, true);
    };
    auto pred = [&subNet](CNode* pnode) {
        return subNet.Match((CNetAddr)pnode->addr);
    };
    ForEachNodeIf(func, pred);
}

void CConnman::AddDNSSeeds()
{
    CConnectionOptions dns_opts;
    dns_opts.fWhitelisted = false;
    dns_opts.fPersistent = false;
    dns_opts.nRetries = 0;
    dns_opts.nFamily = GetConnectionFamily();
    if (HaveNameProxy()) {
        dns_opts.doResolve = CConnectionOptions::RESOLVE_CONNECT;
        dns_opts.fOneShot = true;
    } else {
        dns_opts.doResolve = CConnectionOptions::RESOLVE_ONLY;
    }
    std::vector<CDNSSeedData> seeds(m_params.DNSSeeds());
    for (std::vector<CDNSSeedData>::iterator i = seeds.begin(); i != seeds.end(); ++i)
        m_connection_queue.push_back(CreateConnection(i->host, m_params.GetDefaultPort(), dns_opts, m_config));

    m_dns_seed_done = true;
}

std::list<CConnection> CConnman::OnNeedOutgoingConnections(int need_count)
{
    std::list<CConnection> conns;
    if (!m_dns_seed_done && m_num_connections < 2 && m_connection_queue.empty() && !m_num_connecting && !addrman.size() && (GetTime() - m_start_time > 11))
        AddDNSSeeds();

    while (!m_connection_queue.empty() && conns.size() <= (size_t)need_count) {
        if (!m_connection_queue.front().IsDNS() || m_connection_queue.front().GetOptions().doResolve == CConnectionOptions::RESOLVE_CONNECT)
            m_num_connecting++;
        if(m_connection_queue.front().IsSet())
            conns.push_back(m_connection_queue.front());
        m_connection_queue.pop_front();
    }

    if (!conns.empty()) {
        for (auto&& i : conns)
            LogPrintf("connecting to: %s\n", i.ToString());
        return conns;
    }

    if (!m_static_seed_done && addrman.size() == 0 && (GetTime() - m_start_time > 60)) {
        LogPrintf("Adding fixed seed nodes as DNS doesn't seem to be available.\n");
        addrman.Add(convertSeed6(m_params.FixedSeeds()), CNetAddr("127.0.0.1"));
        m_static_seed_done = true;
    }

    int64_t nANow = GetAdjustedTime();

    CConnectionOptions opts;
    opts.fWhitelisted = false;
    opts.fPersistent = false;
    opts.nFamily = GetConnectionFamily();
    opts.doResolve = CConnectionOptions::NO_RESOLVE;
    opts.nRetries = 0;
    opts.nMaxSendBuffer = m_max_queue_size;

    for (int nTries = 0; nTries != 100; nTries++) {
        CAddrInfo addr = addrman.Select();

        // if we selected an invalid address, restart
        if (!addr.IsValid() || m_connection_groups.count(addr.GetGroup()) || IsLocal(addr))
            break;

        if (IsLimited(addr) || CNode::IsBanned(addr))
            continue;

        // only consider very recently tried nodes after 30 failed attempts
        if (nANow - addr.nLastTry < 600 && nTries < 30)
            continue;

        // do not allow non-default ports, unless after 50 invalid addresses selected already
        if (nTries < 50 && addr.GetPort() != m_params.GetDefaultPort())
            continue;

        LogPrintf("Trying connection to: %s\n", addr.ToString());
        m_connection_groups.insert(addr.GetGroup());
        conns.push_back(CreateConnection(addr, opts, m_config));
        m_num_connecting++;
        break;
    }
    return conns;
}

void CConnman::Interrupt()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex_shutdown);
        if (!m_started)
            return;
    }
    Shutdown();
}

void CConnman::Stop()
{
    std::unique_lock<std::mutex> lock(m_mutex_shutdown);
    if (!m_started)
        return;
    m_cond_shutdown.wait(lock, [this] {return m_started && m_shutdown && m_nodes_disconnected.empty(); });
    m_started = false;
    m_shutdown = false;
}

void CConnman::Run()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex_shutdown);
        m_started = true;
    }
    boost::this_thread::disable_interruption di;
    Start(m_outgoing_limit);

    while (1) {
        if (!PumpEvents(true))
            break;
    }
}

void CConnman::RelayAddress(const CAddress& addr, int nRelayNodes)
{
    {
        // Use deterministic randomness to send to the same nodes for 24 hours
        // at a time so the addrKnowns of the chosen nodes prevent repeats
        static uint256 hashSalt;
        if (hashSalt.IsNull())
            hashSalt = GetRandHash();
        uint64_t hashAddr = addr.GetHash();
        uint256 hashRand = ArithToUint256(UintToArith256(hashSalt) ^ (hashAddr << 32) ^ ((GetTime() + hashAddr) / (24 * 60 * 60)));
        hashRand = Hash(BEGIN(hashRand), END(hashRand));
        std::multimap<uint256, std::weak_ptr<CNode> > mapMix;
        {
            std::lock_guard<std::mutex> lock(m_nodes_mutex);
            for (auto&& it : m_nodes) {
                auto pnode(it.second);
                if (pnode->nVersion < CADDR_TIME_VERSION)
                    continue;
                unsigned int nPointer;
                memcpy(&nPointer, &pnode, sizeof(nPointer));
                uint256 hashKey = ArithToUint256(UintToArith256(hashRand) ^ nPointer);
                hashKey = Hash(BEGIN(hashKey), END(hashKey));
                mapMix.insert(make_pair(hashKey, pnode));
            }
        }
        for (std::multimap<uint256, std::weak_ptr<CNode> >::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi) {
            auto ptr(mi->second.lock());
            if (ptr)
                ptr->PushAddress(addr);
        }
    }
}

void CConnman::RelayInventory(int height, int nBlockEstimate, const std::vector<uint256>& vHashes)
{
    auto func = [&vHashes](CNode* pnode) {
        for(auto it(vHashes.rbegin()); it != vHashes.rend(); ++it)
            pnode->PushBlockHash(*it);
    };
    auto pred = [height, nBlockEstimate](CNode* pnode) {
        return height > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate);
    };
    ForEachNodeIf(func, pred);
}

void CConnman::ForEachNode(std::function<void(CNode*)>&& func)
{
    std::vector<std::weak_ptr<CNode> > nodes;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        nodes.reserve(m_nodes.size());
        for (auto&& i : m_nodes)
            nodes.emplace_back(i.second);
    }
    for (auto&& i : nodes) {
        auto shared(i.lock());
        if (shared)
            func(shared.get());
    }
}

void CConnman::ForEachNodeIf(std::function<void(CNode*)>&& func, std::function<bool(CNode*)>&& pred)
{
    std::vector<std::weak_ptr<CNode> > nodes;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        nodes.reserve(m_nodes.size());
        for (auto&& i : m_nodes)
            nodes.emplace_back(i.second);
    }
    for (auto&& i : nodes) {
        auto shared(i.lock());
        if (shared && pred(shared.get()))
            func(shared.get());
    }
}

void CConnman::RelayTransaction(const CTransaction& tx, CFeeRate feerate)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, feerate, ss);
}

void CConnman::RelayTransaction(const CTransaction& tx, CFeeRate feerate, const CDataStream& ss)
{
    CInv inv(MSG_TX, tx.GetHash());
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetTime()) {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert(std::make_pair(inv, ss));
        vRelayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
    }

    auto func = [&tx, &inv](CNode* pnode) {
        if (pnode->pfilter)
        {
            if (pnode->pfilter->IsRelevantAndUpdate(tx))
                pnode->PushInventory(inv);
        } else
            pnode->PushInventory(inv);
    };
    auto pred = [&feerate](CNode* pnode) {
        if(!pnode->fRelayTxes)
            return false;
        {
            LOCK(pnode->cs_feeFilter);
            if (feerate.GetFeePerK() < pnode->minFeeFilter)
                return false;
        }
        return true;
    };
    ForEachNodeIf(func, pred);
}

void CConnman::QueuePing()
{
    auto func = [](CNode* pnode) {
        pnode->fPingQueued = true;
    };
    ForEachNode(func);
}

size_t CConnman::GetNodeCount(NumConnections num)
{
    if (num == CONNECTIONS_IN)
        return m_incoming_connected;
    else if (num == CONNECTIONS_OUT)
        return m_outgoing_connected;
    else if (num == CONNECTIONS_ALL)
        return m_incoming_connected + m_outgoing_connected;
    return 0;
}

void CConnman::GetNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();
    auto func = [&vstats](CNode* pnode) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    };
    ForEachNode(func);
}

void CConnman::GetAddedNodes(std::list<std::string>& added)
{
    std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
    added = m_added_nodes;
}

void CConnman::ResetNodeTimes(uint64_t t)
{
    auto func = [t](CNode* pnode) {
        pnode->nLastSend = pnode->nLastRecv = t;
    };
    ForEachNode(func);
}

void CConnman::OnDnsResponse(const CConnection& conn, std::list<CConnection> results)
{
    for (auto it = m_resolve_for_external_ip.begin(); it != m_resolve_for_external_ip.end(); ++it) {
        if (conn.GetHost() == it->GetHost()) {
            for (auto&& resolved : results)
                AddLocal(CService(resolved.ToString(), false), LOCAL_MANUAL);
            m_resolve_for_external_ip.erase(it);
            return;
        }
    }

    std::vector<CAddress> addresses;
    for (auto&& resolved : results) {
        sockaddr_storage stor;
        int addr_size = sizeof(stor);
        memset(&stor, 0, addr_size);
        resolved.GetSockAddr((sockaddr*)&stor, &addr_size);
        CService serv;
        serv.SetSockAddr((sockaddr*)&stor);

        CAddress addr = CAddress(serv);
        int nOneDay = 24 * 3600;
        addr.nTime = GetTime() - 3 * nOneDay - GetRand(4 * nOneDay); // use a random age between 3 and 7 days old
        if (addr.IsValid())
            addresses.push_back(addr);
    }
    CNetAddr source(addresses.front()); // TODO: For now, just using the first result as the source
    if (!addresses.empty())
        addrman.Add(addresses, source);
}

void CConnman::NodeFinished(ConnID id)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex_shutdown);
        auto it(m_nodes_disconnected.find(id));
        assert(it != m_nodes_disconnected.end());
        m_nodes_disconnected.erase(id);
    }
    m_cond_shutdown.notify_one();
}

bool CConnman::OnIncomingConnection(ConnID id, const CConnection& listenconn, const CConnection& resolved_conn)
{
    LogPrintf("on incoming: %s:", resolved_conn.ToString());
    CService serv(resolved_conn.ToString());

    int nMaxInbound = nMaxConnections - m_outgoing_limit;

    bool whitelisted = resolved_conn.GetOptions().fWhitelisted || CNode::IsWhitelistedRange(serv);
    if (!whitelisted && CNode::IsBanned(serv))
        return false;

    int nInbound = 0;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        for (auto&& it : m_nodes) {
            auto pnode(it.second);
            if (pnode->fInbound)
                nInbound++;
        }
    }

    if (nInbound >= nMaxInbound) {
        if (!AttemptToEvictConnection(whitelisted)) {
            // No connection to evict, disconnect the new connection
            LogPrint("net", "failed to find an eviction candidate - connection dropped (full)\n");
            return false;
        }
    }


    m_num_connections++;
    m_incoming_connected++;
    auto deleter = [this](CNode* node) {
        ConnID id = node->id;
        delete node;
        NodeFinished(id);
    };

    auto pnode = std::shared_ptr<CNode>(new CNode(id, CAddress(serv), "", true), deleter);
    pnode->nTimeConnected = GetTime();
    pnode->fNetworkNode = true;
    pnode->fWhitelisted = whitelisted;
    size_t node_count;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        m_nodes.emplace_hint(m_nodes.end(), id, std::move(pnode));
        node_count = m_nodes.size();
    }
    uiInterface.NotifyNumConnectionsChanged(node_count);
    return true;
}

bool CConnman::OnOutgoingConnection(ConnID id, const CConnection& conn, const CConnection& resolved_conn)
{
    m_num_connecting--;
    CService serv(resolved_conn.ToString());
    bool whitelist = CNode::IsWhitelistedRange(serv);
    if (!whitelist && CNode::IsBanned(serv))
        return false;

    m_num_connections++;
    m_outgoing_connected++;
    addrman.Attempt(serv);

    auto deleter = [this](CNode* node) {
        ConnID id = node->id;
        delete node;
        NodeFinished(id);
    };

    auto pnode = std::shared_ptr<CNode>(new CNode(id, CAddress(serv), conn.GetConnectionString(), false), deleter);
    pnode->nTimeConnected = GetTime();
    pnode->fNetworkNode = true;
    if (resolved_conn.GetOptions().fOneShot)
        pnode->fOneShot = true;
    size_t node_count;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        m_nodes.emplace_hint(m_nodes.end(), id, std::move(pnode));
        node_count = m_nodes.size();
    }
    uiInterface.NotifyNumConnectionsChanged(node_count);
    return true;
}
bool CConnman::OnConnectionFailure(const CConnection& conn, const CConnection& resolved, bool retry)
{
    LogPrintf("Connection failed to: %s\n", conn.ToString().c_str());
    bool ret = true;
    {
        std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
        for (auto it(m_removed_added_nodes.begin()); it != m_removed_added_nodes.end(); ++it) {
            if (*it == conn.GetConnectionString()) {
                m_removed_added_nodes.erase(it);
                ret = false;
                break;
            }
        }
    }
    if (!retry) {
        m_num_connecting--;
        CService serv(resolved.ToString());
        m_connection_groups.erase(serv.GetGroup());
    }
    return ret;
}
bool CConnman::OnDisconnected(ConnID id, bool persistent)
{
    LogPrintf("Peer %lu disconnected. persistent: %i\n", id, persistent);
    std::string addrName;
    std::shared_ptr<CNode> node;
    size_t node_count;
    {
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        auto it(m_nodes.find(id));
        if (it != m_nodes.end()) {
            node = it->second;
            m_nodes.erase(it);
            node_count = m_nodes.size();
        }
    }

    bool ret = true;
    if (node) {
        node->fDisconnect = true;
        addrName = node->addrName;
        m_num_connections--;
        if (node->fInbound)
            m_incoming_connected--;
        else
            m_outgoing_connected--;

        if (node->fOneShot)
            ret = false;

        uiInterface.NotifyNumConnectionsChanged(node_count);
        {
            std::lock_guard<std::mutex> lock(m_mutex_shutdown);
            m_nodes_disconnected.emplace(id, node);
        }
        m_connection_groups.erase(node->addr.GetGroup());
        LOCK(node->cs_vRecvMsg);
        node->vRecvMsg.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
        for (auto it(m_removed_added_nodes.begin()); it != m_removed_added_nodes.end(); ++it) {
            if (*it == addrName) {
                m_removed_added_nodes.erase(it);
                ret = false;
                break;
            }
        }
    }
    return ret;
}
void CConnman::OnBindFailure(const CConnection& listener)
{
    LogPrintf("Error: failed to bind to address: %s\n", listener.ToString());
}
bool CConnman::OnDnsFailure(const CConnection& conn, bool retry)
{
    LogPrintf("Error: failed to resolve address: %s\n", conn.ToString());
    bool ret = true;
    {
        std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
        for (auto it(m_removed_added_nodes.begin()); it != m_removed_added_nodes.end(); ++it) {
            if (*it == conn.GetConnectionString()) {
                m_removed_added_nodes.erase(it);
                ret = false;
                break;
            }
        }
    }
    return ret;
}

void CConnman::OnWriteBufferFull(ConnID id, size_t bufsize)
{
    auto node(FindNode(id));
    if (node)
        node->fSendFull = true;
}
void CConnman::OnWriteBufferReady(ConnID id, size_t bufsize)
{
    auto node(FindNode(id));
    if (node)
        node->fSendFull = false;
}
bool CConnman::OnReceiveMessages(ConnID id, std::list<std::vector<unsigned char> > msgs, size_t totalsize)
{
    auto node(FindNode(id));
    if (node) {
        bool bad_message = false;
        {
            LOCK(node->cs_vRecvMsg);
            for (const auto& i : msgs) {
                if(!node->ReceiveMsgBytes((const char*)i.data(), i.size())) {
                    bad_message = true;
                    break;
                }
            }
        }
        if(bad_message) {
            CloseConnection(id, true);
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_message_wake_mutex);
            m_message_wake = true;
        }
        m_message_wake_cond.notify_one();
        return true;
    } else
        return false;
}

void CConnman::OnMalformedMessage(ConnID id)
{
    LogPrintf("Error: malformed message from peer: %li\n");
}

void CConnman::OnReadyForFirstSend(ConnID id)
{
    auto node(FindNode(id));
    if (node && !node->fInbound) {
        node->PushVersion();
        {
            std::lock_guard<std::mutex> lock(m_message_wake_mutex);
            m_message_wake = true;
        }
        m_message_wake_cond.notify_one();
    }
}
bool CConnman::OnProxyFailure(const CConnection& conn, bool retry)
{
    LogPrintf("Error: failed to connect to proxy: %s for connection: %s\n", conn.GetProxy().ToString(), conn.ToString());
    bool ret = true;
    {
        std::lock_guard<std::mutex> lock(m_added_nodes_mutex);
        for (auto it(m_removed_added_nodes.begin()); it != m_removed_added_nodes.end(); ++it) {
            if (*it == conn.GetConnectionString()) {
                m_removed_added_nodes.erase(it);
                ret = false;
                break;
            }
        }
    }
    if (!retry) {
        CService serv(conn.ToString());
        if (serv.IsValid())
            m_connection_groups.erase(serv.GetGroup());
    }
    return ret;
}

std::shared_ptr<CNode> CConnman::FindNode(ConnID id)
{
    std::shared_ptr<CNode> node;
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    auto it = m_nodes.find(id);
    if (it != m_nodes.end())
        node = it->second;
    return node;
}

void CConnman::OnBytesRead(ConnID id, size_t bytes, size_t total_bytes)
{
    std::shared_ptr<CNode> node(FindNode(id));
    if (node) {
        node->nLastRecv = GetTime();
        node->nRecvBytes += bytes;
        node->RecordBytesRecv(bytes);
    }
}
void CConnman::OnBytesWritten(ConnID id, size_t bytes, size_t total_bytes)
{
    std::shared_ptr<CNode> node(FindNode(id));
    if (node) {
        node->nLastSend = GetTime();
        node->nSendBytes += bytes;
        node->RecordBytesSent(bytes);
    }
}

void CConnman::OnPingTimeout(ConnID id)
{
    LogPrintf("ping timeout for peer: %li\n", id);
    CloseConnection(id, true);
}

void CConnman::OnShutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex_shutdown);
        m_shutdown = true;
    }
    m_cond_shutdown.notify_one();
}

void CConnman::OnStartup()
{
    CRateLimit limit;
    limit.nMaxBurstRead = 1000000;
    limit.nMaxReadRate = 1000000;
    limit.nMaxBurstWrite = 1000000;
    limit.nMaxWriteRate = 1000000;
    //SetOutgoingRateLimit(limit);

    for (auto&& i : m_binds) {
        if (!i.IsSet())
            continue;
        bool bound = Bind(i);
        if (bound)
            LogPrintf("Bound to: %s\n", i.ToString());
    }
    m_binds.clear();
    m_start_time = GetTime();
}
