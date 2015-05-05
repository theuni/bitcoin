// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ZMQPORTS_H
#define BITCOIN_ZMQPORTS_H

#include "validationinterface.h"
#include <string>

class CZMQPublisher : public CValidationInterface
{
public:
    enum Format
    {
        HashFormat,
        NetworkFormat
    };
  
    CZMQPublisher();
    virtual ~CZMQPublisher();

    void Initialize(const std::string &endpoint, Format format);
    void Shutdown();

protected: // CValidationInterface
    void SyncTransaction(const CTransaction &tx, const CBlock *pblock);
    void UpdatedBlockTip(const CBlock &block);

private:
    int Publish(const void* data, size_t size, int flags);

    void *pcontext;
    void *psocket;
    Format format;
};

#endif // BITCOIN_ZMQPORTS_H
