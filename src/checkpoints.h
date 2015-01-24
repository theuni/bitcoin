// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHECKPOINTS_H
#define BITCOIN_CHECKPOINTS_H

#include "uint256.h"

#include <map>

class CBlockIndex;

/** 
 * Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
class CCheckpoints
{
public:

    typedef std::map<int, uint256> MapCheckpoints;
    struct CCheckpointData {
        MapCheckpoints mapCheckpoints;
        int64_t nTimeLastCheckpoint;
        int64_t nTransactionsLastCheckpoint;
        double fTransactionsPerDay;
    };

    CCheckpoints(const CCheckpointData& checkpointDataIn) : checkpointData(checkpointDataIn){}
    CCheckpoints(){}

    //! Returns true if block passes checkpoint checks
    bool CheckBlock(int nHeight, const uint256& hash) const;

    //! Return conservative estimate of total number of blocks, 0 if unknown
    int GetTotalBlocksEstimate() const ;

    double GuessVerificationProgress(CBlockIndex* pindex, bool fSigchecks = true) const;

    CCheckpointData checkpointData;
    static bool fEnabled;
};

#endif // BITCOIN_CHECKPOINTS_H
