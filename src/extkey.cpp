#include "extkey.h"

#include "uint256.h"

#ifdef USE_SECP256K1
#include <secp256k1.h>
#else
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#endif

#include <assert.h>
#include <string.h>

#ifdef USE_SECP256K1
#include <secp256k1.h>
class CSecp256k1Init {
public:
    CSecp256k1Init() {
        secp256k1_start();
    }
    ~CSecp256k1Init() {
        secp256k1_stop();
    }
};
static CSecp256k1Init instance_of_csecp256k1;

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


// Generate a private key from just the secret parameter
int EC_KEY_regenerate_key(EC_KEY *eckey, BIGNUM *priv_key)
{
    int ok = 0;
    BN_CTX *ctx = NULL;
    EC_POINT *pub_key = NULL;

    if (!eckey) return 0;

    const EC_GROUP *group = EC_KEY_get0_group(eckey);

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    pub_key = EC_POINT_new(group);

    if (pub_key == NULL)
        goto err;

    if (!EC_POINT_mul(group, pub_key, priv_key, NULL, NULL, ctx))
        goto err;

    EC_KEY_set_private_key(eckey,priv_key);
    EC_KEY_set_public_key(eckey,pub_key);

    ok = 1;

err:

    if (pub_key)
        EC_POINT_free(pub_key);
    if (ctx != NULL)
        BN_CTX_free(ctx);

    return(ok);
}

// Perform ECDSA key recovery (see SEC1 4.1.6) for curves over (mod p)-fields
// recid selects which key is recovered
// if check is non-zero, additional checks are performed
int ECDSA_SIG_recover_key_GFp(EC_KEY *eckey, ECDSA_SIG *ecsig, const unsigned char *msg, int msglen, int recid, int check)
{
    if (!eckey) return 0;

    int ret = 0;
    BN_CTX *ctx = NULL;

    BIGNUM *x = NULL;
    BIGNUM *e = NULL;
    BIGNUM *order = NULL;
    BIGNUM *sor = NULL;
    BIGNUM *eor = NULL;
    BIGNUM *field = NULL;
    EC_POINT *R = NULL;
    EC_POINT *O = NULL;
    EC_POINT *Q = NULL;
    BIGNUM *rr = NULL;
    BIGNUM *zero = NULL;
    int n = 0;
    int i = recid / 2;

    const EC_GROUP *group = EC_KEY_get0_group(eckey);
    if ((ctx = BN_CTX_new()) == NULL) { ret = -1; goto err; }
    BN_CTX_start(ctx);
    order = BN_CTX_get(ctx);
    if (!EC_GROUP_get_order(group, order, ctx)) { ret = -2; goto err; }
    x = BN_CTX_get(ctx);
    if (!BN_copy(x, order)) { ret=-1; goto err; }
    if (!BN_mul_word(x, i)) { ret=-1; goto err; }
    if (!BN_add(x, x, ecsig->r)) { ret=-1; goto err; }
    field = BN_CTX_get(ctx);
    if (!EC_GROUP_get_curve_GFp(group, field, NULL, NULL, ctx)) { ret=-2; goto err; }
    if (BN_cmp(x, field) >= 0) { ret=0; goto err; }
    if ((R = EC_POINT_new(group)) == NULL) { ret = -2; goto err; }
    if (!EC_POINT_set_compressed_coordinates_GFp(group, R, x, recid % 2, ctx)) { ret=0; goto err; }
    if (check)
    {
        if ((O = EC_POINT_new(group)) == NULL) { ret = -2; goto err; }
        if (!EC_POINT_mul(group, O, NULL, R, order, ctx)) { ret=-2; goto err; }
        if (!EC_POINT_is_at_infinity(group, O)) { ret = 0; goto err; }
    }
    if ((Q = EC_POINT_new(group)) == NULL) { ret = -2; goto err; }
    n = EC_GROUP_get_degree(group);
    e = BN_CTX_get(ctx);
    if (!BN_bin2bn(msg, msglen, e)) { ret=-1; goto err; }
    if (8*msglen > n) BN_rshift(e, e, 8-(n & 7));
    zero = BN_CTX_get(ctx);
    if (!BN_zero(zero)) { ret=-1; goto err; }
    if (!BN_mod_sub(e, zero, e, order, ctx)) { ret=-1; goto err; }
    rr = BN_CTX_get(ctx);
    if (!BN_mod_inverse(rr, ecsig->r, order, ctx)) { ret=-1; goto err; }
    sor = BN_CTX_get(ctx);
    if (!BN_mod_mul(sor, ecsig->s, rr, order, ctx)) { ret=-1; goto err; }
    eor = BN_CTX_get(ctx);
    if (!BN_mod_mul(eor, e, rr, order, ctx)) { ret=-1; goto err; }
    if (!EC_POINT_mul(group, Q, eor, R, sor, ctx)) { ret=-2; goto err; }
    if (!EC_KEY_set_public_key(eckey, Q)) { ret=-2; goto err; }

    ret = 1;

err:
    if (ctx) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (R != NULL) EC_POINT_free(R);
    if (O != NULL) EC_POINT_free(O);
    if (Q != NULL) EC_POINT_free(Q);
    return ret;
}

