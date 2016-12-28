// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include "messageinterface.h"
#include "validationinterface.h"
#include "threadinterrupt.h"
#include "protocol.h"
#include "net.h"

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <deque>

namespace Consensus
{
    struct Params;
}
class CChainParams;
class CDataStream;

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

struct CProcessQueue
{
    CProcessQueue(CNode* pnodeIn) : pnode(pnodeIn){}
    CNode* pnode;
    std::deque<CInv> vRecvGetData;
    std::list<CNetMessage> vRecvMsg;
    std::mutex cs_vRecvMsg;
};

class CMessageProcessor final : public CMessageProcessorInterface
{
public:
    CMessageProcessor(CConnman& connmanIn);
    ~CMessageProcessor() final = default;
protected:
    void OnStartup() final;
    void OnShutdown() final;
    void OnInterrupt() final;
    bool OnNewMessages(NodeId id, std::list<CNetMessage>&& messages) final;
    void InitializeNode(CNode *pnode) final;
    void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) final;
private:
    bool ProcessMessages(CProcessQueue* pfrom, CNetMessage& msg);
    bool SendMessages(CProcessQueue* pto);
    void ProcessGetData(CProcessQueue* pfrom, const Consensus::Params& consensusParams);
    bool ProcessMessage(CProcessQueue* pfrom, std::string strCommand, CDataStream& vRecv, int64_t nTimeReceived, const CChainParams& chainparams);
    void Run();
    CConnman& connman;
    std::map<NodeId, std::shared_ptr<CProcessQueue>> mapNodes;
    std::mutex mapNodesMutex;
    std::mutex mutexNetProcessing;
    std::condition_variable condNetProcessing;
    std::atomic<bool> interruptNetProcessing;
    std::thread processThread;
    bool fSleep;
};

#endif // BITCOIN_NET_PROCESSING_H
