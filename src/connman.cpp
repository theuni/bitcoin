// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "connman.h"
#include "node.h"
#include "addrman.h"
#include "net.h"
#include "chainparams.h"

#include <set>
#include <list>

namespace {

const int MAX_OUTBOUND_CONNECTIONS = 8;

//! Convert the pnSeeds6 array into usable address objects.
static std::vector<CAddress> convertSeed6(const std::vector<SeedSpec6> &vSeedsIn)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    std::vector<CAddress> vSeedsOut;
    vSeedsOut.reserve(vSeedsIn.size());
    for (std::vector<SeedSpec6>::const_iterator i(vSeedsIn.begin()); i != vSeedsIn.end(); ++i)
    {
        struct in6_addr ip;
        memcpy(&ip, i->addr, sizeof(ip));
        CAddress addr(CService(ip, i->port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
    return vSeedsOut;
}
}

static CProxy CreateProxy(const proxyType& proxy)
{
    CProxyAuth auth;
    if (proxy.randomize_credentials)
    {
        const std::string& username = strprintf("%i", insecure_rand());
        const std::string& password = strprintf("%i", insecure_rand());
        auth = CProxyAuth(username, password);
    }
    sockaddr_storage sproxy;
    socklen_t sproxy_size = sizeof(sproxy);
    memset(&sproxy, 0, sproxy_size);
    proxy.proxy.GetSockAddr((sockaddr*)&sproxy, &sproxy_size);
    return CProxy((sockaddr*)&sproxy, sproxy_size, CProxy::SOCKS5, auth);
}

static CConnection CreateConnection(const CService& serv, const CConnectionOptions& opts, const CNetworkConfig& config)
{
    CProxy socks5;
    proxyType proxy;
    if (GetProxy(serv.GetNetwork(), proxy))
    {
        socks5 = CreateProxy(proxy);
    }

    if(serv.IsTor())
    {
        return CConnection(serv.ToStringIP(), serv.GetPort(), opts, config, socks5);
    }
    else
    {
        sockaddr_storage saddr;
        socklen_t saddr_size = sizeof(saddr);
        memset(&saddr, 0, saddr_size);
        serv.GetSockAddr((sockaddr*)&saddr, &saddr_size);
        return CConnection((sockaddr*)&saddr, saddr_size, opts, config, socks5);
    }
}

static CConnection CreateConnection(const std::string& strAddr, int port, const CConnectionOptions& opts, const CNetworkConfig& config)
{
    CService serv(strAddr, port, false);
    if(serv.IsValid() && !serv.IsTor())
    {
        return CreateConnection(serv, opts, config);
    }
    else if(serv.IsValid() && serv.IsTor())
    {
        proxyType proxy;
        CProxy socks5;
        if (GetNameProxy(proxy))
            socks5 = CreateProxy(proxy);
        return CConnection(serv.ToStringIP(), serv.GetPort(), opts, config, socks5);
    }
    else
    {
        proxyType proxy;
        CProxy socks5;
        if (GetNameProxy(proxy))
            socks5 = CreateProxy(proxy);
        return CConnection(strAddr, port, opts, config, socks5);
    }
}

CThreadedConnManager::CThreadedConnManager(const CChainParams& params, uint64_t nLocalServices, int nVersion, size_t max_queue_size)
: CConnectionHandler(true), m_stop(false), m_max_queue_size(max_queue_size), m_outgoing_limit(MAX_OUTBOUND_CONNECTIONS), m_failed_binds(0)
, m_dns_seed_done(false), m_static_seed_done(false)
{

    m_dns_seed_done = !GetBoolArg("-dnsseed", true);

    const CMessageHeader::MessageStartChars& chars  = params.MessageStart();

    m_config.header_msg_string_offset=4;
    m_config.header_msg_size_offset=16;
    m_config.header_msg_size_size=4;
    m_config.header_msg_string_size=12;
    m_config.header_size=24;
    m_config.message_max_size=1000000 + m_config.header_size;
    m_config.message_start = std::vector<unsigned char>(chars, chars + sizeof(chars));
    m_config.protocol_version = nVersion;
    m_config.protocol_handshake_version = 209;
    m_config.service_flags = 0;

    CConnectionOptions opts;
    opts.fWhitelisted = true;
    opts.fPersistent = true;
    opts.doResolve = fNameLookup ? CConnectionOptions::RESOLVE_CONNECT : CConnectionOptions::NO_RESOLVE;
    opts.nRetries = -1;
    opts.nMaxSendBuffer = 5000000;
    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0)
    {
        BOOST_FOREACH(const std::string& strAddr, mapMultiArgs["-connect"])
        {
            if(!strAddr.empty())
                m_added.push_back(CreateConnection(strAddr, params.GetDefaultPort(), opts, m_config));
        }
        m_outgoing_limit = (int)m_added.size();
    }

    if (mapArgs.count("-addnode"))
    {
        if(mapMultiArgs["-addnode"].size() > 0)
        {
            BOOST_FOREACH(const std::string& strAddr, mapMultiArgs["-addnode"])
            {
                m_added.push_back(CreateConnection(strAddr, params.GetDefaultPort(), opts, m_config));
            }
        }
    }

    if (mapArgs.count("-seednode"))
    {
         CConnectionOptions oneshot_opts = opts;
         opts.fOneShot = true;
         BOOST_FOREACH(const std::string& strDest, mapMultiArgs["-seednode"])
         {
             m_added.push_back(CreateConnection(strDest, params.GetDefaultPort(), oneshot_opts, m_config));
         }
    }

    CConnectionOptions bindopts;
    bindopts.fWhitelisted = false;
    bindopts.fPersistent = false;
    bindopts.doResolve = CConnectionOptions::NO_RESOLVE;
    bindopts.nRetries = 0;
    bindopts.nMaxSendBuffer = 5000000;

    int listenport = GetArg("-port", Params().GetDefaultPort());

    fListen = GetBoolArg("-listen", DEFAULT_LISTEN);
    if (fListen) {
        if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
            BOOST_FOREACH(const std::string& strBind, mapMultiArgs["-bind"]) {
                CService serv(strBind, listenport, false);
                sockaddr_storage stor;
                socklen_t stor_size = sizeof(stor);
                memset(&stor, 0, stor_size);
                serv.GetSockAddr((sockaddr*)&stor, &stor_size);
                CConnectionListener listener((sockaddr*)&stor, stor_size, bindopts, m_config);
                m_listeners.push_back(listener);
            }
            BOOST_FOREACH(const std::string& strBind, mapMultiArgs["-whitebind"]) {

                CConnectionOptions whitebindopts = bindopts;
                bindopts.fWhitelisted = true;

                CService serv(strBind, listenport, false);
                sockaddr_storage stor;
                socklen_t stor_size = sizeof(stor);
                memset(&stor, 0, stor_size);
                serv.GetSockAddr((sockaddr*)&stor, &stor_size);
                CConnectionListener listener((sockaddr*)&stor, stor_size, whitebindopts, m_config);
                if (listener.GetPort() == 0)
                    continue;
                //TODO:
                //    return InitError(strprintf(_("Need to specify a port with -whitebind: '%s'"), strBind));
                m_listeners.push_back(listener);
            }
        }
        else {
                 sockaddr_in sock_any;
                 sock_any.sin_family = AF_INET;
                 sock_any.sin_addr.s_addr = INADDR_ANY;
                 sock_any.sin_port = htons(listenport);
                 CConnectionListener listener((sockaddr*)&sock_any, sizeof(sock_any), bindopts, m_config);
                 m_listeners.push_back(listener);

                 sockaddr_in6 sock_any6;
                 sock_any6.sin6_family = AF_INET6;
                 sock_any6.sin6_addr = in6addr_any;
                 sock_any6.sin6_port = htons(listenport);
                 CConnectionListener listener6((sockaddr*)&sock_any6, sizeof(sock_any6), bindopts, m_config);
                 m_listeners.push_back(listener6);
        }
    }
    m_bind_limit = m_listeners.size();
    m_seeds = Params().DNSSeeds();
}

