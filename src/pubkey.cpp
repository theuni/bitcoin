// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pubkey.h"

#include "hash.h"

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#ifdef USE_SECP256K1
#include <secp256k1.h>
#else
#include "keywrapper.h"
#endif

namespace {

int CompareBigEndian(const unsigned char *c1, size_t c1len, const unsigned char *c2, size_t c2len) {
    while (c1len > c2len) {
        if (*c1)
            return 1;
        c1++;
        c1len--;
    }
    while (c2len > c1len) {
        if (*c2)
            return -1;
        c2++;
        c2len--;
    }
    while (c1len > 0) {
        if (*c1 > *c2)
            return 1;
        if (*c2 > *c1)
            return -1;
        c1++;
        c2++;
        c1len--;
    }
    return 0;
}

// Order of secp256k1's generator minus 1.
const unsigned char vchMaxModOrder[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
    0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40
};

// Half of the order of secp256k1's generator minus 1.
const unsigned char vchMaxModHalfOrder[32] = {
    0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x5D,0x57,0x6E,0x73,0x57,0xA4,0x50,0x1D,
    0xDF,0xE9,0x2F,0x46,0x68,0x1B,0x20,0xA0
};

const unsigned char vchZero[1] = {0};

} // anon namespace

bool CPubKey::Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) const {
    if (!IsValid())
        return false;
#ifdef USE_SECP256K1
    if (secp256k1_ecdsa_verify((const unsigned char*)&hash, 32, &vchSig[0], vchSig.size(), begin(), size()) != 1)
        return false;
#else
    CECKey key;
    if (!key.SetPubKey(*this))
        return false;
    if (!key.Verify(hash, vchSig))
        return false;
#endif
    return true;
}

bool CPubKey::RecoverCompact(const uint256 &hash, const std::vector<unsigned char>& vchSig) {
    if (vchSig.size() != 65)
        return false;
    int recid = (vchSig[0] - 27) & 3;
    bool fComp = ((vchSig[0] - 27) & 4) != 0;
#ifdef USE_SECP256K1
    int pubkeylen = 65;
    if (!secp256k1_ecdsa_recover_compact((const unsigned char*)&hash, 32, &vchSig[1], (unsigned char*)begin(), &pubkeylen, fComp, recid))
        return false;
    assert((int)size() == pubkeylen);
#else
    CECKey key;
    if (!key.Recover(hash, &vchSig[1], recid))
        return false;
    key.GetPubKey(*this, fComp);
#endif
    return true;
}

bool CPubKey::IsFullyValid() const {
    if (!IsValid())
        return false;
#ifdef USE_SECP256K1
    if (!secp256k1_ecdsa_pubkey_verify(begin(), size()))
        return false;
#else
    CECKey key;
    if (!key.SetPubKey(*this))
        return false;
#endif
    return true;
}

bool CPubKey::Decompress() {
    if (!IsValid())
        return false;
#ifdef USE_SECP256K1
    int clen = size();
    int ret = secp256k1_ecdsa_pubkey_decompress((unsigned char*)begin(), &clen);
    assert(ret);
    assert(clen == (int)size());
#else
    CECKey key;
    if (!key.SetPubKey(*this))
        return false;
    key.GetPubKey(*this, false);
#endif
    return true;
}

bool CPubKey::Derive(CPubKey& pubkeyChild, unsigned char ccChild[32], unsigned int nChild, const unsigned char cc[32]) const {
    assert(IsValid());
    assert((nChild >> 31) == 0);
    assert(begin() + 33 == end());
    unsigned char out[64];
    BIP32Hash(cc, nChild, *begin(), begin()+1, out);
    memcpy(ccChild, out+32, 32);
#ifdef USE_SECP256K1
    pubkeyChild = *this;
    bool ret = secp256k1_ecdsa_pubkey_tweak_add((unsigned char*)pubkeyChild.begin(), pubkeyChild.size(), out);
#else
    CECKey key;
    bool ret = key.SetPubKey(*this);
    ret &= key.TweakPublic(out);
    key.GetPubKey(pubkeyChild, true);
#endif
    return ret;
}

bool CPubKey::Check(const unsigned char *vch) {
    return CompareBigEndian(vch, 32, vchZero, 0) > 0 &&
           CompareBigEndian(vch, 32, vchMaxModOrder, 32) <= 0;
}

bool CPubKey::CheckSignatureElement(const unsigned char *vch, int len, bool half) {
    return CompareBigEndian(vch, len, vchZero, 0) > 0 &&
           CompareBigEndian(vch, len, half ? vchMaxModHalfOrder : vchMaxModOrder, 32) <= 0;
}

void CExtPubKey::Encode(unsigned char code[74]) const {
    code[0] = nDepth;
    memcpy(code+1, vchFingerprint, 4);
    code[5] = (nChild >> 24) & 0xFF; code[6] = (nChild >> 16) & 0xFF;
    code[7] = (nChild >>  8) & 0xFF; code[8] = (nChild >>  0) & 0xFF;
    memcpy(code+9, vchChainCode, 32);
    assert(pubkey.size() == 33);
    memcpy(code+41, pubkey.begin(), 33);
}

void CExtPubKey::Decode(const unsigned char code[74]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code+1, 4);
    nChild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(vchChainCode, code+9, 32);
    pubkey.Set(code+41, code+74);
}

bool CExtPubKey::Derive(CExtPubKey &out, unsigned int nChild) const {
    out.nDepth = nDepth + 1;
    CKeyID id = pubkey.GetID();
    memcpy(&out.vchFingerprint[0], &id, 4);
    out.nChild = nChild;
    return pubkey.Derive(out.pubkey, out.vchChainCode, nChild, vchChainCode);
}