// RAII Wrapper around OpenSSL's EC_KEY
class CECKey {
private:
    EC_KEY *pkey;

public:
    CECKey() {
        pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
        assert(pkey != NULL);
    }

    ~CECKey() {
        EC_KEY_free(pkey);
    }

    void GetSecretBytes(unsigned char vch[32]) const {
        const BIGNUM *bn = EC_KEY_get0_private_key(pkey);
        assert(bn);
        int nBytes = BN_num_bytes(bn);
        int n=BN_bn2bin(bn,&vch[32 - nBytes]);
        assert(n == nBytes);
        memset(vch, 0, 32 - nBytes);
    }

    void SetSecretBytes(const unsigned char vch[32]) {
        bool ret;
        BIGNUM bn;
        BN_init(&bn);
        ret = BN_bin2bn(vch, 32, &bn) != NULL;
        assert(ret);
        ret = EC_KEY_regenerate_key(pkey, &bn) != 0;
        assert(ret);
        BN_clear_free(&bn);
    }

    int GetPrivKeySize(bool fCompressed) {
        EC_KEY_set_conv_form(pkey, fCompressed ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED);
        return i2d_ECPrivateKey(pkey, NULL);
    }
    int GetPrivKey(unsigned char* privkey, bool fCompressed) {
        EC_KEY_set_conv_form(pkey, fCompressed ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED);
        return i2d_ECPrivateKey(pkey, &privkey);
    }

    bool SetPrivKey(const unsigned char* privkey, size_t size, bool fSkipCheck=false) {
        if (d2i_ECPrivateKey(&pkey, &privkey, size)) {
            if(fSkipCheck)
                return true;

            // d2i_ECPrivateKey returns true if parsing succeeds.
            // This doesn't necessarily mean the key is valid.
            if (EC_KEY_check_key(pkey))
                return true;
        }
        return false;
    }

    int GetPubKeySize(bool fCompressed) {
        EC_KEY_set_conv_form(pkey, fCompressed ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED);
        return i2o_ECPublicKey(pkey, NULL);
    }

    void GetPubKey(unsigned char vch[65], bool fCompressed) {
        EC_KEY_set_conv_form(pkey, fCompressed ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED);
        int nSize = i2o_ECPublicKey(pkey, NULL);
        assert(nSize);
        assert(nSize <= 65);
        int nSize2 = i2o_ECPublicKey(pkey, &vch);
        assert(nSize == nSize2);
    }

    bool SetPubKey(const unsigned char* vch, size_t size) {
        return o2i_ECPublicKey(&pkey, &vch, size) != NULL;
    }

