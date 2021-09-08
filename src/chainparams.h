// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include <kernel/chainparams.h>

#include <netaddress.h>
#include <protocol.h>

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Bitcoin system.
 */
class CChainParams : public KernelCChainParams
{
public:
    using KernelCChainParams::KernelCChainParams;

    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    uint16_t GetDefaultPort() const { return nDefaultPort; }
    uint16_t GetDefaultPort(Network net) const
    {
        return net == NET_I2P ? I2P_SAM31_PORT : GetDefaultPort();
    }
    uint16_t GetDefaultPort(const std::string& addr) const
    {
        CNetAddr a;
        return a.SetSpecial(addr) ? GetDefaultPort(a.GetNetwork()) : GetDefaultPort();
    }
protected:
    CMessageHeader::MessageStartChars pchMessageStart;
    uint16_t nDefaultPort;
};

#endif // BITCOIN_CHAINPARAMS_H
