// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BTCSIGNALS_H
#define BITCOIN_BTCSIGNALS_H


#if 0

#include <boost/signals2/connection.hpp>
#include <boost/signals2/optional_last_value.hpp>
#include <boost/signals2/signal.hpp>

namespace btcsignals = boost::signals2;

#else

#include <optional>

namespace btcsignals
{

struct connection
{
    void disconnect(){};
};

struct scoped_connection : public connection
{
    scoped_connection(connection&&){};
};

template<typename T>
class optional_last_value {
public:
    using result_type = std::optional<T>;

    template<typename InputIterator>
    result_type operator()(InputIterator, InputIterator) const
    {
        return {};
    }
};

template <typename Signature, typename Combiner = optional_last_value<bool>>
struct signal
{
    template <typename... Args>
    typename Combiner::result_type operator()(Args&&... args)
    {
        return {};
    }

    template <typename Callable, typename... Args>
    connection connect(Callable, Args&&... args)
    {
        return {};
    }

    bool empty() const
    {
        return true;
    }

};

} // namespace btcsignals
#endif

#endif // BITCOIN_BTCSIGNALS_H
