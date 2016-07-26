// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sigcache.h"

#include "memusage.h"
#include "pubkey.h"
#include "random.h"
#include "uint256.h"
#include "util.h"

size_t CSignatureCacheHasher::operator()(const uint256& key) const {
    return key.GetCheapHash();
}

CSignatureCache::CSignatureCache()
{
    GetRandBytes(nonce.begin(), 32);
}

void CSignatureCache::ComputeEntry(uint256& entry, const uint256 &hash, const std::vector<unsigned char>& vchSig, const CPubKey& pubkey)
{
    CSHA256().Write(nonce.begin(), 32).Write(hash.begin(), 32).Write(&pubkey[0], pubkey.size()).Write(&vchSig[0], vchSig.size()).Finalize(entry.begin());
}

bool CSignatureCache::Get(const uint256& entry)
{
    boost::shared_lock<boost::shared_mutex> lock(cs_sigcache);
    return setValid.count(entry);
}

void CSignatureCache::Erase(const uint256& entry)
{
    boost::unique_lock<boost::shared_mutex> lock(cs_sigcache);
    setValid.erase(entry);
}

void CSignatureCache::Set(const uint256& entry)
{
    size_t nMaxCacheSize = GetArg("-maxsigcachesize", DEFAULT_MAX_SIG_CACHE_SIZE) * ((size_t) 1 << 20);
    if (nMaxCacheSize <= 0) return;

    boost::unique_lock<boost::shared_mutex> lock(cs_sigcache);
    while (memusage::DynamicUsage(setValid) > nMaxCacheSize)
    {
        map_type::size_type s = GetRand(setValid.bucket_count());
        map_type::local_iterator it = setValid.begin(s);
        if (it != setValid.end(s)) {
            setValid.erase(*it);
        }
    }

    setValid.insert(entry);
}

bool CachingTransactionSignatureChecker::VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& pubkey, const uint256& sighash) const
{
    static CSignatureCache signatureCache;

    uint256 entry;
    signatureCache.ComputeEntry(entry, sighash, vchSig, pubkey);

    if (signatureCache.Get(entry)) {
        if (!store) {
            signatureCache.Erase(entry);
        }
        return true;
    }

    if (!TransactionSignatureChecker::VerifySignature(vchSig, pubkey, sighash))
        return false;

    if (store) {
        signatureCache.Set(entry);
    }
    return true;
}
