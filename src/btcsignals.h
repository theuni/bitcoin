// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BTCSIGNALS_H
#define BITCOIN_BTCSIGNALS_H


#ifdef USE_BOOST_SIGNALS

#include <boost/signals2/connection.hpp>
#include <boost/signals2/optional_last_value.hpp>
#include <boost/signals2/signal.hpp>

namespace btcsignals = boost::signals2;

#else

#include <vector>
#include <optional>
#include <memory>
#include <type_traits>
#include <mutex>
#include <functional>

namespace btcsignals
{

struct connection
{
    connection() : m_refcount(std::make_shared<uint32_t>(1))
    {
    }
    void disconnect()
    {
        if (*m_refcount) {
            (*m_refcount)--;
        }
    };
    std::shared_ptr<uint32_t> m_refcount;
};

struct scoped_connection : public connection
{
    scoped_connection(connection&& rhs) : m_conn(rhs)
    {
    }
    ~scoped_connection()
    {
        m_conn.disconnect();
    }
    connection m_conn;
};

template <typename Ret, typename... Args>
struct slot
{
    slot(std::function<Ret(Args...)> func) : m_func(std::move(func))
    {
    }

    std::function<Ret(Args...)> m_func;
    connection m_connection;
};

template <typename Ret, typename... Args>
struct signal
{
    using functype = std::function<Ret(Args...)>;

    std::optional<Ret> operator()(const Args&... args) const
    {
        std::optional<Ret> ret = std::nullopt;
        std::lock_guard l(m_mutex);
        for(auto&& slot : m_slots) {
            if (*(slot.m_connection.m_refcount)) {
                auto tmpret = slot.m_func(args...);
                if (tmpret) ret = tmpret;
            }
        }
        return ret;
    }

    connection connect(std::function<Ret(Args...)> func)
    {
        std::lock_guard l(m_mutex);
        auto& slot = m_slots.emplace_back(std::move(func));
        return slot.m_connection;
    }

    // UNDOCUMENTED/UNSUPPORTED BOOST FUNCTIONALITY!
    // It's not clear what should happen here.
    void connect(const signal<Ret, Args...>& sig)
    {
        std::lock_guard l(m_mutex);
        m_slots.emplace_back(sig.m_slots.back());
        for(auto&& slot : sig.m_slots) {
            if (*slot.m_connection.m_refcount) {
                m_slots.emplace_back(slot);
            }
        }
    }

    bool empty() const
    {
        std::lock_guard l(m_mutex);
        for(auto&& slot : m_slots) {
            if (*slot.m_connection.m_refcount) {
                return false;
            }
        }
        return true;
    }
    std::vector<slot<Ret, Args...>> m_slots;
    mutable std::mutex m_mutex;
};

template<typename... Args>
struct signal<void, Args...>
{
    using functype = std::function<void(Args...)>;
    void operator()(const Args&... args) const
    {
        std::lock_guard l(m_mutex);
        for(auto&& slot : m_slots) {
            if (*(slot.m_connection.m_refcount)) {
                slot.m_func(args...);
            }
        }
    }

    connection connect(std::function<void(Args...)> func)
    {
        std::lock_guard l(m_mutex);
        auto& slot = m_slots.emplace_back(std::move(func));
        return slot.m_connection;
    }

    // UNDOCUMENTED/UNSUPPORTED BOOST FUNCTIONALITY!
    // It's not clear what should happen here.
    void connect(const signal<void, Args...>& sig)
    {
        std::lock_guard l(m_mutex);
        for(auto&& slot : sig.m_slots) {
            if (*slot.m_connection.m_refcount) {
                m_slots.emplace_back(slot);
            }
        }
    }

    bool empty() const
    {
        std::lock_guard l(m_mutex);
        for(auto&& slot : m_slots) {
            if (*slot.m_connection.m_refcount) {
                return false;
            }
        }
        return true;
    }
    std::vector<slot<void, Args...>> m_slots;
    mutable std::mutex m_mutex;
};

} // namespace btcsignals
#endif

#endif // BITCOIN_BTCSIGNALS_H
