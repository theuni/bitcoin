// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NETMESSAGEMAKER_H
#define BITCOIN_NETMESSAGEMAKER_H

#include "protocol.h"
#include "net.h"
#include "serialize.h"

#include <memory>

class CChainParams;
class uint256;

class CNetMsgMaker
{
public:
    template <typename... Args>
    static CSerializedNetMsg Make(const CChainParams& params, std::string sCommand, int nVersionIn, Args&&... args)
    {
        CSerializedNetMsg msg;
        CWriter writer{msg.data};
        ::SerializeMany(writer, SER_NETWORK, nVersionIn, GetHeader(params, sCommand), std::forward<Args>(args)...);
        msg.command = std::move(sCommand);
        EndMessage(msg.data);
        msg.data.shrink_to_fit();
        return msg;
    }

private:
    CNetMsgMaker() = delete;
    struct CWriter
    {
        // Avoids making CNetMsgMaker::write() public
        void write(const char* pch, size_t nSize);
        std::vector<unsigned char>& data;
    };

    static void EndMessage(std::vector<unsigned char>& msg);
    static CMessageHeader GetHeader(const CChainParams& params, const std::string& sCommand);

};


#endif // BITCOIN_NETMESSAGEMAKER_H
