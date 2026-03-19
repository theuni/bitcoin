// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <local_addresses.h>

#include <net.h>

bool LocalAddresses::Add(const CService& addr_, int nScore)
{
    CService addr{MaybeFlipIPv6toCJDNS(addr_)};

    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (!g_reachable_nets.Contains(addr))
        return false;

    LogInfo("Add Local Address(%s,%i)\n", addr.ToStringAddrPort(), nScore);

    LOCK(m_mutex);
    const auto [it, is_newly_added] = m_addresses.emplace(addr, LocalServiceInfo());
    LocalServiceInfo &info = it->second;
    if (is_newly_added || nScore >= info.nScore) {
        info.nScore = nScore + (is_newly_added ? 0 : 1);
        info.nPort = addr.GetPort();
    }
    return true;
}

void LocalAddresses::Remove(const CService& addr)
{
    LogInfo("Remove Local Address(%s)\n", addr.ToStringAddrPort());
    LOCK(m_mutex);
    m_addresses.erase(addr);
}

/** vote for a local address */
bool LocalAddresses::Seen(const CService& addr)
{
    LOCK(m_mutex);
    const auto it = m_addresses.find(addr);
    if (it == m_addresses.end()) return false;
    ++it->second.nScore;
    return true;
}


/** check whether a given address is potentially local */
bool LocalAddresses::Contains(const CService& addr) const
{
    LOCK(m_mutex);
    return m_addresses.contains(addr);
}

// Determine the "best" local address for a particular peer.
[[nodiscard]] std::optional<CService> LocalAddresses::Get(const CNode& peer) const
{
    if (!fListen) return std::nullopt;

    std::optional<CService> addr;
    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(m_mutex);
        for (const auto& [local_addr, local_service_info] : m_addresses) {
            // For privacy reasons, don't advertise our privacy-network address
            // to other networks and don't advertise our other-network address
            // to privacy networks.
            if (local_addr.GetNetwork() != peer.ConnectedThroughNetwork()
                && (local_addr.IsPrivacyNet() || peer.IsConnectedThroughPrivacyNet())) {
                continue;
            }
            const int nScore{local_service_info.nScore};
            const int nReachability{local_addr.GetReachabilityFrom(peer.addr)};
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore)) {
                addr.emplace(CService{local_addr, local_service_info.nPort});
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return addr;
}
void LocalAddresses::Clear()
{
    LOCK(m_mutex);
    return m_addresses.clear();
}

int LocalAddresses::GetnScore(const CService& addr) const
{
    LOCK(m_mutex);
    const auto it = m_addresses.find(addr);
    return (it != m_addresses.end()) ? it->second.nScore : 0;
}

LocalAddresses::map_type LocalAddresses::GetHosts() const
{
    LOCK(m_mutex);
    return m_addresses;
}