    bool Sign(const uint256 &hash, std::vector<unsigned char>& vchSig, bool lowS) {
        vchSig.clear();
        ECDSA_SIG *sig = ECDSA_do_sign((unsigned char*)&hash, sizeof(hash), pkey);
        if (sig == NULL)
            return false;
        BN_CTX *ctx = BN_CTX_new();
        BN_CTX_start(ctx);
        const EC_GROUP *group = EC_KEY_get0_group(pkey);
        BIGNUM *order = BN_CTX_get(ctx);
        BIGNUM *halforder = BN_CTX_get(ctx);
        EC_GROUP_get_order(group, order, ctx);
        BN_rshift1(halforder, order);
        if (lowS && BN_cmp(sig->s, halforder) > 0) {
            // enforce low S values, by negating the value (modulo the order) if above order/2.
            BN_sub(sig->s, order, sig->s);
        }
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        unsigned int nSize = ECDSA_size(pkey);
        vchSig.resize(nSize); // Make sure it is big enough
        unsigned char *pos = &vchSig[0];
        nSize = i2d_ECDSA_SIG(sig, &pos);
        ECDSA_SIG_free(sig);
        vchSig.resize(nSize); // Shrink to fit actual size
        return true;
    }

    bool Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) {
        // -1 = error, 0 = bad sig, 1 = good
        if (ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), &vchSig[0], vchSig.size(), pkey) != 1)
            return false;
        return true;
    }

    bool SignCompact(const uint256 &hash, unsigned char *p64, int &rec) {
        bool fOk = false;
        ECDSA_SIG *sig = ECDSA_do_sign((unsigned char*)&hash, sizeof(hash), pkey);
        if (sig==NULL)
            return false;
        memset(p64, 0, 64);
        int nBitsR = BN_num_bits(sig->r);
        int nBitsS = BN_num_bits(sig->s);
        if (nBitsR <= 256 && nBitsS <= 256) {
            unsigned char pubkey[65];
            memset(pubkey, 0, sizeof(pubkey));
            int pubkeysize = GetPubKeySize(true);
            GetPubKey(pubkey, true);
            for (int i=0; i<4; i++) {
                CECKey keyRec;
                if (ECDSA_SIG_recover_key_GFp(keyRec.pkey, sig, (unsigned char*)&hash, sizeof(hash), i, 1) == 1) {
                    unsigned char pubkeyRec[65];
                    memset(pubkeyRec, 0, sizeof(pubkeyRec));
                    keyRec.GetPubKey(pubkeyRec, true);
                    if (memcmp(pubkey, pubkeyRec, pubkeysize) == 0) {
                        rec = i;
                        fOk = true;
                        break;
                    }
                }
            }
            assert(fOk);
            BN_bn2bin(sig->r,&p64[32-(nBitsR+7)/8]);
            BN_bn2bin(sig->s,&p64[64-(nBitsS+7)/8]);
        }
        ECDSA_SIG_free(sig);
        return fOk;
    }

    // reconstruct public key from a compact signature
    // This is only slightly more CPU intensive than just verifying it.
    // If this function succeeds, the recovered public key is guaranteed to be valid
    // (the signature is a valid signature of the given data for that key)
    bool Recover(const uint256 &hash, const unsigned char *p64, int rec)
    {
        if (rec<0 || rec>=3)
            return false;
        ECDSA_SIG *sig = ECDSA_SIG_new();
        BN_bin2bn(&p64[0],  32, sig->r);
        BN_bin2bn(&p64[32], 32, sig->s);
        bool ret = ECDSA_SIG_recover_key_GFp(pkey, sig, (unsigned char*)&hash, sizeof(hash), rec, 0) == 1;
        ECDSA_SIG_free(sig);
        return ret;
    }

    static bool TweakSecret(unsigned char vchSecretOut[32], const unsigned char vchSecretIn[32], const unsigned char vchTweak[32])
    {
        bool ret = true;
        BN_CTX *ctx = BN_CTX_new();
        BN_CTX_start(ctx);
        BIGNUM *bnSecret = BN_CTX_get(ctx);
        BIGNUM *bnTweak = BN_CTX_get(ctx);
        BIGNUM *bnOrder = BN_CTX_get(ctx);
        EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp256k1);
        EC_GROUP_get_order(group, bnOrder, ctx); // what a grossly inefficient way to get the (constant) group order...
        BN_bin2bn(vchTweak, 32, bnTweak);
        if (BN_cmp(bnTweak, bnOrder) >= 0)
            ret = false; // extremely unlikely
        BN_bin2bn(vchSecretIn, 32, bnSecret);
        BN_add(bnSecret, bnSecret, bnTweak);
        BN_nnmod(bnSecret, bnSecret, bnOrder, ctx);
        if (BN_is_zero(bnSecret))
            ret = false; // ridiculously unlikely
        int nBits = BN_num_bits(bnSecret);
        memset(vchSecretOut, 0, 32);
        BN_bn2bin(bnSecret, &vchSecretOut[32-(nBits+7)/8]);
        EC_GROUP_free(group);
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        return ret;
    }

    bool TweakPublic(const unsigned char vchTweak[32]) {
        bool ret = true;
        BN_CTX *ctx = BN_CTX_new();
        BN_CTX_start(ctx);
        BIGNUM *bnTweak = BN_CTX_get(ctx);
        BIGNUM *bnOrder = BN_CTX_get(ctx);
        BIGNUM *bnOne = BN_CTX_get(ctx);
        const EC_GROUP *group = EC_KEY_get0_group(pkey);
        EC_GROUP_get_order(group, bnOrder, ctx); // what a grossly inefficient way to get the (constant) group order...
        BN_bin2bn(vchTweak, 32, bnTweak);
        if (BN_cmp(bnTweak, bnOrder) >= 0)
            ret = false; // extremely unlikely
        EC_POINT *point = EC_POINT_dup(EC_KEY_get0_public_key(pkey), group);
        BN_one(bnOne);
        EC_POINT_mul(group, point, bnTweak, point, bnOne, ctx);
        if (EC_POINT_is_at_infinity(group, point))
            ret = false; // ridiculously unlikely
        EC_KEY_set_public_key(pkey, point);
        EC_POINT_free(point);
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
        return ret;
    }
};
}

