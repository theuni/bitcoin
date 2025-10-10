// Copyright (c) 2017-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Based on the public domain implementation 'merged' by D. J. Bernstein
// See https://cr.yp.to/chacha.html.

#include <crypto/common.h>
#include <crypto/chacha20.h>
#include <support/cleanse.h>
#include <span.h>

#include <algorithm>
#include <bit>
#include <cstring>

#include <immintrin.h>

#define QUARTERROUND(a,b,c,d) \
  a += b; d = std::rotl(d ^ a, 16); \
  c += d; b = std::rotl(b ^ c, 12); \
  a += b; d = std::rotl(d ^ a, 8); \
  c += d; b = std::rotl(b ^ c, 7);

#define REPEAT10(a) do { {a}; {a}; {a}; {a}; {a}; {a}; {a}; {a}; {a}; {a}; } while(0)

void ChaCha20Aligned::SetKey(std::span<const std::byte> key) noexcept
{
    assert(key.size() == KEYLEN);
    input[0] = ReadLE32(key.data() + 0);
    input[1] = ReadLE32(key.data() + 4);
    input[2] = ReadLE32(key.data() + 8);
    input[3] = ReadLE32(key.data() + 12);
    input[4] = ReadLE32(key.data() + 16);
    input[5] = ReadLE32(key.data() + 20);
    input[6] = ReadLE32(key.data() + 24);
    input[7] = ReadLE32(key.data() + 28);
    input[8] = 0;
    input[9] = 0;
    input[10] = 0;
    input[11] = 0;
}

ChaCha20Aligned::~ChaCha20Aligned()
{
    memory_cleanse(input, sizeof(input));
}

ChaCha20Aligned::ChaCha20Aligned(std::span<const std::byte> key) noexcept
{
    SetKey(key);
}

void ChaCha20Aligned::Seek(Nonce96 nonce, uint32_t block_counter) noexcept
{
    input[8] = block_counter;
    input[9] = nonce.first;
    input[10] = nonce.second;
    input[11] = nonce.second >> 32;
}

namespace {
template <int I>
inline __m128i rotl(__m128i va);

