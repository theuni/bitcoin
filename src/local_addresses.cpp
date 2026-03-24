// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <local_addresses.h>

#include <net.h>

LocalAddressManager g_localaddressman;

[[nodiscard]] std::optional<CService> LocalAddressManager::Get(const CAddress& addr, const Network& network) const
{
    if (!fListen) return std::nullopt;

    std::optional<CService> ret;
    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(g_maplocalhost_mutex);
        for (const auto& [local_addr, local_service_info] : mapLocalHost) {
            // For privacy reasons, don't advertise our privacy-network address
            // to other networks and don't advertise our other-network address
            // to privacy networks.
            if (local_addr.GetNetwork() != network
                && (local_addr.IsPrivacyNet() || addr.IsPrivacyNet())) {
                continue;
            }
            const int nScore{local_service_info.nScore};
            const int nReachability{local_addr.GetReachabilityFrom(addr)};
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore)) {
                ret.emplace(CService{local_addr, local_service_info.nPort});
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return ret;
}

void LocalAddressManager::Clear()
{
    LOCK(g_maplocalhost_mutex);
    return mapLocalHost.clear();
}

bool LocalAddressManager::Add(const CService& addr_, int nScore)
{
    CService addr{MaybeFlipIPv6toCJDNS(addr_)};

    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (!g_reachable_nets.Contains(addr))
        return false;

    LogInfo("AddLocal(%s,%i)\n", addr.ToStringAddrPort(), nScore);

    {
        LOCK(g_maplocalhost_mutex);
        const auto [it, is_newly_added] = mapLocalHost.emplace(addr, LocalServiceInfo());
        LocalServiceInfo &info = it->second;
        if (is_newly_added || nScore >= info.nScore) {
            info.nScore = nScore + (is_newly_added ? 0 : 1);
            info.nPort = addr.GetPort();
        }
    }

    return true;
}

void LocalAddressManager::Remove(const CService& addr)
{
    LOCK(g_maplocalhost_mutex);
    LogInfo("RemoveLocal(%s)\n", addr.ToStringAddrPort());
    mapLocalHost.erase(addr);
}

bool LocalAddressManager::Seen(const CService& addr)
{
    LOCK(g_maplocalhost_mutex);
    const auto it = mapLocalHost.find(addr);
    if (it == mapLocalHost.end()) return false;
    ++it->second.nScore;
    return true;
}

bool LocalAddressManager::Contains(const CService& addr) const
{
    LOCK(g_maplocalhost_mutex);
    return mapLocalHost.contains(addr);
}

int LocalAddressManager::GetnScore(const CService& addr) const
{
    LOCK(g_maplocalhost_mutex);
    const auto it = mapLocalHost.find(addr);
    return (it != mapLocalHost.end()) ? it->second.nScore : 0;
}

LocalAddressManager::map_type LocalAddressManager::GetAll() const
{
    LOCK(g_maplocalhost_mutex);
    return mapLocalHost;
}
