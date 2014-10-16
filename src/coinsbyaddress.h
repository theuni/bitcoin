// Copyright (c) 2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COINSBYADDRESS_H
#define BITCOIN_COINSBYADDRESS_H

#include "coins.h"
#include "core.h"
#include "serialize.h"

class CCoinsViewDB;
class CScript;

class CCoinsByAddress
{
public:
    // unspent transaction outputs
    std::set<COutPoint> setCoins;

    // empty constructor
    CCoinsByAddress() { }

    bool IsEmpty() const {
        return (setCoins.empty());
    }

    void swap(CCoinsByAddress &to) {
        to.setCoins.swap(setCoins);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(setCoins);
    }
};

typedef std::map<CScript, CCoinsByAddress> CCoinsMapByAddress;

/** Adds a memory cache for coins by address */
class CCoinsViewByAddress
{
private:
    CCoinsViewDB *base;

public:
    CCoinsMapByAddress cacheCoinsByAddress; // accessed also from CCoinsViewDB in txdb.cpp
    CCoinsViewByAddress(CCoinsViewDB* baseIn);

    bool GetCoinsByAddress(const CScript &script, CCoinsByAddress &coins);

    // Return a modifiable reference to a CCoinsByAddress.
    CCoinsByAddress &GetCoinsByAddress(const CScript &script, bool fRequireExisting = true);

private:
    CCoinsMapByAddress::iterator FetchCoinsByAddress(const CScript &script, bool fRequireExisting);
};

#endif // BITCOIN_COINSBYADDRESS_H
