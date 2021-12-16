// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FATAL_ERROR_H
#define BITCOIN_FATAL_ERROR_H

#include <variant>

enum class FatalError
{
    UNKNOWN,
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
class MaybeFatalReturn final : std::variant<T, FatalError>
{
public:
    //std::variant<T, FatalError> m_value;
    using std::variant<T, FatalError>::variant;
//    using std::variant<T, FatalError>::operator=;

    const T& operator*() const {
        assert(!std::holds_alternative<FatalError>(*this));
        return std::get<T>(*this);
    }

    T& operator*() {
        assert(!std::holds_alternative<FatalError>(*this));
        return std::get<T>(*this);
    }

    const T* operator->() const {
        assert(!std::holds_alternative<FatalError>(*this));
        return &std::get<T>(*this);
    }

    bool IsFatal() const
    {
        return std::holds_alternative<FatalError>(*this);
    }

    FatalError GetFatal() const
    {
        assert(std::holds_alternative<FatalError>(*this));
        return std::get<FatalError>(*this);
    }
};

template <typename T = std::monostate>
using maybe_fatal_t = MaybeFatalReturn<T>;

#endif // BITCOIN_FATAL_ERROR_H