 template <>
inline __m128i rotl<7>(__m128i x)
{
        return _mm_or_si128(_mm_slli_epi32(x, 7), _mm_srli_epi32(x, 32 - 7));
}

template <>
inline __m128i rotl<12>(__m128i x)
{
        return _mm_or_si128(_mm_slli_epi32(x, 12), _mm_srli_epi32(x, 32 - 12));
}

template <>
inline __m128i rotl<8>(__m128i x)
{
        static constexpr __m128i mask = _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
        return _mm_shuffle_epi8(x, mask);
}

template <>
inline __m128i rotl<16>(__m128i x)
{
        static constexpr __m128i mask = _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
        return _mm_shuffle_epi8(x, mask);
}

inline void doubleround(__m128i& a, __m128i& b, __m128i& c, __m128i& d)
{
    a = _mm_add_epi32(a, b); d = rotl<16>(_mm_xor_si128(d, a));
    c = _mm_add_epi32(c, d); b = rotl<12>(_mm_xor_si128(b, c));
    a = _mm_add_epi32(a, b); d = rotl<8>(_mm_xor_si128(d, a));
    c = _mm_add_epi32(c, d); b = rotl<7>(_mm_xor_si128(b ,c));

    b = _mm_shuffle_epi32(b, _MM_SHUFFLE(0, 3, 2, 1));
    c = _mm_shuffle_epi32(c, _MM_SHUFFLE(1, 0, 3, 2));
    d = _mm_shuffle_epi32(d, _MM_SHUFFLE(2, 1, 0, 3));

    a = _mm_add_epi32(a, b); d = rotl<16>(_mm_xor_si128(d, a));
    c = _mm_add_epi32(c, d); b = rotl<12>(_mm_xor_si128(b, c));
    a = _mm_add_epi32(a, b); d = rotl<8>(_mm_xor_si128(d, a));
    c = _mm_add_epi32(c, d); b = rotl<7>(_mm_xor_si128(b ,c));

    b = _mm_shuffle_epi32(b, _MM_SHUFFLE(2, 1, 0, 3));
    c = _mm_shuffle_epi32(c, _MM_SHUFFLE(1, 0, 3, 2));
    d = _mm_shuffle_epi32(d, _MM_SHUFFLE(0, 3, 2, 1));
}

template <int I>
inline __m256i rotl(__m256i va);

template <>
inline __m256i rotl<7>(__m256i x)
{
    return _mm256_or_si256(_mm256_slli_epi32(x, 7), _mm256_srli_epi32(x, 32 - 7));
}

template <>
inline __m256i rotl<12>(__m256i x)
{
    return _mm256_or_si256(_mm256_slli_epi32(x, 12), _mm256_srli_epi32(x, 32 - 12));
}

template <>
inline __m256i rotl<8>(__m256i x)
{
	static constexpr auto mask = _mm256_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3, 14,
			13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
	return _mm256_shuffle_epi8(x, mask);
}

template <>
inline __m256i rotl<16>(__m256i x)
{
	static constexpr auto mask = _mm256_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2, 13,
			12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
	return _mm256_shuffle_epi8(x, mask);
}


inline void doubleround(__m256i& a, __m256i& b, __m256i& c, __m256i& d)
{
    a = _mm256_add_epi32(a, b); d = rotl<16>(_mm256_xor_si256(d, a));
    c = _mm256_add_epi32(c, d); b = rotl<12>(_mm256_xor_si256(b, c));
    a = _mm256_add_epi32(a, b); d = rotl<8>(_mm256_xor_si256(d, a));
    c = _mm256_add_epi32(c, d); b = rotl<7>(_mm256_xor_si256(b ,c));
    b = _mm256_shuffle_epi32(b, _MM_SHUFFLE(0, 3, 2, 1));
    c = _mm256_shuffle_epi32(c, _MM_SHUFFLE(1, 0, 3, 2));
    d = _mm256_shuffle_epi32(d, _MM_SHUFFLE(2, 1, 0, 3));
    a = _mm256_add_epi32(a, b); d = rotl<16>(_mm256_xor_si256(d, a));
    c = _mm256_add_epi32(c, d); b = rotl<12>(_mm256_xor_si256(b, c));
    a = _mm256_add_epi32(a, b); d = rotl<8>(_mm256_xor_si256(d, a));
    c = _mm256_add_epi32(c, d); b = rotl<7>(_mm256_xor_si256(b ,c));
    b = _mm256_shuffle_epi32(b, _MM_SHUFFLE(2, 1, 0, 3));
    c = _mm256_shuffle_epi32(c, _MM_SHUFFLE(1, 0, 3, 2));
    d = _mm256_shuffle_epi32(d, _MM_SHUFFLE(0, 3, 2, 1));
}
template <unsigned long I>
inline void doubleround(std::array<__m256i, I>& a, std::array<__m256i, I>& b, std::array<__m256i, I>&c, std::array<__m256i, I>&d)
{
    for(unsigned long i = 0; i < I; i++) { a[i] = _mm256_add_epi32(a[i], b[i]); d[i] = rotl<16>(_mm256_xor_si256(d[i], a[i])); }
    for(unsigned long i = 0; i < I; i++) { c[i] = _mm256_add_epi32(c[i], d[i]); b[i] = rotl<12>(_mm256_xor_si256(b[i], c[i])); }
    for(unsigned long i = 0; i < I; i++) { a[i] = _mm256_add_epi32(a[i], b[i]); d[i] = rotl<8>(_mm256_xor_si256(d[i], a[i])); }
    for(unsigned long i = 0; i < I; i++) { c[i] = _mm256_add_epi32(c[i], d[i]); b[i] = rotl<7>(_mm256_xor_si256(b[i] ,c[i])); }
    for(unsigned long i = 0; i < I; i++) { b[i] = _mm256_shuffle_epi32(b[i], _MM_SHUFFLE(0, 3, 2, 1)); }
    for(unsigned long i = 0; i < I; i++) { c[i] = _mm256_shuffle_epi32(c[i], _MM_SHUFFLE(1, 0, 3, 2)); }
    for(unsigned long i = 0; i < I; i++) { d[i] = _mm256_shuffle_epi32(d[i], _MM_SHUFFLE(2, 1, 0, 3)); }
    for(unsigned long i = 0; i < I; i++) { a[i] = _mm256_add_epi32(a[i], b[i]); d[i] = rotl<16>(_mm256_xor_si256(d[i], a[i])); }
    for(unsigned long i = 0; i < I; i++) { c[i] = _mm256_add_epi32(c[i], d[i]); b[i] = rotl<12>(_mm256_xor_si256(b[i], c[i])); }
    for(unsigned long i = 0; i < I; i++) { a[i] = _mm256_add_epi32(a[i], b[i]); d[i] = rotl<8>(_mm256_xor_si256(d[i], a[i])); }
    for(unsigned long i = 0; i < I; i++) { c[i] = _mm256_add_epi32(c[i], d[i]); b[i] = rotl<7>(_mm256_xor_si256(b[i] ,c[i])); }
    for(unsigned long i = 0; i < I; i++) { b[i] = _mm256_shuffle_epi32(b[i], _MM_SHUFFLE(2, 1, 0, 3)); }
    for(unsigned long i = 0; i < I; i++) { c[i] = _mm256_shuffle_epi32(c[i], _MM_SHUFFLE(1, 0, 3, 2)); }
    for(unsigned long i = 0; i < I; i++) { d[i] = _mm256_shuffle_epi32(d[i], _MM_SHUFFLE(0, 3, 2, 1)); }
}

} // anonymous namespace

