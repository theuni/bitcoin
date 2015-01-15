// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/consensus.h"

#include "primitives/transaction.h"
#include "version.h"

void Consensus::CheckTransaction(const CTransaction& tx)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        throw ConsensusException(10, __func__, "vin empty", "bad-txns-vin-empty");
    if (tx.vout.empty())
        throw ConsensusException(10, __func__, "vout empty", "bad-txns-vout-empty");
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        throw ConsensusException(100, __func__, "size limits failed", "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        if (tx.vout[i].nValue < 0)
            throw ConsensusException(100, __func__, "txout.nValue negative", "bad-txns-vout-negative");
        if (tx.vout[i].nValue > MAX_MONEY)
            throw ConsensusException(100, __func__, "txout.nValue too high", "bad-txns-vout-toolarge");
        nValueOut += tx.vout[i].nValue;
        if (!MoneyRange(nValueOut))
            throw ConsensusException(100, __func__, "txout total out of range", "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs
    std::set<COutPoint> vInOutPoints;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        if (vInOutPoints.count(tx.vin[i].prevout))
            throw ConsensusException(100, __func__, "duplicate inputs", "bad-txns-inputs-duplicate");
        vInOutPoints.insert(tx.vin[i].prevout);
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            throw ConsensusException(100, __func__, "coinbase script size", "bad-cb-length");
    }
    else
    {
        for (unsigned int i = 0; i < tx.vin.size(); i++)
            if (tx.vin[i].prevout.IsNull())
                throw ConsensusException(10, __func__, "prevout is null", "bad-txns-prevout-null");
    }
}
