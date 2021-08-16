// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_BITCOINKERNEL_H
#define BITCOIN_KERNEL_BITCOINKERNEL_H

#include <sync.h> // For RecursiveMutex

#include <atomic> // For std::atomic_bool
#include <set> // For std::set

class CClientUIInterface;
class ChainstateManager;
class CChainParams;
class ArgsManager;
class CTxMemPool;
enum class BlockFilterType : uint8_t;

extern RecursiveMutex cs_main;

void HelloKernel();

bool ActivateChainstateSequence(std::atomic_bool& fReindex,
                                CClientUIInterface& uiInterface,
                                ChainstateManager& chainman,
                                CTxMemPool* mempool,
                                bool& fPruneMode,
                                const CChainParams& chainparams,
                                bool fReindexChainState,
                                int64_t nBlockTreeDBCache,
                                int64_t nCoinDBCache,
                                int64_t nCoinCacheUsage,
                                unsigned int check_blocks,
                                unsigned int check_level);

#endif // BITCOIN_KERNEL_BITCOINKERNEL_H