void CThreadedConnManager::Stop()
{
    {
        lock_guard_type lock(m_mutex);
        m_stop = true;
        m_nodemanager.Shutdown();
    }
    m_condvar.notify_one();
}

void CThreadedConnManager::Start()
{
    m_stop = false;
    m_notify = false;
    m_start_time = GetTime();
    CConnectionHandler::Start(m_outgoing_limit, m_bind_limit);
    boost::thread event_thread(&CThreadedConnManager::Run, this);
    while(1)
    {
        {
            lock_guard_type lock(m_mutex);
            if(m_stop)
                break;
        }
        PumpEvents(true);
        if(m_notify)
            m_condvar.notify_one();
        m_notify = false;
    }
    {
        lock_guard_type lock(m_mutex);
        m_message_queue.clear();
        m_ready_for_first_send.clear();
    }
    event_thread.join();
    CConnectionHandler::Shutdown();
}

void CThreadedConnManager::Run()
{
    std::set<uint64_t> first_send;
    std::set<uint64_t> first_receive;
    std::set<uint64_t> getdata;
    std::map<uint64_t, std::deque<CReceivedMessage> > m_messages;
    while(true)
    {
        {
            unique_lock_type lock(m_mutex);
            while(!m_stop && m_message_queue.empty() && m_ready_for_first_send.empty() && m_messages.empty() && getdata.empty())
                m_condvar.wait(lock);
            if(m_stop)
                break;
            uint64_t recvTime = GetTimeMicros();

            // TODO: Cleanup this monstrosity
            for(std::map<uint64_t, CNodeMessages>::iterator i = m_message_queue.begin(); i != m_message_queue.end();)
            {
                uint64_t id = i->first;
                if(i->second.size)
                {
                    CReceivedMessage received;
                    received.vRecvMsg = i->second.messages.front();
                    received.recvTime = recvTime;
                    m_messages[id].push_back(received);
                    size_t msgsize = i->second.messages.front().size();

                    if(i->second.size >= m_max_queue_size && i->second.size - msgsize < m_max_queue_size)
                        UnpauseRecv(id);

                    i->second.size -= msgsize;

                    if(i->second.first_receive)
                    {
                        first_receive.insert(i->first);
                        i->second.first_receive = false;
                    }

                    if(!i->second.size)
                    {
                        m_message_queue.erase(i++);
                    }
                    else
                    {
                        i->second.messages.pop_front();
                        ++i;
                    }
                }
            }

            if(!m_ready_for_first_send.empty())
            {
                m_ready_for_first_send.swap(first_send);
            }
        }

        if(!first_send.empty())
        {
            for(std::set<uint64_t>::const_iterator i = first_send.begin(); i != first_send.end(); ++i)
            {
                CNode* node = m_nodemanager.GetNode(*i);
                if(node)
                {
                    node->PushVersion();
                    m_nodemanager.Release(*i);
                }
            }
            first_send.clear();
        }

        if(!getdata.empty())
        {
            for(std::set<uint64_t>::iterator it = getdata.begin(); it != getdata.end(); ++it)
            {
                uint64_t id = *it;
                CNode* node = m_nodemanager.GetNode(id);
                if(node)
                {
                    GetNodeSignals().ProcessMessages(node);
                    if(node->vRecvGetData.empty())
                        getdata.erase(it++);
                    else
                        ++it;
                    m_nodemanager.Release(id);
                }
            }
        }

        if(!m_messages.empty())
        {
            for(std::map<uint64_t, std::deque<CReceivedMessage> >::iterator i = m_messages.begin(); i != m_messages.end();)
            {
                uint64_t id = i->first;
                CNode* node = m_nodemanager.GetNode(id);
                if(node)
                {
                    if(first_receive.erase(id))
                    {
                        node->vRecvMsg.push_back(i->second.front());
                        i->second.pop_front();
                        GetNodeSignals().ProcessMessages(node);
                    }
                    else if(node->vRecvMsg.empty() && node->vRecvGetData.empty())
                    {
                        node->vRecvMsg.push_back(i->second.front());
                        i->second.pop_front();
                        GetNodeSignals().ProcessMessages(node);
                    }
                    if(i->second.empty())
                    {
                        if(!node->vRecvGetData.empty())
                            getdata.insert(id);
                    }
                    else
                        getdata.erase(id);

                    if(node->fDisconnect)
                        CloseConnection(id, true);

                    m_nodemanager.Release(id);
                }
                else
                {
                    m_messages.erase(i++);
                    continue;
                }
                if(i->second.empty())
                    m_messages.erase(i++);
                else
                    ++i;
            }
        }
        SendMessages();
    }
}

