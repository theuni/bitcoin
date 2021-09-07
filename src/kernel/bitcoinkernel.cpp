// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <kernel/bitcoinkernel.h>

#include <iostream>

#include <chainparams.h> // For CChainParams
#include <node/blockstorage.h> // For CleanupBlockRevFiles, fHavePruned
#include <validation.h> // For a lot of things
#include <validationinterface.h> // For GetMainSignals
#include <node/context.h> // For NodeContext
#include <script/sigcache.h> // For InitSignatureCache
#include <scheduler.h> // For CScheduler
#include <util/thread.h> // For util::TraceThread
#include <init/common.h> // for init::SetGlobals()

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

void InitCaches() {
    // Initialize signatureCache for
    // #1  0x000055555558a2d3 in base_blob<256u>::Compare (this=0x0, other=...) at ./uint256.h:44
    // #2  0x00007ffff79e1c0e in operator== (a=..., b=...) at ./uint256.h:46
    // #3  0x00007ffff79dffd8 in CuckooCache::cache<uint256, SignatureCacheHasher>::contains (
    //     this=0x7ffff7fc3238 <(anonymous namespace)::signatureCache+208>, e=..., erase=true) at ./cuckoocache.h:473
    // #4  0x00007ffff7ca2c83 in (anonymous namespace)::CSignatureCache::Get (
    //     this=0x7ffff7fc3168 <(anonymous namespace)::signatureCache>, entry=..., erase=true) at script/sigcache.cpp:70
    // #5  0x00007ffff7ca2a3d in CachingTransactionSignatureChecker::VerifyECDSASignature (this=0x7fffffffaeb0,
    //     vchSig=std::vector of length 72, capacity 73 = {...}, pubkey=..., sighash=...) at script/sigcache.cpp:109
    // #6  0x00007ffff7c9c68c in GenericTransactionSignatureChecker<CTransaction>::CheckECDSASignature (this=0x7fffffffaeb0,
    //     vchSigIn=std::vector of length 73, capacity 73 = {...}, vchPubKey=std::vector of length 65, capacity 65 = {...},
    //     scriptCode=..., sigversion=SigVersion::BASE) at script/interpreter.cpp:1688
    // #7  0x00007ffff7c987fd in EvalChecksigPreTapscript (vchSig=std::vector of length 73, capacity 73 = {...},
    //     vchPubKey=std::vector of length 65, capacity 65 = {...}, pbegincodehash=..., pend=..., flags=2053, checker=...,
    //     sigversion=SigVersion::BASE, serror=0x7fffffffb224, fSuccess=@0x7fffffffa527: true) at script/interpreter.cpp:363
    // #8  0x00007ffff7c95917 in EvalChecksig (sig=std::vector of length 73, capacity 73 = {...},
    //     pubkey=std::vector of length 65, capacity 65 = {...}, pbegincodehash=..., pend=..., execdata=..., flags=2053, checker=...,
    //     sigversion=SigVersion::BASE, serror=0x7fffffffb224, success=@0x7fffffffa527: true) at script/interpreter.cpp:421
    // #9  0x00007ffff7c940e9 in EvalScript (stack=std::vector of length 2, capacity 4 = {...}, script=..., flags=2053, checker=...,
    //     sigversion=SigVersion::BASE, execdata=..., serror=0x7fffffffb224) at script/interpreter.cpp:1094
    // #10 0x00007ffff7c95b6b in EvalScript (stack=std::vector of length 2, capacity 4 = {...}, script=..., flags=2053, checker=...,
    //     sigversion=SigVersion::BASE, serror=0x7fffffffb224) at script/interpreter.cpp:1264
    // #11 0x00007ffff7c969d9 in VerifyScript (scriptSig=..., scriptPubKey=..., witness=0x55555e227550, flags=2053, checker=...,
    //     serror=0x7fffffffb224) at script/interpreter.cpp:1991
    // #12 0x00007ffff79a6599 in CScriptCheck::operator() (this=0x7fffffffb1e8) at validation.cpp:1345
    // #13 0x00007ffff79a72af in CheckInputScripts (tx=..., state=..., inputs=..., flags=2053, cacheSigStore=false,
    //     cacheFullScriptStore=false, txdata=..., pvChecks=0x0) at validation.cpp:1445
    // #14 0x00007ffff79aa1f1 in CChainState::ConnectBlock (this=0x5555555eee80, block=..., state=..., pindex=0x55555a674fd0, view=...,
    //     fJustCheck=false) at validation.cpp:1927
    // #15 0x00007ffff79b3872 in CChainState::ConnectTip (this=0x5555555eee80, state=..., pindexNew=0x55555a674fd0,
    //     pblock=std::shared_ptr<const CBlock> (empty) = {...}, connectTrace=..., disconnectpool=...) at validation.cpp:2370
    // #16 0x00007ffff79b572b in CChainState::ActivateBestChainStep (this=0x5555555eee80, state=..., pindexMostWork=0x555555d48480,
    //     pblock=std::shared_ptr<const CBlock> (empty) = {...}, fInvalidFound=@0x7fffffffcf97: false, connectTrace=...)
    //     at validation.cpp:2530
    // #17 0x00007ffff79b5eaf in CChainState::ActivateBestChain (this=0x5555555eee80, state=..., pblock=warning: RTTI symbol not found for class 'std::_Sp_counted_ptr_inplace<CBlock, std::allocator<CBlock>, (__gnu_cxx::_Lock_policy)2>'
    // warning: RTTI symbol not found for class 'std::_Sp_counted_ptr_inplace<CBlock, std::allocator<CBlock>, (__gnu_cxx::_Lock_policy)2>'
    //
    // std::shared_ptr<const CBlock> (use count 3, weak count 0) = {...}) at validation.cpp:2655
    // #18 0x00007ffff79bf0c5 in ChainstateManager::ProcessNewBlock (this=0x7fffffffd548, chainparams=..., block=warning: RTTI symbol not found for class 'std::_Sp_counted_ptr_inplace<CBlock, std::allocator<CBlock>, (__gnu_cxx::_Lock_policy)2>'
    // warning: RTTI symbol not found for class 'std::_Sp_counted_ptr_inplace<CBlock, std::allocator<CBlock>, (__gnu_cxx::_Lock_policy)2>'
    //
    // std::shared_ptr<const CBlock> (use count 3, weak count 0) = {...}, force_processing=true, new_block=0x7fffffffd4cf)
    //     at validation.cpp:3499
    InitSignatureCache();
    // Initialize g_scriptExecutionCache for
    // CheckInputScripts <- ConnectBlock <- ConnectTip <- ActivateBestChainStep <- ActivateBestChain <- ProcessNewBlock
    InitScriptExecutionCache();
}

