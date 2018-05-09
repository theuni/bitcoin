// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTXOHASHSET_H
#define BITCOIN_UTXOHASHSET_H

#include <uint256.h>

#include <unordered_set>

class UTXOHashSet
{
public:
    bool insert(const uint256& hash);
    bool erase(const uint256& hash);
    bool count(const uint256& hash) const;

    size_t count(const std::vector<uint256>& hashes) const;
    void insert(std::vector<uint256> hashes);
    bool try_insert(std::vector<uint256> hashes);
    void erase(std::vector<uint256> hashes);
    bool try_erase(std::vector<uint256> hashes);

    size_t size() const;
private:
    struct Hasher
    {
        std::size_t operator()(const uint256& hash) const
        {
            return hash.GetCheapHash();
        }
    };
    using set_type = std::unordered_set<uint256, Hasher>;
    using const_iterator = typename set_type::const_iterator;
    using iterator = typename set_type::iterator;
    set_type m_hashes;
};

#endif // BITCOIN_UTXOHASHSET_H
