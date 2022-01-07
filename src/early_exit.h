// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EARLY_EXIT_H
#define BITCOIN_EARLY_EXIT_H

#include <atomic>
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

using EarlyExit = std::variant<FatalError, UserInterrupted>;

template <typename T>
class MaybeEarlyExit;

template <typename T>
EarlyExit BubbleUp(MaybeEarlyExit<T>&& ret);

template <typename T = std::monostate>
class MaybeEarlyExit : std::variant<T, FatalError, UserInterrupted>
{
    EarlyExit Bubble() const &&
    {
        if (std::holds_alternative<FatalError>(*this)) {
            return std::get<FatalError>(*this);
        } else if (std::holds_alternative<UserInterrupted>(*this)) {
            return std::get<UserInterrupted>(*this);
        } else {
            return FatalError::UNKNOWN;
        }
    }
    friend EarlyExit BubbleUp<T>(MaybeEarlyExit<T>&&);

public:
    using underlying = std::variant<T, FatalError, UserInterrupted>;
    using std::variant<T, FatalError, UserInterrupted>::variant;

    MaybeEarlyExit& operator=(const MaybeEarlyExit&) = delete;
    MaybeEarlyExit(const MaybeEarlyExit&) = delete;

    MaybeEarlyExit& operator=(MaybeEarlyExit&&) = default;
    MaybeEarlyExit(MaybeEarlyExit&&) = default;

    MaybeEarlyExit(EarlyExit err)
    {
        if (std::holds_alternative<FatalError>(*this)) {
            std::variant<T, FatalError, UserInterrupted>::operator=(std::get<FatalError>(err));
        } else {
            std::variant<T, FatalError, UserInterrupted>::operator=(std::get<UserInterrupted>(err));
        }
    }

    bool ShouldEarlyExit() const
    {
        return std::holds_alternative<FatalError>(*this) || std::holds_alternative<FatalError>(*this);
    }

    const T& operator*() const {
        assert(!ShouldEarlyExit());
        return std::get<T>(*this);
    }

    T& operator*() {
        assert(!ShouldEarlyExit());
        return std::get<T>(*this);
    }

    const T* operator->() const {
        assert(!ShouldEarlyExit());
        return &std::get<T>(*this);
    }

    EarlyExit GetEarlyExit() const
    {
        assert(ShouldEarlyExit());
        return std::get<FatalError>(*this);
    }
};

template <typename T>
EarlyExit BubbleUp(MaybeEarlyExit<T>&& ret)
{
    return std::move(ret).Bubble();
}

using user_interrupt_t = std::atomic<bool>;

#endif // BITCOIN_EARLY_EXIT_H
