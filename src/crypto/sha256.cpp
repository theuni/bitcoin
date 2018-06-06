// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/sha256.h>
#include <crypto/common.h>

#include <assert.h>
#include <string.h>
#include <atomic>

#if defined(USE_ASM) && (defined(__x86_64__) || defined(__amd64__))
#include <cpuid.h>
#endif

namespace sha256_generic
{
void Transform(uint32_t* s, const unsigned char* chunk, size_t blocks);
#if defined(USE_ASM) && (defined(__x86_64__) || defined(__amd64__))
void Transform_sse41(uint32_t* s, const unsigned char* chunk, size_t blocks);
#endif
void TransformD64(unsigned char* out, const unsigned char* in);
}

namespace sha256_sse41
{
void Transform(uint32_t* s, const unsigned char* chunk, size_t blocks);
#if defined(USE_ASM) && (defined(__x86_64__) || defined(__amd64__))
void Transform_sse41(uint32_t* s, const unsigned char* chunk, size_t blocks);
#endif
void TransformD64(unsigned char* out, const unsigned char* in);
void Transform_4way_sse41(unsigned char* out, const unsigned char* in);
}

namespace sha256_avx2
{
void Transform(uint32_t* s, const unsigned char* chunk, size_t blocks);
#if defined(USE_ASM) && (defined(__x86_64__) || defined(__amd64__))
void Transform_sse41(uint32_t* s, const unsigned char* chunk, size_t blocks);
#endif
void TransformD64(unsigned char* out, const unsigned char* in);
void Transform_4way_sse41(unsigned char* out, const unsigned char* in);
void Transform_8way_avx2(unsigned char* out, const unsigned char* in);
}

typedef void (*TransformType)(uint32_t*, const unsigned char*, size_t);
typedef void (*TransformD64Type)(unsigned char*, const unsigned char*);

namespace {

/** Initialize SHA-256 state. */
void inline Initialize(uint32_t* s)
{
    s[0] = 0x6a09e667ul;
    s[1] = 0xbb67ae85ul;
    s[2] = 0x3c6ef372ul;
    s[3] = 0xa54ff53aul;
    s[4] = 0x510e527ful;
    s[5] = 0x9b05688cul;
    s[6] = 0x1f83d9abul;
    s[7] = 0x5be0cd19ul;
}

template<TransformType tr>
void TransformD64Wrapper(unsigned char* out, const unsigned char* in)
{
    uint32_t s[8];
    static const unsigned char padding1[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0
    };
    unsigned char buffer2[64] = {
        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0
    };
    ::Initialize(s);
    tr(s, in, 1);
    tr(s, padding1, 1);
    WriteBE32(buffer2 + 0, s[0]);
    WriteBE32(buffer2 + 4, s[1]);
    WriteBE32(buffer2 + 8, s[2]);
    WriteBE32(buffer2 + 12, s[3]);
    WriteBE32(buffer2 + 16, s[4]);
    WriteBE32(buffer2 + 20, s[5]);
    WriteBE32(buffer2 + 24, s[6]);
    WriteBE32(buffer2 + 28, s[7]);
    ::Initialize(s);
    tr(s, buffer2, 1);
    WriteBE32(out + 0, s[0]);
    WriteBE32(out + 4, s[1]);
    WriteBE32(out + 8, s[2]);
    WriteBE32(out + 12, s[3]);
    WriteBE32(out + 16, s[4]);
    WriteBE32(out + 20, s[5]);
    WriteBE32(out + 24, s[6]);
    WriteBE32(out + 28, s[7]);
}

bool SelfTest(TransformType tr) {
    static const unsigned char in1[65] = {0, 0x80};
    static const unsigned char in2[129] = {
        0,
        32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 
        32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0
    };
    static const uint32_t init[8] = {0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul, 0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul};
    static const uint32_t out1[8] = {0xe3b0c442ul, 0x98fc1c14ul, 0x9afbf4c8ul, 0x996fb924ul, 0x27ae41e4ul, 0x649b934cul, 0xa495991bul, 0x7852b855ul};
    static const uint32_t out2[8] = {0xce4153b0ul, 0x147c2a86ul, 0x3ed4298eul, 0xe0676bc8ul, 0x79fc77a1ul, 0x2abe1f49ul, 0xb2b055dful, 0x1069523eul};
    uint32_t buf[8];
    memcpy(buf, init, sizeof(buf));
    // Process nothing, and check we remain in the initial state.
    tr(buf, nullptr, 0);
    if (memcmp(buf, init, sizeof(buf))) return false;
    // Process the padded empty string (unaligned)
    tr(buf, in1 + 1, 1);
    if (memcmp(buf, out1, sizeof(buf))) return false;
    // Process 64 spaces (unaligned)
    memcpy(buf, init, sizeof(buf));
    tr(buf, in2 + 1, 2);
    if (memcmp(buf, out2, sizeof(buf))) return false;
    return true;
}

TransformType Transform = sha256_generic::Transform;
TransformD64Type TransformD64 = sha256_generic::TransformD64;
TransformD64Type TransformD64_4way = nullptr;
TransformD64Type TransformD64_8way = nullptr;

#if defined(USE_ASM) && (defined(__x86_64__) || defined(__amd64__))
// We can't use cpuid.h's __get_cpuid as it does not support subleafs.
void inline cpuid(uint32_t leaf, uint32_t subleaf, uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d)
{
  __asm__ ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(leaf), "2"(subleaf));
}
#endif
} // namespace


