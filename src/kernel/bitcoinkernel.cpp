// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <kernel/bitcoinkernel.h>

#include <iostream>

#include <blockfilter.h> // For BlockFilterType
#include <chainparams.h> // For CChainParams
#include <rpc/blockchain.h> // For RPCNotifyBlockChange
#include <util/time.h> // For GetTimeMillis
#include <util/translation.h> // For bilingual_str
#include <node/blockstorage.h> // For CleanupBlockRevFiles, fHavePruned
#include <node/ui_interface.h> // For InitError, CClientUIInterface member access
#include <shutdown.h> // For ShutdownRequested and AbortShutdown
#include <timedata.h> // For GetAdjustedTime
#include <validation.h> // For a lot of things

void HelloKernel() {
    LOCK(::cs_main);
    std::cout << "Hello Kernel!";
}

bool ActivateChainstateSequence(std::atomic_bool& fReindex,
                                CClientUIInterface& uiInterface,
                                ChainstateManager& chainman,
                                CTxMemPool* mempool,
                                bool& fPruneMode,
                                const CChainParams& chainparams,
                                const ArgsManager& args,
                                int64_t& nTxIndexCache,
                                int64_t& filter_index_cache,
                                std::set<BlockFilterType>& enabled_filter_types) {
    fReindex = args.GetBoolArg("-reindex", false);
    bool fReindexChainState = args.GetBoolArg("-reindex-chainstate", false);

    // cache size calculations
    int64_t nTotalCache = (args.GetArg("-dbcache", nDefaultDbCache) << 20);
    nTotalCache = std::max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
    nTotalCache = std::min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greater than nMaxDbcache
    int64_t nBlockTreeDBCache = std::min(nTotalCache / 8, nMaxBlockDBCache << 20);
    nTotalCache -= nBlockTreeDBCache;
    nTxIndexCache = std::min(nTotalCache / 8, args.GetBoolArg("-txindex", DEFAULT_TXINDEX) ? nMaxTxIndexCache << 20 : 0);
    nTotalCache -= nTxIndexCache;
    filter_index_cache = 0;
    if (!enabled_filter_types.empty()) {
        size_t n_indexes = enabled_filter_types.size();
        int64_t max_cache = std::min(nTotalCache / 8, max_filter_index_cache << 20);
        filter_index_cache = max_cache / n_indexes;
        nTotalCache -= filter_index_cache * n_indexes;
    }
    int64_t nCoinDBCache = std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23)); // use 25%-50% of the remainder for disk cache
    nCoinDBCache = std::min(nCoinDBCache, nMaxCoinsDBCache << 20); // cap total coins db cache
    nTotalCache -= nCoinDBCache;
    int64_t nCoinCacheUsage = nTotalCache; // the rest goes to in-memory cache
    int64_t nMempoolSizeMax = args.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    LogPrintf("Cache configuration:\n");
    LogPrintf("* Using %.1f MiB for block index database\n", nBlockTreeDBCache * (1.0 / 1024 / 1024));
    if (args.GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
        LogPrintf("* Using %.1f MiB for transaction index database\n", nTxIndexCache * (1.0 / 1024 / 1024));
    }
    for (BlockFilterType filter_type : enabled_filter_types) {
        LogPrintf("* Using %.1f MiB for %s block filter index database\n",
                  filter_index_cache * (1.0 / 1024 / 1024), BlockFilterTypeName(filter_type));
    }
    LogPrintf("* Using %.1f MiB for chain state database\n", nCoinDBCache * (1.0 / 1024 / 1024));
    LogPrintf("* Using %.1f MiB for in-memory UTXO set (plus up to %.1f MiB of unused mempool space)\n", nCoinCacheUsage * (1.0 / 1024 / 1024), nMempoolSizeMax * (1.0 / 1024 / 1024));

    bool fLoaded = false;
    while (!fLoaded && !ShutdownRequested()) {
        const bool fReset = fReindex;
        auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
            return fReset || fReindexChainState || chainstate->CoinsTip().GetBestBlock().IsNull();
        };
        bilingual_str strLoadError;

        uiInterface.InitMessage(_("Loading block index…").translated);

        do {
            const int64_t load_block_index_start_time = GetTimeMillis();
            try {
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

                if (ShutdownRequested()) break;

                // LoadBlockIndex will load fHavePruned if we've ever removed a
                // block file from disk.
                // Note that it also sets fReindex based on the disk flag!
                // From here on out fReindex and fReset mean something different!
                if (!chainman.LoadBlockIndex()) {
                    if (ShutdownRequested()) break;
                    strLoadError = _("Error loading block database");
                    break;
                }

                // If the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way around).
                if (!chainman.BlockIndex().empty() &&
                        !chainman.m_blockman.LookupBlockIndex(chainparams.GetConsensus().hashGenesisBlock)) {
                    return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));
                }

                // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
                // in the past, but is now trying to run unpruned.
                if (fHavePruned && !fPruneMode) {
                    strLoadError = _("You need to rebuild the database using -reindex to go back to unpruned mode.  This will redownload the entire blockchain");
                    break;
                }

                // At this point blocktree args are consistent with what's on disk.
                // If we're not mid-reindex (based on disk + args), add a genesis block on disk
                // (otherwise we use the one already on disk).
                // This is called again in ThreadImport after the reindex completes.
                if (!fReindex && !chainman.ActiveChainstate().LoadGenesisBlock()) {
                    strLoadError = _("Error initializing block database");
                    break;
                }

                // At this point we're either in reindex or we've loaded a useful
                // block tree into BlockIndex()!

                bool failed_chainstate_init = false;

                for (CChainState* chainstate : chainman.GetAll()) {
                    chainstate->InitCoinsDB(
                        /* cache_size_bytes */ nCoinDBCache,
                        /* in_memory */ false,
                        /* should_wipe */ fReset || fReindexChainState);

                    chainstate->CoinsErrorCatcher().AddReadErrCallback([&uiInterface]() {
                        uiInterface.ThreadSafeMessageBox(
                            _("Error reading from database, shutting down."),
                            "", CClientUIInterface::MSG_ERROR);
                    });

                    // If necessary, upgrade from older database format.
                    // This is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
                    if (!chainstate->CoinsDB().Upgrade()) {
                        strLoadError = _("Error upgrading chainstate database");
                        failed_chainstate_init = true;
                        break;
                    }

                    // ReplayBlocks is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
                    if (!chainstate->ReplayBlocks()) {
                        strLoadError = _("Unable to replay blocks. You will need to rebuild the database using -reindex-chainstate.");
                        failed_chainstate_init = true;
                        break;
                    }

                    // The on-disk coinsdb is now in a good state, create the cache
                    chainstate->InitCoinsCache(nCoinCacheUsage);
                    assert(chainstate->CanFlushToDisk());

                    if (!is_coinsview_empty(chainstate)) {
                        // LoadChainTip initializes the chain based on CoinsTip()'s best block
                        if (!chainstate->LoadChainTip()) {
                            strLoadError = _("Error initializing block database");
                            failed_chainstate_init = true;
                            break; // out of the per-chainstate loop
                        }
                        assert(chainstate->m_chain.Tip() != nullptr);
                    }
                }

                if (failed_chainstate_init) {
                    break; // out of the chainstate activation do-while
                }
            } catch (const std::exception& e) {
                LogPrintf("%s\n", e.what());
                strLoadError = _("Error opening block database");
                break;
            }

            if (!fReset) {
                LOCK(cs_main);
                auto chainstates{chainman.GetAll()};
                if (std::any_of(chainstates.begin(), chainstates.end(),
                                [](const CChainState* cs) EXCLUSIVE_LOCKS_REQUIRED(cs_main) { return cs->NeedsRedownload(); })) {
                    strLoadError = strprintf(_("Witness data for blocks after height %d requires validation. Please restart with -reindex."),
                                             chainparams.GetConsensus().SegwitHeight);
                    break;
                }
            }

            bool failed_verification = false;

            try {
                LOCK(cs_main);

                for (CChainState* chainstate : chainman.GetAll()) {
                    if (!is_coinsview_empty(chainstate)) {
                        uiInterface.InitMessage(_("Verifying blocks…").translated);
                        if (fHavePruned && args.GetArg("-checkblocks", DEFAULT_CHECKBLOCKS) > MIN_BLOCKS_TO_KEEP) {
                            LogPrintf("Prune: pruned datadir may not have more than %d blocks; only checking available blocks\n",
                                MIN_BLOCKS_TO_KEEP);
                        }

                        const CBlockIndex* tip = chainstate->m_chain.Tip();
                        RPCNotifyBlockChange(tip);
                        if (tip && tip->nTime > GetAdjustedTime() + 2 * 60 * 60) {
                            strLoadError = _("The block database contains a block which appears to be from the future. "
                                    "This may be due to your computer's date and time being set incorrectly. "
                                    "Only rebuild the block database if you are sure that your computer's date and time are correct");
                            failed_verification = true;
                            break;
                        }

                        if (!CVerifyDB().VerifyDB(
                                *chainstate, chainparams, chainstate->CoinsDB(),
                                args.GetArg("-checklevel", DEFAULT_CHECKLEVEL),
                                args.GetArg("-checkblocks", DEFAULT_CHECKBLOCKS))) {
                            strLoadError = _("Corrupted block database detected");
                            failed_verification = true;
                            break;
                        }
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("%s\n", e.what());
                strLoadError = _("Error opening block database");
                failed_verification = true;
                break;
            }

            if (!failed_verification) {
                fLoaded = true;
                LogPrintf(" block index %15dms\n", GetTimeMillis() - load_block_index_start_time);
            }
        } while(false);

        if (!fLoaded && !ShutdownRequested()) {
            // first suggest a reindex
            if (!fReset) {
                bool fRet = uiInterface.ThreadSafeQuestion(
                    strLoadError + Untranslated(".\n\n") + _("Do you want to rebuild the block database now?"),
                    strLoadError.original + ".\nPlease restart with -reindex or -reindex-chainstate to recover.",
                    "", CClientUIInterface::MSG_ERROR | CClientUIInterface::BTN_ABORT);
                if (fRet) {
                    fReindex = true;
                    AbortShutdown();
                } else {
                    LogPrintf("Aborted block database rebuild. Exiting.\n");
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }
    return true;
}
