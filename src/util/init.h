// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stddef.h>
#include <stdint.h>

#ifndef BITCOIN_UTIL_INIT_H
#define BITCOIN_UTIL_INIT_H

class ArgsManager;

struct CacheSizes {
    int64_t block_tree_db_cache_size;
    int64_t coin_db_cache_size;
    int64_t coin_cache_usage_size;
    int64_t tx_index_cache_size;
    int64_t filter_index_cache_size;
};
void CalculateCacheSizes(const ArgsManager& args, size_t n_indexes, CacheSizes* cache_sizes);

#endif // BITCOIN_UTIL_INIT_H
