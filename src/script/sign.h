// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGN_H
#define BITCOIN_SCRIPT_SIGN_H

#include "script/interpreter.h"

class CKeyID;
class CKeyStore;
class CScript;
class CTransaction;
class CScriptID;

struct CMutableTransaction;

class BaseSignatureCreator {
public:
    virtual ~BaseSignatureCreator() {}
    virtual const BaseSignatureChecker& Checker() const =0;
    virtual bool CreateSig(std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode) const =0;
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const = 0;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const = 0;
};

class SignatureCreator : public BaseSignatureCreator {
    const CKeyStore& keystore;
    const CTransaction& txTo;
    unsigned int nIn;
    int nHashType;
    const SignatureChecker checker;

public:
    SignatureCreator(const CKeyStore& keystoreIn, const CTransaction& txToIn, unsigned int nInIn, int nHashTypeIn=SIGHASH_ALL);
    const BaseSignatureChecker& Checker() const { return checker; }
    bool CreateSig(std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode) const;
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const;
};

bool ProduceSignature(const BaseSignatureCreator& creator, const CScript& scriptPubKey, CScript& scriptSig);

bool SignSignature(const CKeyStore& keystore, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL);
bool SignSignature(const CKeyStore& keystore, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL);

// Given two sets of signatures for scriptPubKey, possibly with OP_0 placeholders,
// combine them intelligently and return the result.
CScript CombineSignatures(CScript scriptPubKey, const BaseSignatureChecker& checker, const CScript& scriptSig1, const CScript& scriptSig2);
CScript CombineSignatures(CScript scriptPubKey, const CTransaction& txTo, unsigned int nIn, const CScript& scriptSig1, const CScript& scriptSig2);

#endif // BITCOIN_SCRIPT_SIGN_H
