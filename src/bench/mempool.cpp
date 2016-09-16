// Copyright (c) 2016 the Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"

#include "main.h"
#include "test/test_bitcoin.h"

#include <vector>
#include <string>


static void Basic(benchmark::State& state)
{
    CTxMemPool testPool(CFeeRate(0));
    while (state.KeepRunning()) {
    }
}

BENCHMARK(Basic);
