// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MESSAGE_INTERFACE_H
#define BITCOIN_MESSAGE_INTERFACE_H

typedef int NodeId;

class CNode;
class CMessageProcessorInterface
{
public:
    virtual ~CMessageProcessorInterface() = default;
    virtual void OnStartup() = 0;
    virtual void OnShutdown() = 0;
    virtual void OnInterrupt() = 0;
    virtual bool ProcessMessages(CNode* pfrom) = 0;
    virtual bool SendMessages(CNode* pto) = 0;
    virtual void InitializeNode(CNode *pnode) = 0;
    virtual void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) = 0;
};

#endif // BITCOIN_MESSAGE_INTERFACE_H