inline void ChaCha20Aligned::Keystream(std::span<std::byte> output) noexcept
{
    std::byte* c = output.data();
    size_t blocks = output.size() / BLOCKLEN;
    assert(blocks * BLOCKLEN == output.size());

    uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
    uint32_t j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;

    if (!blocks) return;

    j4 = input[0];
    j5 = input[1];
    j6 = input[2];
    j7 = input[3];
    j8 = input[4];
    j9 = input[5];
    j10 = input[6];
    j11 = input[7];
    j12 = input[8];
    j13 = input[9];
    j14 = input[10];
    j15 = input[11];

    for (;;) {
        x0 = 0x61707865;
        x1 = 0x3320646e;
        x2 = 0x79622d32;
        x3 = 0x6b206574;
        x4 = j4;
        x5 = j5;
        x6 = j6;
        x7 = j7;
        x8 = j8;
        x9 = j9;
        x10 = j10;
        x11 = j11;
        x12 = j12;
        x13 = j13;
        x14 = j14;
        x15 = j15;

        // The 20 inner ChaCha20 rounds are unrolled here for performance.
        REPEAT10(
            QUARTERROUND( x0, x4, x8,x12);
            QUARTERROUND( x1, x5, x9,x13);
            QUARTERROUND( x2, x6,x10,x14);
            QUARTERROUND( x3, x7,x11,x15);
            QUARTERROUND( x0, x5,x10,x15);
            QUARTERROUND( x1, x6,x11,x12);
            QUARTERROUND( x2, x7, x8,x13);
            QUARTERROUND( x3, x4, x9,x14);
        );

        x0 += 0x61707865;
        x1 += 0x3320646e;
        x2 += 0x79622d32;
        x3 += 0x6b206574;
        x4 += j4;
        x5 += j5;
        x6 += j6;
        x7 += j7;
        x8 += j8;
        x9 += j9;
        x10 += j10;
        x11 += j11;
        x12 += j12;
        x13 += j13;
        x14 += j14;
        x15 += j15;

        ++j12;
        if (!j12) ++j13;

        WriteLE32(c + 0, x0);
        WriteLE32(c + 4, x1);
        WriteLE32(c + 8, x2);
        WriteLE32(c + 12, x3);
        WriteLE32(c + 16, x4);
        WriteLE32(c + 20, x5);
        WriteLE32(c + 24, x6);
        WriteLE32(c + 28, x7);
        WriteLE32(c + 32, x8);
        WriteLE32(c + 36, x9);
        WriteLE32(c + 40, x10);
        WriteLE32(c + 44, x11);
        WriteLE32(c + 48, x12);
        WriteLE32(c + 52, x13);
        WriteLE32(c + 56, x14);
        WriteLE32(c + 60, x15);

        if (blocks == 1) {
            input[8] = j12;
            input[9] = j13;
            return;
        }
        blocks -= 1;
        c += BLOCKLEN;
    }
}

