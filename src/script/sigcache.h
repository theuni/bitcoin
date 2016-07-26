// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGCACHE_H
#define BITCOIN_SCRIPT_SIGCACHE_H

#include "script/interpreter.h"

#include <vector>
#include <boost/thread.hpp>
#include <boost/unordered_set.hpp>

// DoS prevention: limit cache size to less than 40MB (over 500000
// entries on 64-bit systems).
static const unsigned int DEFAULT_MAX_SIG_CACHE_SIZE = 40;

class CPubKey;

class CPubKey;

/**
 * We're hashing a nonce into the entries themselves, so we don't need extra
 * blinding in the set hash computation.
 */
class CSignatureCacheHasher
{
public:
    size_t operator()(const uint256& key) const;
};

/**
 * Valid signature cache, to avoid doing expensive ECDSA signature checking
 * twice for every transaction (once when accepted into memory pool, and
 * again when accepted into the block chain)
 */
class CSignatureCache
{
public:
    CSignatureCache();
    void ComputeEntry(uint256& entry, const uint256 &hash, const std::vector<unsigned char>& vchSig, const CPubKey& pubkey) const;
    bool Get(const uint256& entry) const;
    void Erase(const uint256& entry);
    void Set(const uint256& entry);
private:
     //! Entries are SHA256(nonce || signature hash || public key || signature):
    uint256 nonce;
    typedef boost::unordered_set<uint256, CSignatureCacheHasher> map_type;
    map_type setValid;
    mutable boost::shared_mutex cs_sigcache;
};

class CachingTransactionSignatureChecker : public TransactionSignatureChecker
{
private:
    bool store;
    CSignatureCache& signatureCache;
public:
    CachingTransactionSignatureChecker(CSignatureCache& signatureCacheIn, const CTransaction* txToIn, unsigned int nInIn, const CAmount& amount, bool storeIn) : TransactionSignatureChecker(txToIn, nInIn, amount), store(storeIn), signatureCache(signatureCacheIn) {}

    bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;
};

#endif // BITCOIN_SCRIPT_SIGCACHE_H
