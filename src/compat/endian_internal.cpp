// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BITCOIN_ENDIAN_IMPL 1

#include <compat/endian_internal.h>

#include <compat/endian.h>

uint16_t htobe16_internal(uint16_t host_16bits)
{
    return htobe16(host_16bits);
}

uint16_t htole16_internal(uint16_t host_16bits)
{
    return htole16(host_16bits);
}

uint16_t be16toh_internal(uint16_t big_endian_16bits)
{
    return be16toh(big_endian_16bits);
}

uint16_t le16toh_internal(uint16_t little_endian_16bits)
{
    return le16toh(little_endian_16bits);
}

uint32_t htobe32_internal(uint32_t host_32bits)
{
    return htobe32(host_32bits);
}

uint32_t htole32_internal(uint32_t host_32bits)
{
    return htole32(host_32bits);
}

uint32_t be32toh_internal(uint32_t big_endian_32bits)
{
    return be32toh(big_endian_32bits);
}

uint32_t le32toh_internal(uint32_t little_endian_32bits)
{
    return le32toh(little_endian_32bits);
}

uint64_t htobe64_internal(uint64_t host_64bits)
{
    return htobe64(host_64bits);
}

uint64_t htole64_internal(uint64_t host_64bits)
{
    return htole64(host_64bits);
}

uint64_t be64toh_internal(uint64_t big_endian_64bits)
{
    return be64toh(big_endian_64bits);
}

uint64_t le64toh_internal(uint64_t little_endian_64bits)
{
    return le64toh(little_endian_64bits);
}
