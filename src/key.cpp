// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "crypto/sha2.h"
#include "random.h"
#include "extkey.h"

bool CKey::Check(const unsigned char *vch) {
    return ::KeyCheck(vch);
}

bool CKey::CheckSignatureElement(const unsigned char *vch, int len, bool half) {
    return ::KeyCheckSignatureElement(vch, len, half);
}

void CKey::MakeNewKey(bool fCompressedIn) {
    do {
        GetRandBytes(vch, sizeof(vch));
    } while (!::KeyCheck(vch));
    fValid = true;
    fCompressed = fCompressedIn;
}

CPrivKey CKey::GetPrivKey() const {
    assert(fValid);
    int size = ::GetPrivKeySize(vch, fCompressed);
    CPrivKey ret(size);
    ::GetPrivKey(vch, fCompressed, &ret[0]);
    return ret;
}

CPubKey CKey::GetPubKey() const {
    unsigned char buf[65];
    int keysize = ::GetPubKey(vch, fCompressed, &buf[0]);
    return CPubKey(&buf[0], &buf[0] + keysize);
}

bool CKey::Sign(const uint256 &hash, std::vector<unsigned char>& vchSig, bool lowS) const {
    if (!fValid)
        return false;
  return ::Sign(vch, hash, vchSig, lowS);
}

bool CKey::SignCompact(const uint256 &hash, std::vector<unsigned char>& vchSig) const {
    if (!fValid)
        return false;

    return ::SignCompact(vch, fCompressed, hash, vchSig);
}

bool CKey::Load(CPrivKey &privkey, CPubKey &vchPubKey, bool fSkipCheck=false) {
    ::SetPrivKey(&privkey[0], privkey.size(), &vch[0]);
    fCompressed = vchPubKey.IsCompressed();
    fValid = true;
    if (GetPubKey() != vchPubKey)
        return false;

    return true;
}

bool CPubKey::Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) const {
  return ::Verify(vch, size(), hash, vchSig);
}

bool CPubKey::RecoverCompact(const uint256 &hash, const std::vector<unsigned char>& vchSig) {

    if (vchSig.size() != 65)
        return false;

    return ::RecoverCompact(vch, size(), hash, vchSig);

}

bool CPubKey::IsFullyValid() const {
    return ::IsFullyValid(vch, size());
}

bool CPubKey::Decompress() {
    if (!IsValid())
        return false;
  return ::Decompress(vch, size());
}

void static BIP32Hash(const unsigned char chainCode[32], unsigned int nChild, unsigned char header, const unsigned char data[32], unsigned char output[64]) {
    unsigned char num[4];
    num[0] = (nChild >> 24) & 0xFF;
    num[1] = (nChild >> 16) & 0xFF;
    num[2] = (nChild >>  8) & 0xFF;
    num[3] = (nChild >>  0) & 0xFF;
    CHMAC_SHA512(chainCode, 32).Write(&header, 1)
                               .Write(data, 32)
                               .Write(num, 4)
                               .Finalize(output);
}

bool CKey::Derive(CKey& keyChild, unsigned char ccChild[32], unsigned int nChild, const unsigned char cc[32]) const {
    assert(IsValid());
    assert(IsCompressed());
    unsigned char out[64];
    LockObject(out);
    if ((nChild >> 31) == 0) {
        CPubKey pubkey = GetPubKey();
        assert(pubkey.begin() + 33 == pubkey.end());
        BIP32Hash(cc, nChild, *pubkey.begin(), pubkey.begin()+1, out);
    } else {
        assert(begin() + 32 == end());
        BIP32Hash(cc, nChild, 0, begin(), out);
    }
    memcpy(ccChild, out+32, 32);
    bool ret = ::TweakSecret((unsigned char*)keyChild.begin(), begin(), out);

    UnlockObject(out);
    keyChild.fCompressed = true;
    keyChild.fValid = ret;
    return ret;
}

bool CPubKey::Derive(CPubKey& pubkeyChild, unsigned char ccChild[32], unsigned int nChild, const unsigned char cc[32]) const {
    assert(IsValid());
    assert((nChild >> 31) == 0);
    assert(begin() + 33 == end());
    unsigned char out[64];
    BIP32Hash(cc, nChild, *begin(), begin()+1, out);
    memcpy(ccChild, out+32, 32);
    return ::TweakPublic(vch, size(), out, pubkeyChild.vch);
}

bool CExtKey::Derive(CExtKey &out, unsigned int nChild) const {
    out.nDepth = nDepth + 1;
    CKeyID id = key.GetPubKey().GetID();
    memcpy(&out.vchFingerprint[0], &id, 4);
    out.nChild = nChild;
    return key.Derive(out.key, out.vchChainCode, nChild, vchChainCode);
}

void CExtKey::SetMaster(const unsigned char *seed, unsigned int nSeedLen) {
    static const unsigned char hashkey[] = {'B','i','t','c','o','i','n',' ','s','e','e','d'};
    unsigned char out[64];
    LockObject(out);
    CHMAC_SHA512(hashkey, sizeof(hashkey)).Write(seed, nSeedLen).Finalize(out);
    key.Set(&out[0], &out[32], true);
    memcpy(vchChainCode, &out[32], 32);
    UnlockObject(out);
    nDepth = 0;
    nChild = 0;
    memset(vchFingerprint, 0, sizeof(vchFingerprint));
}

CExtPubKey CExtKey::Neuter() const {
    CExtPubKey ret;
    ret.nDepth = nDepth;
    memcpy(&ret.vchFingerprint[0], &vchFingerprint[0], 4);
    ret.nChild = nChild;
    ret.pubkey = key.GetPubKey();
    memcpy(&ret.vchChainCode[0], &vchChainCode[0], 32);
    return ret;
}

void CExtKey::Encode(unsigned char code[74]) const {
    code[0] = nDepth;
    memcpy(code+1, vchFingerprint, 4);
    code[5] = (nChild >> 24) & 0xFF; code[6] = (nChild >> 16) & 0xFF;
    code[7] = (nChild >>  8) & 0xFF; code[8] = (nChild >>  0) & 0xFF;
    memcpy(code+9, vchChainCode, 32);
    code[41] = 0;
    assert(key.size() == 32);
    memcpy(code+42, key.begin(), 32);
}

void CExtKey::Decode(const unsigned char code[74]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code+1, 4);
    nChild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(vchChainCode, code+9, 32);
    key.Set(code+42, code+74, true);
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