inline void ChaCha20Aligned::Crypt(std::span<const std::byte> in_bytes, std::span<std::byte> out_bytes) noexcept
{
    assert(in_bytes.size() == out_bytes.size());
    const std::byte* m = in_bytes.data();
    std::byte* c = out_bytes.data();
    size_t blocks = out_bytes.size() / BLOCKLEN;
    assert(blocks * BLOCKLEN == out_bytes.size());

    if (!blocks) return;

    const auto state01 = _mm256_load_si256((__m256i*)(&input[0]));
    const auto state0 = _mm256_broadcastsi128_si256(_mm256_castsi256_si128(state01));
    const auto state1 = _mm256_broadcastsi128_si256(_mm256_extracti128_si256(state01, 1));
    auto state2 =  _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)(&input[8])));

    static constexpr auto nums256 = _mm256_set_epi32(0x6b206574, 0x79622d32, 0x3320646e, 0x61707865, 0x6b206574, 0x79622d32, 0x3320646e, 0x61707865);
    static constexpr auto row0 = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, 1);
    static constexpr auto row1 = _mm256_set_epi32(0, 0, 0, 2, 0, 0, 0, 3);
    static constexpr auto row2 = _mm256_set_epi32(0, 0, 0, 4, 0, 0, 0, 5);

    while(blocks >= 6) {
        std::array<__m256i, 3> xv0, xv1, xv2, xv3;

        xv0[0] = nums256;
        xv0[1] = nums256;
        xv0[2] = nums256;

        xv1[0] = state0;
        xv1[1] = state0;
        xv1[2] = state0;

        xv2[0] = state1;
        xv2[1] = state1;
        xv2[2] = state1;

        xv3[0] = _mm256_add_epi64(state2, row0);
        xv3[1] = _mm256_add_epi64(state2, row1);
        xv3[2] = _mm256_add_epi64(state2, row2);

        // The 20 inner ChaCha20 rounds are unrolled here for performance.
        REPEAT10(
            doubleround(xv0, xv1, xv2, xv3);
        );

        xv0[0] = _mm256_add_epi32(xv0[0], nums256);
        xv0[1] = _mm256_add_epi32(xv0[1], nums256);
        xv0[2] = _mm256_add_epi32(xv0[2], nums256);

        xv1[0] = _mm256_add_epi32(xv1[0], state0);
        xv1[1] = _mm256_add_epi32(xv1[1], state0);
        xv1[2] = _mm256_add_epi32(xv1[2], state0);

        xv2[0] = _mm256_add_epi32(xv2[0], state1);
        xv2[1] = _mm256_add_epi32(xv2[1], state1);
        xv2[2] = _mm256_add_epi32(xv2[2], state1);

        xv3[0] = _mm256_add_epi32(xv3[0], _mm256_add_epi64(state2, row0));
        xv3[1] = _mm256_add_epi32(xv3[1], _mm256_add_epi64(state2, row1));
        xv3[2] = _mm256_add_epi32(xv3[2], _mm256_add_epi64(state2, row2));

        for (int i = 0; i < 3; i++) {
            auto x256v = _mm256_permute2x128_si256(xv0[i], xv1[i], 1 + (3 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[0 + (128 * i)]), x256v);

            x256v = _mm256_permute2x128_si256(xv2[i], xv3[i], 1 + (3 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 32 + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[32 + (128 * i)]), x256v);

            x256v = _mm256_permute2x128_si256(xv0[i], xv1[i], 0 + (2 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 64 + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[64 + (128 * i)]), x256v);

            x256v = _mm256_permute2x128_si256(xv2[i], xv3[i], 0 + (2 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 96 + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[96 + (128 * i)]), x256v);
        }

        state2 = _mm256_add_epi64(state2, _mm256_set_epi32(0, 0, 0, 6, 0, 0, 0, 6));
        blocks -= 6;
        c += BLOCKLEN * 6;
        m += BLOCKLEN * 6;
    }

    while(blocks >= 4) {
        std::array<__m256i, 2> xv0, xv1, xv2, xv3;

        xv0[0] = nums256;
        xv0[1] = nums256;

        xv1[0] = state0;
        xv1[1] = state0;

        xv2[0] = state1;
        xv2[1] = state1;

        xv3[0] = _mm256_add_epi64(state2, row0);
        xv3[1] = _mm256_add_epi64(state2, row1);

        // The 20 inner ChaCha20 rounds are unrolled here for performance.
        REPEAT10(
            doubleround(xv0, xv1, xv2, xv3);
        );

        xv0[0] = _mm256_add_epi32(xv0[0], nums256);
        xv0[1] = _mm256_add_epi32(xv0[1], nums256);

        xv1[0] = _mm256_add_epi32(xv1[0], state0);
        xv1[1] = _mm256_add_epi32(xv1[1], state0);

        xv2[0] = _mm256_add_epi32(xv2[0], state1);
        xv2[1] = _mm256_add_epi32(xv2[1], state1);

        xv3[0] = _mm256_add_epi32(xv3[0], _mm256_add_epi64(state2, row0));
        xv3[1] = _mm256_add_epi32(xv3[1], _mm256_add_epi64(state2, row1));

        for (int i = 0; i < 2; i++) {
            auto x256v = _mm256_permute2x128_si256(xv0[i], xv1[i], 1 + (3 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[0 + (128 * i)]), x256v);

            x256v = _mm256_permute2x128_si256(xv2[i], xv3[i], 1 + (3 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 32 + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[32 + (128 * i)]), x256v);

            x256v = _mm256_permute2x128_si256(xv0[i], xv1[i], 0 + (2 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 64 + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[64 + (128 * i)]), x256v);

            x256v = _mm256_permute2x128_si256(xv2[i], xv3[i], 0 + (2 << 4));
            x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 96 + (128 * i))));
            _mm256_storeu_si256((__m256i*)(&c[96 + (128 * i)]), x256v);
        }

        state2 = _mm256_add_epi64(state2, _mm256_set_epi32(0, 0, 0, 4, 0, 0, 0, 4));
        blocks -= 4;
        c += BLOCKLEN * 4;
        m += BLOCKLEN * 4;
    }


    while (blocks >= 2) {
        __m256i xv0, xv1, xv2, xv3;
        xv0 = nums256;
        xv1 = state0;
        xv2 = state1;
        xv3 = _mm256_add_epi64(state2, row0);

        // The 20 inner ChaCha20 rounds are unrolled here for performance.
        REPEAT10(
            doubleround(xv0, xv1, xv2, xv3);
        );

        xv0 = _mm256_add_epi32(xv0, nums256);
        xv1 = _mm256_add_epi32(xv1, state0);
        xv2 = _mm256_add_epi32(xv2, state1);
        xv3 = _mm256_add_epi32(xv3, _mm256_add_epi64(state2, row0));

        auto x256v = _mm256_permute2x128_si256(xv0, xv1, 1 + (3 << 4));
        x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m)));
        _mm256_storeu_si256((__m256i*)(&c[0]), x256v);

        x256v = _mm256_permute2x128_si256(xv2, xv3, 1 + (3 << 4));
        x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 32)));
        _mm256_storeu_si256((__m256i*)(&c[32]), x256v);

        x256v = _mm256_permute2x128_si256(xv0, xv1, 0 + (2 << 4));
        x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 64)));
        _mm256_storeu_si256((__m256i*)(&c[64]), x256v);

        x256v = _mm256_permute2x128_si256(xv2, xv3, 0 + (2 << 4));
        x256v = _mm256_xor_si256(x256v, _mm256_loadu_si256((__m256i*)(m + 96)));
        _mm256_storeu_si256((__m256i*)(&c[96]), x256v);

        state2 = _mm256_add_epi32(state2, _mm256_set_epi32(0, 0, 0, 2, 0, 0, 0, 2));
        blocks -= 2;
        c += BLOCKLEN * 2;
        m += BLOCKLEN * 2;
    }

    if (blocks) {
        __m128i xv0, xv1, xv2, xv3;
        xv0 = _mm256_extracti128_si256(nums256, 0);
        xv1 = _mm256_extracti128_si256(state0, 0);
        xv2 = _mm256_extracti128_si256(state1, 0);
        xv3 = _mm256_extracti128_si256(state2, 0);

        // The 20 inner ChaCha20 rounds are unrolled here for performance.
        REPEAT10(
            doubleround(xv0, xv1, xv2, xv3);
        );

        auto data = _mm256_loadu_si256((__m256i*)(m));
        auto mixin = _mm256_permute2x128_si256(nums256, state0, 1 + (3 << 4));
        auto x256v = _mm256_xor_si256(_mm256_add_epi32(_mm256_set_m128i(xv1, xv0), mixin), data);
        _mm256_storeu_si256((__m256i*)(&c[0]), x256v);

        data = _mm256_loadu_si256((__m256i*)(m+32));
        mixin = _mm256_permute2x128_si256(state1, state2,  0 + (2 << 4));
        x256v = _mm256_xor_si256(_mm256_add_epi32(_mm256_set_m128i(xv3, xv2), mixin), data);
        _mm256_storeu_si256((__m256i*)(&c[32]), x256v);

        state2 = _mm256_add_epi32(state2, _mm256_set_epi32(0, 0, 0, 1, 0, 0, 0, 1));
    }
    _mm_storeu_si64(&input[8], _mm256_castsi256_si128(state2));
}