void StartScriptThreads(int total_script_threads) {
    if (total_script_threads <= 0) {
        // -par=0 means autodetect (number of cores - 1 script threads)
        // -par=-n means "leave n cores free" (number of cores - n - 1 script threads)
        total_script_threads += GetNumCores();
    }

    // Subtract 1 because the main thread counts towards the par threads
    auto script_threads_to_start = std::max(total_script_threads - 1, 0);

    // Number of script-checking threads <= MAX_SCRIPTCHECK_THREADS
    script_threads_to_start = std::min(script_threads_to_start, MAX_SCRIPTCHECK_THREADS);

    if (script_threads_to_start >= 1) {
        g_parallel_script_checks = true;
        StartScriptCheckWorkerThreads(script_threads_to_start);
    }
}

std::unique_ptr<CScheduler> StartScheduler() {
    std::unique_ptr<CScheduler> scheduler = std::make_unique<CScheduler>();
    scheduler->m_service_thread = std::thread(util::TraceThread, "scheduler", [&] { scheduler->serviceQueue(); });
    return std::move(scheduler);
}

void StartMainSignals(CScheduler& scheduler) {
    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);
}

void InitGlobals() {
    init::SetGlobals();
}

// Implicitly relies on RNGState
// Needs that data_dir is writable
// IDEA: document assumptions in comments for error codes
//
// data_dir, chain_name?
// assert(fs::is_directory(data_dir)); // CheckDataDirOption
// SelectParams(chain_name);
// const CChainParams& chainparams = Params();
// init::SetGlobals();
// void InitSequence(NodeContext& node, int num_script_check_threads) {
//     StartScriptThreads(num_script_check_threads);
//     LogPrintf("Script verification uses %d additional threads\n", num_script_check_threads);
//     StartSchedulerAndMainSignals(node);

//     // Gather some entropy once per minute. //XXX is this our responsibility?
//     node.scheduler->scheduleEvery([]{
//         RandAddPeriodic();
//     }, std::chrono::minutes{1});
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
