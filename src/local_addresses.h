// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LOCAL_ADDRESSES_H
#define BITCOIN_LOCAL_ADDRESSES_H

#include <netaddress.h>
#include <sync.h>

class CNode;

struct LocalServiceInfo {
    int nScore;
    uint16_t nPort;
};

class LocalAddresses
{
    mutable Mutex m_mutex;
    using map_type = std::map<CNetAddr, LocalServiceInfo>;
    map_type m_addresses GUARDED_BY(m_mutex);

public:

    // learn a new local address
    bool Add(const CService& addr_, int nScore) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    void Remove(const CService& addr) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    /** vote for a local address */
    bool Seen(const CService& addr) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    /** check whether a given address is potentially local */
    bool Contains(const CService& addr) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    // Determine the "best" local address for a particular peer.
    [[nodiscard]] std::optional<CService> Get(const CNode& peer) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    void Clear() EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    int GetnScore(const CService& addr) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
    map_type GetHosts() const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);
};

#endif // BITCOIN_LOCAL_ADDRESSES_H
