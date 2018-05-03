// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_THREADVAL_H
#define BITCOIN_THREADVAL_H

#include <string>

namespace thread_data
{
    bool set_internal_name(std::string);
    std::string get_internal_name();
    long get_internal_id();
} // namespace thread_data

#endif // BITCOIN_THREADVAL_H
