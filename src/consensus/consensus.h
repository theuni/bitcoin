// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_H
#define BITCOIN_CONSENSUS_H

#include <string>

class CTransaction;
class CValidationState;

/** The maximum allowed size for a serialized block, in bytes (network rule) */
static const unsigned int MAX_BLOCK_SIZE = 1000000;

class ConsensusException: public std::exception
{
public:
    int nDoS;
    std::string reason;
    std::string description;

    ConsensusException(int nDoSIn, std::string functionIn, std::string strErrorIn, std::string reasonIn)
    {
        nDoS = nDoSIn;
        reason = reasonIn;
        description = functionIn + ": " + strErrorIn;
    }
    virtual const char* what() const throw()
    {
        return description.c_str();
    }
    virtual ~ConsensusException() throw() {}
};

namespace Consensus {

void CheckTransaction(const CTransaction& tx);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_H
