// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>

#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>


std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

CPrevOutType::CPrevOutType(CTxOut txout, uint32_t height, bool coinbase)
{
    Update(std::move(txout), height, coinbase);
}

std::string CPrevOutType::ToString() const
{
    return strprintf("nValue=%d.%08d. type=%d, flags=%d", get_amount() / COIN, get_amount() % COIN, get_type(), get_flags());
}

void CPrevOutType::Update(CTxOut txout, uint32_t height, bool coinbase)
{
    template_type type(type_full);
    flag_type flags(flag_none);

    CScript& pub(txout.scriptPubKey);

    int witness_version = 0;
    std::vector<unsigned char> prog;
    if (pub.IsWitnessProgram(witness_version, prog)) {
        if (pub.IsPayToWitnessPubkeyHash()) {
            flags = flag_type(flags | CPrevOutType::flag_witness);
            type = CPrevOutType::type_p2pkh;
            pub.clear();
        } else if (pub.IsPayToWitnessScriptHash()) {
            flags = flag_type(flags | CPrevOutType::flag_witness | CPrevOutType::flag_height);
            type = CPrevOutType::type_p2sh;
            pub.clear();
        }
    } else {
        if (pub.IsPayToPubkeyHash()) {
            type = CPrevOutType::type_p2pkh;
            pub.clear();
        } else if (pub.IsPayToScriptHash()) {
            type = CPrevOutType::type_p2sh;
            flags = flag_type(flags | CPrevOutType::flag_height);
            pub.clear();
        }
    }

    if (coinbase) {
        flags = flag_type(flags | CPrevOutType::flag_coinbase | CPrevOutType::flag_height);
    }
    if (type == type_full) {
        flags = flag_type(flags | CPrevOutType::flag_height);
    }
    m_amount = txout.nValue;
    m_type = type | flags;
    m_script = std::move(pub);

    if (flags & CPrevOutType::flag_height) {
        m_height = height;
    } else {
        m_height = 0;
    }
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != SEQUENCE_FINAL)
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CScript CTxIn::GetPrevOutScript() const
{
    CScript ret;
    if (fullPrev.get_type() == CPrevOutType::type_full) {
        return fullPrev.get_prev_script();
    }

    if (!scriptSig.IsPushOnly())
        return ret;

    if (fullPrev.get_flags(CPrevOutType::flag_witness)) {
        switch(fullPrev.get_type()) {
            case CPrevOutType::type_p2pkh:
            {
                const auto& prog = scriptWitness.stack.back();
                uint160 scriptHash = Hash160(prog.begin(), prog.end());
                ret = CScript() << OP_0 << ToByteVector(scriptHash);
            }
            break;
            case CPrevOutType::type_p2sh:
            {
                const auto& prog = scriptWitness.stack.back();
                uint256 scriptHash;
                CSHA256().Write(prog.data(), prog.size()).Finalize(scriptHash.begin());
                ret = CScript() << OP_0 << ToByteVector(scriptHash);
            }
            default: break;
        }
    } else {
        switch(fullPrev.get_type()) {
            case CPrevOutType::type_p2pkh:
            {
                opcodetype op;
                std::vector<unsigned char> pubkey;
                auto it = scriptSig.begin();
                while (it != scriptSig.end()) {
                    scriptSig.GetOp(it, op, pubkey);
                }
                if (!pubkey.empty()) {
                    uint160 pubkeyHash = Hash160(pubkey);
                    ret = CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkeyHash) << OP_EQUALVERIFY << OP_CHECKSIG;
                }
            }
            break;
            case CPrevOutType::type_p2sh:
            {
                opcodetype op;
                std::vector<unsigned char> redeem;
                auto it = scriptSig.begin();
                while (it != scriptSig.end()) {
                    scriptSig.GetOp(it, op, redeem);
                }
                uint160 scriptHash = Hash160(redeem);
                ret = CScript() << OP_HASH160 << ToByteVector(scriptHash) << OP_EQUAL;
            }
            default: break;
        }
    }
    return ret;
}

uint256 CTxIn::GetPrevOutHash() const
{
    return SerializeHash(0, prevout, fullPrev);
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(SERIALIZE_TRANSACTION_NO_WITNESS, *this);
}

uint256 CTransaction::ComputeHash() const
{
    return SerializeHash(SERIALIZE_TRANSACTION_NO_WITNESS, *this);
}

uint256 CTransaction::GetWitnessHash() const
{
    if (!HasWitness()) {
        return GetHash();
    }
    return SerializeHash(0, *this);
}

/* For backward compatibility, the hash is initialized to 0. TODO: remove the need for this default constructor entirely. */
CTransaction::CTransaction() : vin(), vout(), nVersion(CTransaction::CURRENT_VERSION), nLockTime(0), hash() {}
CTransaction::CTransaction(const CMutableTransaction &tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime), hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx) : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion), nLockTime(tx.nLockTime), hash(ComputeHash()) {}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (const auto& tx_out : vout) {
        nValueOut += tx_out.nValue;
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nValueOut;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (const auto& tx_in : vin)
        str += "    " + tx_in.ToString() + "\n";
    for (const auto& tx_in : vin)
        str += "    " + tx_in.scriptWitness.ToString() + "\n";
    for (const auto& tx_out : vout)
        str += "    " + tx_out.ToString() + "\n";
    return str;
}
