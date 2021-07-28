// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_BITCOINKERNEL_H
#define BITCOIN_KERNEL_BITCOINKERNEL_H

#include <sync.h>

extern RecursiveMutex cs_main;

void HelloKernel();

#endif // BITCOIN_KERNEL_BITCOINKERNEL_H
