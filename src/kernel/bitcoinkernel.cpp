// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <kernel/bitcoinkernel.h>

#include <iostream>

#include <chainparams.h> // For CChainParams
#include <node/blockstorage.h> // For CleanupBlockRevFiles, fHavePruned
#include <validation.h> // For a lot of things

void HelloKernel() {
    LOCK(::cs_main);
    std::cout << "Hello Kernel!";
}

// template <typename ... Args>
// std::unique_ptr<ChainstateManager> MakeFullyInitializedChainstateManager(Args&& ... args){
//     auto chainman = std::make_unique<ChainstateManager>();
//     bool rv = ActivateChainstateSequence(*chainman, std::forward<Args>(args) ...);
//     if (!rv) {
//         chainman.reset();
//     }
//     return std::move(chainman);
// }


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
//                                                                          bool block_tree_db_in_memory) {
//     auto chainman = std::make_unique<ChainstateManager>();
//     bool rv = ActivateChainstateSequence(fReindex,
//                                          uiInterface,
//                                          *chainman,
//                                          mempool,
//                                          fPruneMode,
//                                          chainparams,
//                                          fReindexChainState,
//                                          nBlockTreeDBCache,
//                                          nCoinDBCache,
//                                          nCoinCacheUsage,
//                                          check_blocks,
//                                          check_level,
//                                          block_tree_db_in_memory);
//     if (!rv) {
//         chainman.reset();
//     }
//     return std::move(chainman);
// }

// rv = false -> bail immediately, do nothing else
std::optional<ChainstateActivationError> ActivateChainstateSequence(bool fReset,
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
                                                                    bool block_tree_db_in_memory,
                                                                    std::function<int64_t()> get_adjusted_time,
                                                                    std::function<bool()> shutdown_requested,
                                                                    std::optional<std::function<void()>> coins_error_cb,
                                                                    std::function<void()> verifying_blocks_cb) {
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return fReset || fReindexChainState || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    // Thought of the day: what if we add initialize functions to all of
    // BlockManager, Chainstate, ChainstateManager?

    {
        LOCK(cs_main);
        chainman.InitializeChainstate(mempool);
        chainman.m_total_coinstip_cache = nCoinCacheUsage;
        chainman.m_total_coinsdb_cache = nCoinDBCache;

        UnloadBlockIndex(mempool, chainman);

        auto& pblocktree{chainman.m_blockman.m_block_tree_db};
        // new CBlockTreeDB tries to delete the existing file, which
        // fails if it's still open from the previous loop. Close it first:
        pblocktree.reset();
        pblocktree.reset(new CBlockTreeDB(nBlockTreeDBCache, false, fReset));

        if (fReset) {
            pblocktree->WriteReindexing(true);
            //If we're reindexing in prune mode, wipe away unusable block files and all undo data files
            if (fPruneMode)
                CleanupBlockRevFiles();
        }

        if (shutdown_requested()) return std::nullopt;

        // LoadBlockIndex will load fHavePruned if we've ever removed a
        // block file from disk.
        // Note that it also sets fReindex based on the disk flag!
        // From here on out fReindex and fReset mean something different!
        if (!chainman.LoadBlockIndex()) {
            if (shutdown_requested()) return std::nullopt;
            return ChainstateActivationError::ERROR_LOADING_BLOCK_DB;
        }

        // If the loaded chain has a wrong genesis, bail out immediately
        // (we're likely using a testnet datadir, or the other way around).
        if (!chainman.BlockIndex().empty() &&
                !chainman.m_blockman.LookupBlockIndex(chainparams.GetConsensus().hashGenesisBlock)) {
            return ChainstateActivationError::ERROR_BAD_GENESIS_BLOCK;
        }

        // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
        // in the past, but is now trying to run unpruned.
        if (fHavePruned && !fPruneMode) {
            return ChainstateActivationError::ERROR_PRUNED_NEEDS_REINDEX;
        }

        // At this point blocktree args are consistent with what's on disk.
        // If we're not mid-reindex (based on disk + args), add a genesis block on disk
        // (otherwise we use the one already on disk).
        // This is called again in ThreadImport after the reindex completes.
        if (!fReindex && !chainman.ActiveChainstate().LoadGenesisBlock()) {
            return ChainstateActivationError::ERROR_LOAD_GENESIS_BLOCK_FAILED;
        }

        // At this point we're either in reindex or we've loaded a useful
        // block tree into BlockIndex()!

        // bool failed_chainstate_init = false;

        for (CChainState* chainstate : chainman.GetAll()) {
            chainstate->InitCoinsDB(
                /* cache_size_bytes */ nCoinDBCache,
                /* in_memory */ false,
                /* should_wipe */ fReset || fReindexChainState);

            if (coins_error_cb.has_value()) {
                chainstate->CoinsErrorCatcher().AddReadErrCallback(coins_error_cb.value());
            }

            // If necessary, upgrade from older database format.
            // This is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
            if (!chainstate->CoinsDB().Upgrade()) {
                return ChainstateActivationError::ERROR_CHAINSTATE_UPGRADE_FAILED;
            }

            // ReplayBlocks is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
            if (!chainstate->ReplayBlocks()) {
                return ChainstateActivationError::ERROR_REPLAYBLOCKS_FAILED;
            }

            // The on-disk coinsdb is now in a good state, create the cache
            chainstate->InitCoinsCache(nCoinCacheUsage);
            assert(chainstate->CanFlushToDisk());

            if (!is_coinsview_empty(chainstate)) {
                // LoadChainTip initializes the chain based on CoinsTip()'s best block
                if (!chainstate->LoadChainTip()) {
                    return ChainstateActivationError::ERROR_LOADCHAINTIP_FAILED;
                }
                assert(chainstate->m_chain.Tip() != nullptr);
            }
        }
    }

    if (!fReset) {
        LOCK(cs_main);
        auto chainstates{chainman.GetAll()};
        if (std::any_of(chainstates.begin(), chainstates.end(),
                        [](const CChainState* cs) EXCLUSIVE_LOCKS_REQUIRED(cs_main) { return cs->NeedsRedownload(); })) {
            return ChainstateActivationError::ERROR_BLOCKS_WITNESS_INSUFFICIENTLY_VALIDATED;
        }
    }

    // bool failed_verification = false;

    {
        LOCK(cs_main);

        for (CChainState* chainstate : chainman.GetAll()) {
            if (!is_coinsview_empty(chainstate)) {
                verifying_blocks_cb();

                const CBlockIndex* tip = chainstate->m_chain.Tip();
                if (tip && tip->nTime > get_adjusted_time() + 2 * 60 * 60) {
                    return ChainstateActivationError::ERROR_BLOCK_FROM_FUTURE;
                }

                if (!CVerifyDB().VerifyDB(
                        *chainstate, chainparams, chainstate->CoinsDB(),
                        check_level,
                        check_blocks)) {
                    return ChainstateActivationError::ERROR_CORRUPTED_BLOCK_DB;
                }
            }
        }
    }

    return std::nullopt;
}
