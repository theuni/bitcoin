// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FATAL_ERROR_H
#define BITCOIN_FATAL_ERROR_H

#include <variant>

enum class FatalError
{
    BLOCK_DISCONNECT_ERROR,
    BLOCK_FLUSH_FAILED,
    BLOCK_READ_FAILED,
    BLOCK_READ_CORRUPT,
    BLOCK_WRITE_FAILED,
    BLOCK_INDEX_WRITE_FAILED,
    COINSDB_WRITE_FAILED,
    DISK_SPACE_ERROR,
    FLUSH_SYSTEM_ERROR,
    UNDO_FLUSH_FAILED,
    UNDO_WRITE_FAILED
};

template <typename T = std::monostate>
using maybe_fatal_t = std::variant<T, FatalError>;



#endif // BITCOIN_FATAL_ERROR_H
