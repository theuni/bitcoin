// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_BITCOINKERNEL_H
#define BITCOIN_KERNEL_BITCOINKERNEL_H

#include <sync.h> // For RecursiveMutex

#include <atomic> // For std::atomic_bool
#include <optional>

class CClientUIInterface;
class ChainstateManager;
class CChainParams;
class CTxMemPool;
struct bilingual_str;
enum class BlockFilterType : uint8_t;

extern RecursiveMutex cs_main;

void HelloKernel();

enum class ChainstateActivationError {
    ERROR_LOADING_BLOCK_DB,
    ERROR_BAD_GENESIS_BLOCK,
    ERROR_PRUNED_NEEDS_REINDEX,
    ERROR_LOAD_GENESIS_BLOCK_FAILED,
    ERROR_CHAINSTATE_UPGRADE_FAILED,
    ERROR_REPLAYBLOCKS_FAILED,
    ERROR_LOADCHAINTIP_FAILED,
    ERROR_GENERIC_BLOCKDB_OPEN_FAILED,
    ERROR_BLOCKS_WITNESS_INSUFFICIENTLY_VALIDATED,
    ERROR_BLOCK_FROM_FUTURE,
    ERROR_CORRUPTED_BLOCK_DB,
};
// ERROR_BLOCKS_WITNESS_INSUFFICIENTLY_VALIDATED, needs int

// std::unique_ptr<ChainstateManager> MakeFullyInitializedChainstateManager(std::atomic_bool& fReindex,
//                                                                          CClientUIInterface& uiInterface,
//                                                                          CTxMemPool* mempool,
//                                                                          bool fPruneMode,
//                                                                          const CChainParams& chainparams,
//                                                                          bool fReindexChainState,
//                                                                          int64_t nBlockTreeDBCache,
//                                                                          int64_t nCoinDBCache,
//                                                                          int64_t nCoinCacheUsage,
//                                                                          unsigned int check_blocks,
//                                                                          unsigned int check_level,
//                                                                          bool block_tree_db_in_memory);

std::optional<ChainstateActivationError> ActivateChainstateSequence(bool fReset,
                                                                    CClientUIInterface& uiInterface,
                                                                    ChainstateManager& chainman,
                                                                    CTxMemPool* mempool,
                                                                    bool fPruneMode,
                                                                    const CChainParams& chainparams,
                                                                    bool fReindexChainState,
                                                                    int64_t nBlockTreeDBCache,
                                                                    int64_t nCoinDBCache,
                                                                    int64_t nCoinCacheUsage,
                                                                    unsigned int check_blocks,
                                                                    unsigned int check_level,
                                                                    bool block_tree_db_in_memory);

#endif // BITCOIN_KERNEL_BITCOINKERNEL_H
