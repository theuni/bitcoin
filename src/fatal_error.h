// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FATAL_ERROR_H
#define BITCOIN_FATAL_ERROR_H

#include <cassert>
#include <variant>

enum class FatalError
{
    UNKNOWN,
    BLOCK_DISCONNECT_ERROR,
    BLOCK_MUTATED,
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

enum class UserInterrupted
{
    UNKNOWN
};

using EarlyReturn = std::variant<FatalError, UserInterrupted>;

template <typename T>
class MaybeFatalReturn;

template <typename T>
EarlyReturn BubbleUp(MaybeFatalReturn<T>&& ret);

template <typename T = std::monostate>
class MaybeFatalReturn final : std::variant<T, FatalError, UserInterrupted>
{
    EarlyReturn Bubble() const &&
    {
        if (std::holds_alternative<FatalError>(*this)) {
            return std::get<FatalError>(*this);
        } else if (std::holds_alternative<UserInterrupted>(*this)) {
            return std::get<UserInterrupted>(*this);
        } else {
            return FatalError::UNKNOWN;
        }
    }
    friend EarlyReturn BubbleUp<T>(MaybeFatalReturn<T>&&);

public:
    using underlying = std::variant<T, FatalError, UserInterrupted>;
    using std::variant<T, FatalError, UserInterrupted>::variant;

    MaybeFatalReturn& operator=(const MaybeFatalReturn&) = delete;
    MaybeFatalReturn(const MaybeFatalReturn&) = delete;

    MaybeFatalReturn& operator=(MaybeFatalReturn&&) = default;
    MaybeFatalReturn(MaybeFatalReturn&&) = default;

    MaybeFatalReturn(EarlyReturn err)
    {
        if (std::holds_alternative<FatalError>(*this)) {
            std::variant<T, FatalError, UserInterrupted>::operator=(std::get<FatalError>(err));
        } else {
            std::variant<T, FatalError, UserInterrupted>::operator=(std::get<UserInterrupted>(err));
        }
    }

    const T& operator*() const {
        assert(std::holds_alternative<T>(*this));
        return std::get<T>(*this);
    }

    T& operator*() {
        assert(std::holds_alternative<T>(*this));
        return std::get<T>(*this);
    }

    const T* operator->() const {
        assert(std::holds_alternative<T>(*this));
        return &std::get<T>(*this);
    }


    bool IsFatal() const
    {
        return std::holds_alternative<FatalError>(*this);
    }

    bool ShouldExit() const
    {
        return std::holds_alternative<FatalError>(*this) || std::holds_alternative<FatalError>(*this);
    }

    FatalError GetFatal() const
    {
        assert(std::holds_alternative<FatalError>(*this));
        return std::get<FatalError>(*this);
    }
};

template <typename T>
EarlyReturn BubbleUp(MaybeFatalReturn<T>&& ret)
{
    return std::move(ret).Bubble();
}

template <typename T = std::monostate>
using maybe_fatal_t = MaybeFatalReturn<T>;

#endif // BITCOIN_FATAL_ERROR_H