void ChaCha20::Keystream(std::span<std::byte> out) noexcept
{
    if (out.empty()) return;
    if (m_bufleft) {
        unsigned reuse = std::min<size_t>(m_bufleft, out.size());
        std::copy(m_buffer.end() - m_bufleft, m_buffer.end() - m_bufleft + reuse, out.begin());
        m_bufleft -= reuse;
        out = out.subspan(reuse);
    }
    if (out.size() >= m_aligned.BLOCKLEN) {
        size_t blocks = out.size() / m_aligned.BLOCKLEN;
        m_aligned.Keystream(out.first(blocks * m_aligned.BLOCKLEN));
        out = out.subspan(blocks * m_aligned.BLOCKLEN);
    }
    if (!out.empty()) {
        m_aligned.Keystream(m_buffer);
        std::copy(m_buffer.begin(), m_buffer.begin() + out.size(), out.begin());
        m_bufleft = m_aligned.BLOCKLEN - out.size();
    }
}

void ChaCha20::Crypt(std::span<const std::byte> input, std::span<std::byte> output) noexcept
{
    assert(input.size() == output.size());

    if (!input.size()) return;
    if (m_bufleft) {
        unsigned reuse = std::min<size_t>(m_bufleft, input.size());
        for (unsigned i = 0; i < reuse; i++) {
            output[i] = input[i] ^ m_buffer[m_aligned.BLOCKLEN - m_bufleft + i];
        }
        m_bufleft -= reuse;
        output = output.subspan(reuse);
        input = input.subspan(reuse);
    }
    if (input.size() >= m_aligned.BLOCKLEN) {
        size_t blocks = input.size() / m_aligned.BLOCKLEN;
        m_aligned.Crypt(input.first(blocks * m_aligned.BLOCKLEN), output.first(blocks * m_aligned.BLOCKLEN));
        output = output.subspan(blocks * m_aligned.BLOCKLEN);
        input = input.subspan(blocks * m_aligned.BLOCKLEN);
    }
    if (!input.empty()) {
        m_aligned.Keystream(m_buffer);
        for (unsigned i = 0; i < input.size(); i++) {
            output[i] = input[i] ^ m_buffer[i];
        }
        m_bufleft = m_aligned.BLOCKLEN - input.size();
    }
}

