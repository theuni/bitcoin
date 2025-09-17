// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INTERFACES_NET_H
#define BITCOIN_INTERFACES_NET_H

#include <interfaces/nodeid.h>
#include <netaddress.h>
#include <netmessage.h>
#include <node/connection_types.h>
#include <protocol.h>
#include <sync.h>

#include <cstdint>
#include <cstddef>
#include <vector>
#include <optional>

namespace node
{
    struct NodeContext;
}

struct CSerializedNetMsg;
class CAddress;
class CNetMessage;
class CService;
class CConnman;
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
namespace interfaces {
struct NetManagerEvents;
class NetEventsInterface
{
public:
    /** Mutex for anything that is only accessed via the msg processing thread */
    static Mutex g_msgproc_mutex;

    virtual ~NetEventsInterface() = default;

    /** Initialize a peer (setup state) */
    virtual std::optional<NodeId> initializeNode(PeerOptions options) = 0;

    /** Handle removal of a peer (clear state) */
    virtual void markNodeDisconnected(NodeId) = 0;

    virtual void markSendBufferFull(NodeId, bool) = 0;

    /**
     * Callback to determine whether the given set of service flags are sufficient
     * for a peer to be "relevant".
     */
    virtual bool hasAllDesirableServiceFlags(ServiceFlags services) = 0;

    virtual void wakeMessageHandler() = 0;
};

struct PollMessageResult
{
    std::vector<std::byte> m_recv;                   //!< received message data
    int64_t m_time{0}; //!< time of message receipt
    uint32_t m_message_size{0};          //!< size of the payload
    uint32_t m_raw_message_size{0};      //!< used wire size of the message (including header/checksum)
    std::string m_type;
    bool m_more;
};

struct NetManagerEvents
{
    virtual ~NetManagerEvents() = default;
    virtual bool pushMessage(NodeId, CSerializedNetMsg&&) = 0;
    virtual bool disconnectNode(NodeId) = 0;
    virtual bool outboundTargetReached(bool) = 0;
    virtual std::vector<CAddress> getAddresses(size_t, size_t) = 0;
    virtual std::vector<CAddress> getAddressesForPeer(NodeId, size_t, size_t) = 0;
    virtual void setBootstrapComplete() = 0;
    virtual std::optional<PollMessageResult> pollMessage(NodeId) = 0;
    virtual bool setTryNewOutboundPeer(bool) = 0;
    virtual void startExtraBlockRelayPeers() = 0;
    virtual std::optional<CService> getLocalAddrForPeer(NodeId, const CService&) = 0;
    virtual bool seenLocal(const CService& addr) = 0;
};

//! Return implementation of Mining interface.
std::unique_ptr<NetEventsInterface> MakePeerMan(node::NodeContext& node);
std::unique_ptr<NetManagerEvents> MakeConnman(CConnman& node);

} // namespace interfaces

using NetEventsInterface = interfaces::NetEventsInterface;


#endif // BITCOIN_INTERFACES_NET_H
