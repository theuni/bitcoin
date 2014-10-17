// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EXTKEY_H
#define BITCOIN_EXTKEY_H

#include <vector>
#include <cstdlib>

class uint256;

bool SignCompact(const unsigned char vch[32], bool compressed, const uint256 &hash, std::vector<unsigned char>& vchSig);
bool Sign(const unsigned char vch[32], const uint256 &hash, std::vector<unsigned char>& vchSig, bool lowS);
int  GetPubKey(const unsigned char vch[32], bool compressed, unsigned char ret[65]);
int  GetPrivKeySize(const unsigned char vch[32], bool compressed);
void GetPrivKey(const unsigned char vch[32], bool compressed, unsigned char* ret);
bool SetPrivKey(const unsigned char*  privkey, size_t size, unsigned char ret[32]);


bool Verify(const unsigned char *vch, unsigned int size, const uint256 &hash, const std::vector<unsigned char>& vchSig);
bool RecoverCompact(unsigned char* vch, unsigned int size, const uint256 &hash, const std::vector<unsigned char>& vchSig);
bool IsFullyValid(const unsigned char* vch, unsigned int size);
bool Decompress(unsigned char *vch, unsigned int size);

bool TweakPublic(const unsigned char *vch, unsigned int size, const unsigned char vchTweak[32], unsigned char out[65]);
bool TweakSecret(unsigned char vchSecretOut[32], const unsigned char vchSecretIn[32], const unsigned char vchTweak[32]);

bool KeyCheck(const unsigned char *vch);
bool KeyCheckSignatureElement(const unsigned char *vch, int len, bool half);
#endif
