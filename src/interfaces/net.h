// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INTERFACES_NET_H
#define BITCOIN_INTERFACES_NET_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <optional>

#include <interfaces/nodeid.h>
#include <netaddress.h>
#include <node/connection_types.h>
#include <protocol.h>
#include <sync.h>

struct CSerializedNetMsg;
class CAddress;
class CNetMessage;
class CService;
enum class NetPermissionFlags : uint32_t;

struct PeerOptions
{
    ConnectionType conn_type;
    CAddress addr;
    std::string addr_name;
    NetPermissionFlags permission_flags;
    std::chrono::seconds connected;
    bool inbound_onion;
    uint32_t mapped_as;
    uint64_t keyed_net_group;
    Network connected_through_net;
    bool send_local_address;
};

/**
 * Interface for message handling
 */
class NetEventsInterface
{
public:
    /** Mutex for anything that is only accessed via the msg processing thread */
    static Mutex g_msgproc_mutex;

    /** Initialize a peer (setup state) */
    virtual std::optional<NodeId> InitializeNode(PeerOptions options) = 0;

    /** Handle removal of a peer (clear state) */
    virtual void MarkNodeDisconnected(NodeId) = 0;

    virtual void MarkSendBufferFull(NodeId, bool) = 0;

    /**
     * Callback to determine whether the given set of service flags are sufficient
     * for a peer to be "relevant".
     */
    virtual bool HasAllDesirableServiceFlags(ServiceFlags services) const = 0;

    virtual void WakeMessageHandler() = 0;
protected:
    /**
     * Protected destructor so that instances can only be deleted by derived classes.
     * If that restriction is no longer desired, this should be made public and virtual.
     */
    ~NetEventsInterface() = default;
};

namespace interfaces {

struct NetManagerEvents
{
    virtual bool PushMessage(NodeId, CSerializedNetMsg&&) = 0;
    virtual bool DisconnectNode(NodeId) = 0;
    virtual bool OutboundTargetReached(bool) const = 0;
    virtual std::vector<CAddress> GetAddresses(size_t, size_t, std::optional<Network>, const bool = true) const = 0;
    virtual std::vector<CAddress> GetAddresses(NodeId, size_t, size_t) = 0;
    virtual void SetBootstrapComplete() = 0;
    virtual std::optional<std::pair<CNetMessage, bool>> PollMessage(NodeId) = 0;
    virtual bool SetTryNewOutboundPeer(bool) = 0;
    virtual void StartExtraBlockRelayPeers() = 0;
    virtual std::optional<CService> GetLocalAddrForPeer(NodeId, const CService&) = 0;
    virtual bool SeenLocal(const CService& addr) = 0;
};

} // namespace interfaces

#endif // BITCOIN_INTERFACES_NET_H