ChaCha20::~ChaCha20()
{
    memory_cleanse(m_buffer.data(), m_buffer.size());
}

void ChaCha20::SetKey(std::span<const std::byte> key) noexcept
{
    m_aligned.SetKey(key);
    m_bufleft = 0;
    memory_cleanse(m_buffer.data(), m_buffer.size());
}

FSChaCha20::FSChaCha20(std::span<const std::byte> key, uint32_t rekey_interval) noexcept :
    m_chacha20(key), m_rekey_interval(rekey_interval)
{
    assert(key.size() == KEYLEN);
}

void FSChaCha20::Crypt(std::span<const std::byte> input, std::span<std::byte> output) noexcept
{
    assert(input.size() == output.size());

    // Invoke internal stream cipher for actual encryption/decryption.
    m_chacha20.Crypt(input, output);

    // Rekey after m_rekey_interval encryptions/decryptions.
    if (++m_chunk_counter == m_rekey_interval) {
        // Get new key from the stream cipher.
        std::byte new_key[KEYLEN];
        m_chacha20.Keystream(new_key);
        // Update its key.
        m_chacha20.SetKey(new_key);
        // Wipe the key (a copy remains inside m_chacha20, where it'll be wiped on the next rekey
        // or on destruction).
        memory_cleanse(new_key, sizeof(new_key));
        // Set the nonce for the new section of output.
        m_chacha20.Seek({0, ++m_rekey_counter}, 0);
        // Reset the chunk counter.
        m_chunk_counter = 0;
    }
}
