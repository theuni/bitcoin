// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include "messageinterface.h"
#include "validationinterface.h"

#include <string>
#include <atomic>

class CDataStream;
class CChainParams;
namespace Consensus
{
    struct Params;
}

class PeerLogicValidation : public CValidationInterface {
private:
    CConnman* connman;

public:
    PeerLogicValidation(CConnman* connmanIn);

    virtual void SyncTransaction(const CTransaction& tx, const CBlockIndex* pindex, int nPosInBlock);
    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload);
    virtual void BlockChecked(const CBlock& block, const CValidationState& state);
};

struct CNodeStateStats {
    int nMisbehavior;
    int nSyncHeight;
    int nCommonHeight;
    std::vector<int> vHeightInFlight;
};

/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);
/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch);

class CMessageProcessor final : public CMessageProcessorInterface
{
public:
    CMessageProcessor(CConnman& connmanIn);
    ~CMessageProcessor() final = default;
protected:
    void OnStartup() final;
    void OnShutdown() final;
    void OnInterrupt() final;
    bool ProcessMessages(CNode* pfrom) final;
    bool SendMessages(CNode* pto) final;
    void InitializeNode(CNode *pnode) final;
    void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) final;
private:
    bool ProcessMessage(CNode* pfrom, std::string strCommand, CDataStream& vRecv, int64_t nTimeReceived, const CChainParams& chainparams);
    void ProcessGetData(CNode* pfrom, const Consensus::Params& consensusParams);

    CConnman& connman;
    std::atomic_flag interruptNetProcessing = ATOMIC_FLAG_INIT;
};

#endif // BITCOIN_NET_PROCESSING_H
