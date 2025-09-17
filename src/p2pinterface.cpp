// Copyright (c) 2025 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include <interfaces/net.h>
#include <net.h>

namespace interfaces {
namespace {

class ConnmanImpl : public interfaces::NetManagerEvents
{
public:
    explicit ConnmanImpl(CConnman& connman) : m_connman(connman) {}

    bool pushMessage(NodeId node_id, CSerializedNetMsg&& msg) override
    {
        return m_connman.pushMessage(node_id, std::move(msg));
    }

    bool disconnectNode(NodeId node_id) override
    {
        return m_connman.disconnectNode(node_id);
    }

    bool outboundTargetReached(bool historicalBlockServingLimit) override
    {
        return m_connman.outboundTargetReached(historicalBlockServingLimit);
    }

    std::vector<CAddress> getAddresses(size_t max_addresses, size_t max_pct) override
    {
        return m_connman.getAddresses(max_addresses, max_pct);
    }

    std::vector<CAddress> getAddressesForPeer(NodeId node_id, size_t max_addresses, size_t max_pct) override
    {
        return m_connman.getAddressesForPeer(node_id, max_addresses, max_pct);
    }

    void setBootstrapComplete() override
    {
        return m_connman.setBootstrapComplete();
    }

    std::optional<interfaces::PollMessageResult> pollMessage(NodeId node_id) override
    {
        return m_connman.pollMessage(node_id);
    }

    bool setTryNewOutboundPeer(bool flag) override
    {
        return m_connman.setTryNewOutboundPeer(flag);
    }

    void startExtraBlockRelayPeers() override
    {
        return m_connman.startExtraBlockRelayPeers();
    }

    std::optional<CService> getLocalAddrForPeer(NodeId node_id, const CService& addr_local) override
    {
        return m_connman.getLocalAddrForPeer(node_id, addr_local);
    }

    bool seenLocal(const CService& addr) override
    {
        return m_connman.seenLocal(addr);
    }

private:
    CConnman& m_connman;

};

} // namespace

std::unique_ptr<NetManagerEvents> MakeConnman(CConnman& connman) { return std::make_unique<ConnmanImpl>(connman); }

} // namespace interfaces
