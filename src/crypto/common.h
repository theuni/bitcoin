// Copyright (c) 2014-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_COMMON_H
#define BITCOIN_CRYPTO_COMMON_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <bit>
#include <stdint.h>
#include <string.h>

#include <compat/endian.h>

uint16_t static inline ReadLE16(const unsigned char* ptr)
{
    uint16_t x;
    memcpy(&x, ptr, 2);
    if constexpr (std::endian::native == std::endian::big) return le16toh(x);
    else return x;
}

uint32_t static inline ReadLE32(const unsigned char* ptr)
{
    uint32_t x;
    memcpy(&x, ptr, 4);
    if constexpr (std::endian::native == std::endian::big) return le32toh(x);
    else return x;
}

uint64_t static inline ReadLE64(const unsigned char* ptr)
{
    uint64_t x;
    memcpy(&x, ptr, 8);
    if constexpr (std::endian::native == std::endian::big) return le64toh(x);
    else return x;
}

void static inline WriteLE16(unsigned char* ptr, uint16_t x)
{
    if constexpr (std::endian::native != std::endian::big) memcpy(ptr, &x, 2);
    else {
        uint16_t v = htole16(x);
        memcpy(ptr, &v, 2);
    }
}

void static inline WriteLE32(unsigned char* ptr, uint32_t x)
{
    if constexpr (std::endian::native != std::endian::big) memcpy(ptr, &x, 4);
    else {
        uint32_t v = htole32(x);
        memcpy(ptr, &v, 4);
    }
}

void static inline WriteLE64(unsigned char* ptr, uint64_t x)
{
    if constexpr (std::endian::native != std::endian::big) memcpy(ptr, &x, 8);
    else {
        uint64_t v = htole64(x);
        memcpy(ptr, &v, 8);
    }
}

uint16_t static inline ReadBE16(const unsigned char* ptr)
{
    uint16_t x;
    memcpy(&x, ptr, 2);
    if constexpr (std::endian::native == std::endian::big) return x;
    else return be16toh(x);
}

uint32_t static inline ReadBE32(const unsigned char* ptr)
{
    uint32_t x;
    memcpy(&x, ptr, 4);
    if constexpr (std::endian::native == std::endian::big) return x;
    else return be32toh(x);
}

uint64_t static inline ReadBE64(const unsigned char* ptr)
{
    uint64_t x;
    memcpy(&x, ptr, 8);
    if constexpr (std::endian::native == std::endian::big) return x;
    else return be64toh(x);
}

void static inline WriteBE32(unsigned char* ptr, uint32_t x)
{
    if constexpr (std::endian::native == std::endian::big) memcpy(ptr, &x, 4);
    else {
        uint32_t v = htobe32(x);
        memcpy(ptr, &v, 4);
    }
}

void static inline WriteBE64(unsigned char* ptr, uint64_t x)
{
    if constexpr (std::endian::native == std::endian::big) memcpy(ptr, &x, 8);
    else {
        uint64_t v = htobe64(x);
        memcpy(ptr, &v, 8);
    }
}

/** Return the smallest number n such that (x >> n) == 0 (or 64 if the highest bit in x is set. */
uint64_t static inline CountBits(uint64_t x)
{
    return (sizeof(x) * 8 )- std::countl_zero(x);
}

#endif // BITCOIN_CRYPTO_COMMON_H