bool KeyCheck(const unsigned char *vch) {
    return CompareBigEndian(vch, 32, vchZero, 0) > 0 &&
           CompareBigEndian(vch, 32, vchMaxModOrder, 32) <= 0;
}

bool KeyCheckSignatureElement(const unsigned char *vch, int len, bool half) {
    return CompareBigEndian(vch, len, vchZero, 0) > 0 &&
           CompareBigEndian(vch, len, half ? vchMaxModHalfOrder : vchMaxModOrder, 32) <= 0;
}

bool SignCompact(const unsigned char vch[32], bool compressed, const uint256 &hash, std::vector<unsigned char>& vchSig) {
    //if (!fValid)
    //    return false;
    vchSig.resize(65);
    int rec = -1;
#ifdef USE_SECP256K1
    CKey nonce;
    do {
        nonce.MakeNewKey(true);
        if (secp256k1_ecdsa_sign_compact((const unsigned char*)&hash, 32, &vchSig[1], &vch[0], nonce.begin(), &rec))
            break;
    } while(true);
#else
    CECKey key;
    key.SetSecretBytes(vch);
    if (!key.SignCompact(hash, &vchSig[1], rec))
        return false;
#endif
    assert(rec != -1);
    vchSig[0] = 27 + rec + (compressed ? 4 : 0);
    return true;
}

bool Sign(const unsigned char vch[32], const uint256 &hash, std::vector<unsigned char>& vchSig, bool lowS) {
//    if (!fValid)
//        return false;
#ifdef USE_SECP256K1
    vchSig.resize(72);
    int nSigLen = 72;
    CKey nonce;
    do {
        nonce.MakeNewKey(true);
        if (secp256k1_ecdsa_sign((const unsigned char*)&hash, 32, (unsigned char*)&vchSig[0], &nSigLen, begin(), nonce.begin()))
            break;
    } while(true);
    vchSig.resize(nSigLen);
    return true;
#else
    CECKey key;
    key.SetSecretBytes(vch);
    return key.Sign(hash, vchSig, lowS);
#endif
}

int GetPubKey(const unsigned char vch[32], bool compressed, unsigned char ret[65]) {
    //assert(fValid);
    int clen;
#ifdef USE_SECP256K1
    clen = 65;
    int ret = secp256k1_ecdsa_pubkey_create(ret, &clen, begin(), fCompressed);
    assert(ret);
#else
    CECKey key;
    key.SetSecretBytes(vch);
    clen = key.GetPubKeySize(compressed);
    key.GetPubKey(ret, compressed);
    return clen;
#endif
}

