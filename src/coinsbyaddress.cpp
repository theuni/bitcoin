// Copyright (c) 2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinsbyaddress.h"
#include "txdb.h"

#include <assert.h>

CCoinsViewByAddress::CCoinsViewByAddress(CCoinsViewDB* viewIn) : base(viewIn) { }

bool CCoinsViewByAddress::GetCoinsByAddress(const CScript &script, CCoinsByAddress &coins) {
    if (cacheCoinsByAddress.count(script)) {
        coins = cacheCoinsByAddress[script];
        return true;
    }
    if (base->GetCoinsByAddress(script, coins)) {
        cacheCoinsByAddress[script] = coins;
        return true;
    }
    return false;
}

CCoinsMapByAddress::iterator CCoinsViewByAddress::FetchCoinsByAddress(const CScript &script, bool fRequireExisting) {
    CCoinsMapByAddress::iterator it = cacheCoinsByAddress.find(script);
    if (it != cacheCoinsByAddress.end())
        return it;
    CCoinsByAddress tmp;
    if (!base->GetCoinsByAddress(script, tmp))
    {
        if (fRequireExisting)
            return cacheCoinsByAddress.end();
    }
    CCoinsMapByAddress::iterator ret = cacheCoinsByAddress.insert(it, std::make_pair(script, CCoinsByAddress()));
    tmp.swap(ret->second);
    return ret;
}

CCoinsByAddress &CCoinsViewByAddress::GetCoinsByAddress(const CScript &script, bool fRequireExisting) {
    CCoinsMapByAddress::iterator it = FetchCoinsByAddress(script, fRequireExisting);
    assert(it != cacheCoinsByAddress.end());
    return it->second;
}