std::string SHA256AutoDetect()
{
    std::string ret = "standard";
#if defined(USE_ASM) && (defined(__x86_64__) || defined(__amd64__))
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, eax, ebx, ecx, edx);
    if ((ecx >> 19) & 1) {
        Transform = sha256_generic::Transform_sse41;
        TransformD64 = TransformD64Wrapper<sha256_generic::Transform_sse41>;
#if defined(ENABLE_SSE41) && !defined(BUILD_BITCOIN_INTERNAL)
            Transform = sha256_sse41::Transform_sse41;
            TransformD64 = TransformD64Wrapper<sha256_sse41::Transform_sse41>;
            TransformD64_4way = sha256_sse41::Transform_4way_sse41;
        ret = "sse4(1way+4way)";
#if defined(ENABLE_AVX2) && !defined(BUILD_BITCOIN_INTERNAL)
        cpuid(7, 0, eax, ebx, ecx, edx);
        if ((ebx >> 5) & 1) {
            Transform = sha256_avx2::Transform_sse41;
            TransformD64 = TransformD64Wrapper<sha256_avx2::Transform_sse41>;
            TransformD64_4way = sha256_avx2::Transform_4way_sse41;
            TransformD64_8way = sha256_avx2::Transform_8way_avx2;
            ret += ",avx2(8way)";
        }
#endif
#else
        ret = "sse4";
#endif
    }
#endif

    assert(SelfTest(Transform));
    return ret;
}

////// SHA-256

CSHA256::CSHA256() : bytes(0)
{
    ::Initialize(s);
}

CSHA256& CSHA256::Write(const unsigned char* data, size_t len)
{
    const unsigned char* end = data + len;
    size_t bufsize = bytes % 64;
    if (bufsize && bufsize + len >= 64) {
        // Fill the buffer, and process it.
        memcpy(buf + bufsize, data, 64 - bufsize);
        bytes += 64 - bufsize;
        data += 64 - bufsize;
        Transform(s, buf, 1);
        bufsize = 0;
    }
    if (end - data >= 64) {
        size_t blocks = (end - data) / 64;
        Transform(s, data, blocks);
        data += 64 * blocks;
        bytes += 64 * blocks;
    }
    if (end > data) {
        // Fill the buffer with what remains.
        memcpy(buf + bufsize, data, end - data);
        bytes += end - data;
    }
    return *this;
}

void CSHA256::Finalize(unsigned char hash[OUTPUT_SIZE])
{
    static const unsigned char pad[64] = {0x80};
    unsigned char sizedesc[8];
    WriteBE64(sizedesc, bytes << 3);
    Write(pad, 1 + ((119 - (bytes % 64)) % 64));
    Write(sizedesc, 8);
    WriteBE32(hash, s[0]);
    WriteBE32(hash + 4, s[1]);
    WriteBE32(hash + 8, s[2]);
    WriteBE32(hash + 12, s[3]);
    WriteBE32(hash + 16, s[4]);
    WriteBE32(hash + 20, s[5]);
    WriteBE32(hash + 24, s[6]);
    WriteBE32(hash + 28, s[7]);
}

CSHA256& CSHA256::Reset()
{
    bytes = 0;
    ::Initialize(s);
    return *this;
}

void SHA256D64(unsigned char* out, const unsigned char* in, size_t blocks)
{
    if (TransformD64_8way) {
        while (blocks >= 8) {
            TransformD64_8way(out, in);
            out += 256;
            in += 512;
            blocks -= 8;
        }
    }
    if (TransformD64_4way) {
        while (blocks >= 4) {
            TransformD64_4way(out, in);
            out += 128;
            in += 256;
            blocks -= 4;
        }
    }
    while (blocks) {
        TransformD64(out, in);
        out += 32;
        in += 64;
        --blocks;
    }
}
