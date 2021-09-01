#include <iostream> // for cout and shit
#include <functional> // for std::function

#include <util/init.h> // for CacheSizes + CalculateCacheSizes
#include <kernel/bitcoinkernel.h> // for ActivateChainstateSequence
#include <validation.h> // for ChainstateManager
#include <validationinterface.h> // for CValidationInterface
#include <core_io.h> // for DecodeHexBlk
#include <chainparams.h> // for Params()
#include <node/blockstorage.h> // for fReindex, fPruneMode
#include <timedata.h> // for GetAdjustedTime
#include <shutdown.h> // for ShutdownRequested
#include <util/system.h> // for ArgsManager
#include <util/thread.h> // for TraceThread
#include <scheduler.h> // for CScheduler
#include <script/sigcache.h> // for InitSignatureCache
#include <key.h> // for ECC_{Start,Stop}

const extern std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

class submitblock_StateCatcher final : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    BlockValidationState state;

    explicit submitblock_StateCatcher(const uint256 &hashIn) : hash(hashIn), found(false), state() {}

protected:
    void BlockChecked(const CBlock& block, const BlockValidationState& stateIn) override {
        if (block.GetHash() != hash)
            return;
        found = true;
        state = stateIn;
    }
};

int main() {


    ECCVerifyHandle globalVerifyHandle; // initializes secp256k1_context_verify

    ArgsManager args{};

    SelectParams(CBaseChainParams::MAIN);
    const CChainParams& chainparams = Params();

    ECC_Start();

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

    // START scheduler for RegisterSharedValidationInterface
    CScheduler scheduler{};
    // Start the lightweight task scheduler thread
    scheduler.m_service_thread = std::thread(util::TraceThread, "scheduler", [&] { scheduler.serviceQueue(); });

    // Gather some entropy once per minute.
    // scheduler.scheduleEvery([]{
    //     RandAddPeriodic();
    // }, std::chrono::minutes{1});
    // END scheduler for RegisterSharedValidationInterface

    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

    CacheSizes cache_sizes;
    CalculateCacheSizes(args, 0, &cache_sizes);

    ChainstateManager chainman;

    auto rv = ActivateChainstateSequence(fReindex.load(),
                                         std::ref(chainman),
                                         nullptr,
                                         fPruneMode,
                                         chainparams,
                                         false,
                                         cache_sizes.block_tree_db_cache_size,
                                         cache_sizes.coin_db_cache_size,
                                         cache_sizes.coin_cache_usage_size,
                                         DEFAULT_CHECKBLOCKS,
                                         DEFAULT_CHECKLEVEL,
                                         true,
                                         GetAdjustedTime,
                                         ShutdownRequested);
    if (rv) {
        std::cerr << "Failed to load Chain state from your datadir." << std::endl;
        return 1;
    }

    std::cout << "Hello! I'm going to print out some information about your datadir." << std::endl;
    std::cout << "\t" << "Snapshot Active: " << std::boolalpha << chainman.IsSnapshotActive() << std::noboolalpha << std::endl;
    std::cout << "\t" << "Active Height: " << chainman.ActiveHeight() << std::endl;
    std::cout << "\t" << chainman.ActiveTip()->ToString() << std::endl;

    for (std::string line; std::getline(std::cin, line); ) {
        if (line.empty()) {
            std::cerr << "Empty line found" << std::endl;
            return 1;
        }

        std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
        CBlock& block = *blockptr;

        if (!DecodeHexBlk(block, line)) {
            std::cerr << "Block decode failed" << std::endl;
            return 1;
        }

        if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
            std::cerr << "Block does not start with a coinbase" << std::endl;
            return 1;
        }

        uint256 hash = block.GetHash();
        {
            LOCK(cs_main);
            const CBlockIndex* pindex = chainman.m_blockman.LookupBlockIndex(hash);
            if (pindex) {
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                    std::cerr << "duplicate" << std::endl;
                    return 1;
                }
                if (pindex->nStatus & BLOCK_FAILED_MASK) {
                    std::cerr << "duplicate-invalid" << std::endl;
                    return 1;
                }
            }
        }

        {
            LOCK(cs_main);
            const CBlockIndex* pindex = chainman.m_blockman.LookupBlockIndex(block.hashPrevBlock);
            if (pindex) {
                UpdateUncommittedBlockStructures(block, pindex, chainparams.GetConsensus());
            }
        }

        bool new_block;
        std::cerr << "fore" << std::endl;
        auto sc = std::make_shared<submitblock_StateCatcher>(block.GetHash());
        std::cerr << "1" << std::endl;
        RegisterSharedValidationInterface(sc);
        std::cerr << "2" << std::endl;
        bool accepted = chainman.ProcessNewBlock(chainparams, blockptr, /* fForceProcessing */ true, /* fNewBlock */ &new_block);
        std::cerr << "3" << std::endl;
        UnregisterSharedValidationInterface(sc);
        std::cerr << "aft" << std::endl;
        if (!new_block && accepted) {
            std::cerr << "duplicate" << std::endl;
            return 1;
        }
        if (!sc->found) {
            std::cerr << "inconclusive" << std::endl;
            return 1;
        }
    }

    ECC_Stop();

    // scheduler.stop();
    std::cerr << "schfore" << std::endl;
    scheduler.stop();
    std::cerr << "schaft" << std::endl;
    if (chainman.m_load_block.joinable()) chainman.m_load_block.join();
    StopScriptCheckWorkerThreads();

    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
}