void ForEach(uint64_t id, CNode* node, bool trickle)
{
    GetNodeSignals().SendMessages(node, trickle || node->fWhitelisted);
}

void CThreadedConnManager::SendMessages()
{
    m_nodemanager.ForEach(&ForEach);
}

bool CThreadedConnManager::OnReceiveMessages(uint64_t id, CNodeMessages& msgs)
{
    {
        lock_guard_type lock(m_mutex);
        std::map<uint64_t, CNodeMessages>::iterator it = m_message_queue.find(id);
        if(it == m_message_queue.end())
        {
            m_message_queue.insert(std::make_pair(id, msgs));
        }
        else
        {
            it->second.messages.splice(it->second.messages.end(), msgs.messages, msgs.messages.begin(), msgs.messages.end());
            it->second.size += msgs.size;
        }
        // TODO
        //if(it->second.size < m_max_queue_size && it->second.size + msgs.size >= m_max_queue_size)
        //    PauseRecv(id);
    }
    m_notify = true;
    return true;
}

void CThreadedConnManager::OnReadyForFirstSend(const CConnection& conn, uint64_t id)
{
    {
        lock_guard_type lock(m_mutex);
        m_ready_for_first_send.insert(id);
    }
    m_notify = true;
}

void CThreadedConnManager::OnWriteBufferFull(uint64_t id, size_t)
{
    PauseRecv(id);
}

