// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netmessagemaker.h"

#include "chainparams.h"
#include "crypto/sha256.h"

void CNetMsgMaker::EndMessage(std::vector<unsigned char>& data)
{
    unsigned int nDataSize = data.size() - CMessageHeader::HEADER_SIZE;
    WriteLE32(data.data() + CMessageHeader::MESSAGE_SIZE_OFFSET, nDataSize);

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(data.data(), data.size()).Finalize(hash);
    memcpy(data.data() + CMessageHeader::CHECKSUM_OFFSET, hash, CMessageHeader::CHECKSUM_SIZE);
}

CMessageHeader CNetMsgMaker::GetHeader(const CChainParams& params, const std::string& sCommand)
{
    return { Params().MessageStart(), sCommand.c_str(), 0 };
}

void CNetMsgMaker::CWriter::write(const char* pch, size_t nSize)
{   
    data.insert(data.end(), reinterpret_cast<const unsigned char*>(pch), reinterpret_cast<const unsigned char*>(pch) + nSize);
}   
