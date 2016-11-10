// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NETMESSAGEMAKER_H
#define BITCOIN_NETMESSAGEMAKER_H

#include "net.h"
#include "serialize.h"

class CNetMsgMaker
{
public:
    CNetMsgMaker(int nVersionIn) : nVersion(nVersionIn){}

    template <typename... Args>
    CSerializedNetMsg Make(int nFlags, std::string sCommand, Args&&... args)
    {
        CSerializedNetMsg msg;
        CVectorWriter writer{SER_NETWORK, nFlags | nVersion, msg.data};
        ::SerializeMany(writer, std::forward<Args>(args)...);
        msg.command = std::move(sCommand);
        msg.data.shrink_to_fit();
        return msg;
    }

    template <typename... Args>
    CSerializedNetMsg Make(std::string sCommand, Args&&... args)
    {
        return Make(0, std::move(sCommand), std::forward<Args>(args)...);
    }

private:
    const int nVersion;
};

#endif // BITCOIN_NETMESSAGEMAKER_H