void CThreadedConnManager::OnWriteBufferReady(uint64_t id, size_t)
{
    UnpauseRecv(id);
}

void CThreadedConnManager::OnDnsFailure(const CConnection& conn)
{
    for(std::vector<CDNSSeedData>::iterator i = m_seeds.begin(); i != m_seeds.end(); ++i)
    {
        if(conn.GetHost() == i->host)
        {
            i = m_seeds.erase(i);
            return;
        }
    }
}

void CThreadedConnManager::OnDnsResponse(const CConnection& conn, uint64_t, std::list<CConnection>& resolved)
{
    CNetAddr source;
    for(std::vector<CDNSSeedData>::iterator i = m_seeds.begin(); i != m_seeds.end(); ++i)
    {
        if(conn.GetHost() == i->host)
        {
            i = m_seeds.erase(i);
            source = CNetAddr(i->name);
            break;
        }
    }
    if(!source.IsValid())
        return;

    std::vector<CAddress> addresses;
    for(std::list<CConnection>::iterator i = resolved.begin(); i != resolved.end(); ++i)
    {
        int nOneDay = 24*3600;
        CAddress addr = CAddress(CService(*i));
        addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
        if(addr.IsValid())
            addresses.push_back(addr);
    }
    if(!addresses.empty())
        addrman.Add(addresses, source);
}

void CThreadedConnManager::OnConnectionFailure(const CConnection& conn, const CConnection& resolved_conn, uint64_t, bool, uint64_t)
{
    printf("failed connection to: %s\n", conn.GetHost().c_str());
    addrman.Attempt(CService(resolved_conn));
}

bool CThreadedConnManager::OnIncomingConnection(const CConnectionListener&, const CConnection& resolved_conn, uint64_t id, int incount, int totalincount)
{
    CService serv(resolved_conn);
    bool whitelist = resolved_conn.GetOptions().fWhitelisted || CNode::IsWhitelistedRange(serv);
    if (!whitelist && CNode::IsBanned(serv))
        return false;
    
    bool oneshot = resolved_conn.GetOptions().fOneShot;
    if(m_nodemanager.AddNode(id, CAddress(serv), "", true, whitelist, oneshot))
    {
        m_num_connections++;
        return true;
    }
    return false;
}

bool CThreadedConnManager::OnOutgoingConnection(const CConnection& conn, const CConnection& resolved_conn, uint64_t, uint64_t id)
{
    CService serv(resolved_conn);
    bool whitelist = resolved_conn.GetOptions().fWhitelisted || CNode::IsWhitelistedRange(serv);
    if (!whitelist && CNode::IsBanned(serv))
        return false;
    addrman.Attempt(serv);
    bool oneshot = resolved_conn.GetOptions().fOneShot;
    if(m_nodemanager.AddNode(id, CAddress(serv), strprintf("%s:%i", conn.GetHost(), conn.GetPort()), false, whitelist, oneshot))
    {
        m_connected_groups.insert(serv.GetGroup());
        m_num_connections++;
        return true;
    }
    return false;
}

void CThreadedConnManager::OnBindFailure(const CConnectionListener& conn, uint64_t, bool retry, uint64_t)
{
    if(!retry)
        m_failed_binds++;
    if(m_failed_binds == m_bind_limit)
        assert(0);
}

void CThreadedConnManager::OnBind(const CConnectionListener& conn, uint64_t, uint64_t)
{
    sockaddr_storage sstor;
    int sstor_size = sizeof(sstor);
    memset(&sstor, 0, sstor_size);
    conn.GetSockAddr((sockaddr*)&sstor, &sstor_size);
    CConnection bound((sockaddr*)&sstor, sstor_size, conn.GetOptions(), conn.GetNetConfig());
    CService serv(bound);
    if (serv.IsRoutable() && fDiscover && !conn.GetOptions().fWhitelisted)
        AddLocal(serv, LOCAL_BIND);
    LogPrintf("Bound to %s\n", serv.ToString());
}

