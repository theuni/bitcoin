// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COMPAT_BITCOIN_ENDIAN_INTERNAL_H
#define COMPAT_BITCOIN_ENDIAN_INTERNAL_H

#if defined(HAVE_CONFIG_H) && defined(BUILD_BITCOIN_INTERNAL) && !defined(BITCOIN_ENDIAN_IMPL)
#include <config/bitcoin-config.h>
#include <compat/endian.h>

#define htobe16_internal(x) htobe16(x)
#define htole16_internal(x) htole16(x)
#define be16toh_internal(x) be16toh(x)
#define le16toh_internal(x) le16toh(x)
#define htobe32_internal(x) htobe32(x)
#define htole32_internal(x) htole32(x)
#define be32toh_internal(x) be32toh(x)
#define le32toh_internal(x) le32toh(x)
#define htobe64_internal(x) htobe64(x)
#define htole64_internal(x) htole64(x)
#define be64toh_internal(x) be64toh(x)
#define le64toh_internal(x) le64toh(x)

#else
#include <cstdint>

uint16_t htobe16_internal(uint16_t host_16bits);
uint16_t htole16_internal(uint16_t host_16bits);
uint16_t be16toh_internal(uint16_t big_endian_16bits);
uint16_t le16toh_internal(uint16_t little_endian_16bits);
uint32_t htobe32_internal(uint32_t host_32bits);
uint32_t htole32_internal(uint32_t host_32bits);
uint32_t be32toh_internal(uint32_t big_endian_32bits);
uint32_t le32toh_internal(uint32_t little_endian_32bits);
uint64_t htobe64_internal(uint64_t host_64bits);
uint64_t htole64_internal(uint64_t host_64bits);
uint64_t be64toh_internal(uint64_t big_endian_64bits);
uint64_t le64toh_internal(uint64_t little_endian_64bits);

#endif

#endif // COMPAT_BITCOIN_ENDIAN_INTERNAL_H