int GetPrivKeySize(const unsigned char vch[32], bool compressed) {
#ifdef USE_SECP256K1
    return 279;
#else
    CECKey key;
    key.SetSecretBytes(vch);
    return key.GetPrivKeySize(compressed);
#endif
}

void GetPrivKey(const unsigned char vch[32], bool compressed, unsigned char* ret) {
    //assert(fValid);
#ifdef USE_SECP256K1
    privkeylen = 279;
    ret = secp256k1_ecdsa_privkey_export(vch, ret, &privkeylen, fCompressed);
    assert(ret);
#else
    CECKey key;
    key.SetSecretBytes(vch);
    key.GetPrivKey(ret, compressed);
#endif
}

bool SetPrivKey(const unsigned char*  privkey, size_t size, unsigned char ret[32]) {
#ifdef USE_SECP256K1
    if (!secp256k1_ecdsa_privkey_import(ret, privkey, size))
        return false;
#else
    CECKey key;
    if (!key.SetPrivKey(privkey, size))
        return false;
    key.GetSecretBytes(ret);
    return true;
#endif
}

bool Verify(const unsigned char *vch, unsigned int size, const uint256 &hash, const std::vector<unsigned char>& vchSig) {
    //if (!IsValid())
    //    return false;
#ifdef USE_SECP256K1
    if (secp256k1_ecdsa_verify((const unsigned char*)&hash, 32, &vchSig[0], vchSig.size(), vch, size) != 1)
        return false;
#else
    CECKey key;
    if (!key.SetPubKey(vch, size))
        return false;
    if (!key.Verify(hash, vchSig))
        return false;
#endif
    return true;
}

bool RecoverCompact(unsigned char *vch, unsigned int size, const uint256 &hash, const std::vector<unsigned char>& vchSig) {
    if (vchSig.size() != 65)
        return false;
    int recid = (vchSig[0] - 27) & 3;
    bool fComp = ((vchSig[0] - 27) & 4) != 0;
#ifdef USE_SECP256K1
    int pubkeylen = size;
    if (!secp256k1_ecdsa_recover_compact((const unsigned char*)&hash, 32, &vchSig[1], vch, &pubkeylen, fComp, recid))
        return false;
#else
    CECKey key;
    if (!key.Recover(hash, &vchSig[1], recid))
        return false;
    key.GetPubKey(vch, fComp);
#endif
    return true;
}

bool IsFullyValid(const unsigned char* vch, unsigned int size){
    //if (!IsValid())
    //    return false;
#ifdef USE_SECP256K1
    if (!secp256k1_ecdsa_pubkey_verify(vch, size))
        return false;
#else
    CECKey key;
    if (!key.SetPubKey(vch, size))
        return false;
#endif
    return true;
}

bool Decompress(unsigned char *vch, unsigned int size) {
    //if (!IsValid())
    //    return false;
#ifdef USE_SECP256K1
    int clen = (int)size;
    int ret = secp256k1_ecdsa_pubkey_decompress(vch, &clen);
    assert(ret);
#else
    CECKey key;
    if (!key.SetPubKey(vch, size))
        return false;
    key.GetPubKey(vch, false);
#endif
    return true;
}

bool TweakPublic(const unsigned char *vch, unsigned int size, const unsigned char vchTweak[32], unsigned char out[65]) {
#ifdef USE_SECP256K1
    pubkeyChild = *this;
    bool ret = secp256k1_ecdsa_pubkey_tweak_add((unsigned char*)pubkeyChild.begin(), pubkeyChild.size(), out);
#else
    CECKey key;
    bool ret = key.SetPubKey(vch, size);
    ret &= key.TweakPublic(vchTweak);
    key.GetPubKey(out, true);
#endif
    return ret;
}


bool TweakSecret(unsigned char vchSecretOut[32], const unsigned char vchSecretIn[32], const unsigned char vchTweak[32])
{
    return CECKey::TweakSecret(vchSecretOut, vchSecretIn, vchTweak);
}

bool ECC_InitSanityCheck() {
#ifdef USE_SECP256K1
    return true;
#else
    EC_KEY *pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if(pkey == NULL)
        return false;
    EC_KEY_free(pkey);

    // TODO Is there more EC functionality that could be missing?
    return true;
#endif
}