bool CThreadedConnManager::OnNeedOutgoingConnections(std::vector<CConnection>& conns, int count)
{
    if(!m_dns_seed_done && (GetBoolArg("-forcednsseed", false) || (m_num_connections < 2 && GetTime() - m_start_time > 11 && addrman.size() == 0)))
    {
        CConnectionOptions dns_opts;
        dns_opts.fWhitelisted = false;
        dns_opts.fPersistent = false;
        dns_opts.nRetries = -1;
        if(HaveNameProxy())
        {
            dns_opts.doResolve = CConnectionOptions::RESOLVE_CONNECT;
            dns_opts.fOneShot = true;
        }
        else
        {
            dns_opts.doResolve = CConnectionOptions::RESOLVE_ONLY;
        }
        for(std::vector<CDNSSeedData>::iterator i = m_seeds.begin(); i != m_seeds.end(); ++i)
            m_added.push_back(CreateConnection(i->host, Params().GetDefaultPort(), dns_opts, m_config));

        m_dns_seed_done = true;
    }

    while(!m_added.empty() && conns.size() <= (size_t)count)
    {
        conns.push_back(m_added.front());
        m_added.pop_front();
    }

    if(!conns.empty())
        return true;

    if (!m_static_seed_done && addrman.size() == 0 && (GetTime() - m_start_time > 60)) {
        LogPrintf("Adding fixed seed nodes as DNS doesn't seem to be available.\n");
        addrman.Add(convertSeed6(Params().FixedSeeds()), CNetAddr("127.0.0.1"));
        m_static_seed_done = true;
    }

    int64_t nANow = GetAdjustedTime();

    CConnectionOptions opts;
    opts.fWhitelisted = false;
    opts.fPersistent = false;
    opts.doResolve = CConnectionOptions::NO_RESOLVE;
    opts.nRetries = 0;
    opts.nMaxSendBuffer = 5000000;

    for(int nTries = 0; nTries != 100; nTries++)
    {
        CAddrInfo addr = addrman.Select();

        // if we selected an invalid address, restart
        if (!addr.IsValid() || m_connected_groups.count(addr.GetGroup()) || IsLocal(addr))
            break;

        if (IsLimited(addr) || CNode::IsBanned(addr))
            continue;

        // only consider very recently tried nodes after 30 failed attempts
        if (nANow - addr.nLastTry < 600 && nTries < 30)
            continue;

        // do not allow non-default ports, unless after 50 invalid addresses selected already
        if (nTries < 50 && addr.GetPort() != Params().GetDefaultPort())
            continue;

        conns.push_back(CreateConnection(addr, opts, m_config));
            break;
    }
    return !conns.empty();
}

bool CThreadedConnManager::OnNeedIncomingListener(CConnectionListener& listener, uint64_t)
{
    if(!m_listeners.empty())
    {
        listener = m_listeners.front();
        m_listeners.pop_front();
        return true;
    }
    return false;
}

void CThreadedConnManager::OnDisconnected(const CConnection& conn, uint64_t id, bool, uint64_t)
{
    m_connected_groups.erase(CService(conn).GetGroup());
    m_nodemanager.Remove(id);
    m_num_connections--;
}

void CThreadedConnManager::OnMalformedMessage(uint64_t id)
{
    CloseConnection(id, true);
}

void CThreadedConnManager::OnProxyConnected(uint64_t, const CConnection& conn)
{
}

void CThreadedConnManager::OnProxyFailure(const CConnection&, const CConnection&, uint64_t, bool, uint64_t)
{
}

void CThreadedConnManager::OnBytesRead(uint64_t id, size_t bytes, size_t total_bytes)
{
    CNode* node = m_nodemanager.GetNode(id);
    if(node)
    {
        node->nLastRecv = GetTime();
        node->nRecvBytes += bytes;
        node->RecordBytesRecv(bytes);
        m_nodemanager.Release(0);
    }
}

void CThreadedConnManager::OnBytesWritten(uint64_t id, size_t bytes, size_t total_bytes)
{
    CNode* node = m_nodemanager.GetNode(id);
    if(node)
    {
        node->nLastSend = GetTime();
        node->nSendBytes += bytes;
        node->RecordBytesSent(bytes);
        m_nodemanager.Release(id);
    }
}
