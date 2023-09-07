// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINMAGIC_H
#define BITCOIN_CHAINMAGIC_H

#include <cstddef>

namespace chainmagic {
    static constexpr size_t MAGIC_SIZE = 4;
    typedef unsigned char Magic[MAGIC_SIZE];
}

#endif // BITCOIN_CHAINMAGIC_H
