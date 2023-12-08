// Copyright (c) 2014-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMPAT_BYTESWAP_H
#define BITCOIN_COMPAT_BYTESWAP_H

#include <cstdint>
#include <cstdlib>

static inline uint16_t internal_bswap_16(uint16_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(x);
#elif defined(_MSC_VER)
    return _byteswap_ushort(x);
#else
    return \
  ((__uint16_t) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)));
#endif
}

static inline uint32_t internal_bswap_32(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
#elif defined(_MSC_VER)
    return _byteswap_ulong(x);
#else
    return \
  ((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >> 8) \
   | (((x) & 0x0000ff00u) << 8) | (((x) & 0x000000ffu) << 24));
#endif
}

static inline uint64_t internal_bswap_64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#elif defined(_MSC_VER)
    return _byteswap_uint64(x);
#else
    return \
  ((((x) & 0xff00000000000000ull) >> 56)    \
   | (((x) & 0x00ff000000000000ull) >> 40)  \
   | (((x) & 0x0000ff0000000000ull) >> 24)  \
   | (((x) & 0x000000ff00000000ull) >> 8)   \
   | (((x) & 0x00000000ff000000ull) << 8)   \
   | (((x) & 0x0000000000ff0000ull) << 24)  \
   | (((x) & 0x000000000000ff00ull) << 40)  \
   | (((x) & 0x00000000000000ffull) << 56));
#endif
}

#define bswap_16 internal_bswap_16
#define bswap_32 internal_bswap_32
#define bswap_64 internal_bswap_64

#endif // BITCOIN_COMPAT_BYTESWAP_H
