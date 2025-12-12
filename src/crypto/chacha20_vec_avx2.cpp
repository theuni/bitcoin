// Copyright (c) 2025-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define CHACHA20_NAMESPACE chacha20_vec_avx2

#define CHACHA20_VEC_DISABLE_STATES_16
#define CHACHA20_VEC_DISABLE_STATES_8

#include <crypto/chacha20_vec.ipp>
