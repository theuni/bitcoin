// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <utxohashset.h>
#include <algorithm>

bool UTXOHashSet::insert(const uint256& hash)
{
    return m_hashes.insert(hash).second;
}

bool UTXOHashSet::erase(const uint256& hash)
{
    return m_hashes.erase(hash);
}

bool UTXOHashSet::count(const uint256& hash) const
{
    return m_hashes.count(hash);
}

size_t UTXOHashSet::count(const std::vector<uint256>& hashes) const
{
    size_t ret = 0;
    for (const auto& hash : hashes) {
        ret += m_hashes.count(hash);
    }
    return ret;
}

bool UTXOHashSet::try_insert(std::vector<uint256> hashes)
{
    for (auto&& hash : hashes) {
        if (m_hashes.count(hash)) {
            return false;
        }
    }
    for (auto&& hash : hashes) {
        m_hashes.insert(std::move(hash));
    }
    return true;
}

void UTXOHashSet::insert(std::vector<uint256> hashes)
{
    for (auto&& hash : hashes) {
        m_hashes.insert(std::move(hash));
    }
}

bool UTXOHashSet::try_erase(std::vector<uint256> hashes)
{
    std::vector<const_iterator> iterators;
    iterators.reserve(hashes.size());
    for (auto&& hash : hashes) {
        const_iterator it(m_hashes.find(hash));
        if (it == m_hashes.end()) {
            return false;
        }
        iterators.push_back(it);
    }
    for (auto&& it : iterators) {
        m_hashes.erase(it);
    }
    return true;
}

void UTXOHashSet::erase(std::vector<uint256> hashes)
{
    for (auto&& hash : hashes) {
        m_hashes.erase(hash);
    }
}

size_t UTXOHashSet::size() const
{
    return m_hashes.size();
}
