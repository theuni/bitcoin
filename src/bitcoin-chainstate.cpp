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
#include <key.h> // for ECC_{Start,Stop}
#include <init/common.h> // for ECC_{Start,Stop}

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
    // gArgs initialized statically

    // SelectParams(CBaseChainParams::MAIN);
    // const CChainParams& chainparams = Params();

    // InitGlobals();
    // InitCaches();
    auto zero = StepZero(CBaseChainParams::MAIN);
    auto& chainparams = zero->GetChainParams();

    // int num_script_check_threads = 1;
    // StartScriptThreads(num_script_check_threads);
    // LogPrintf("Script verification uses %d additional threads\n", num_script_check_threads);

    // std::unique_ptr<CScheduler> scheduler_ptr = StartScheduler();
    // CScheduler& scheduler = *scheduler_ptr;
    // StartMainSignals(scheduler);
    auto one = StepOne(std::move(zero), 1);

    CacheSizes cache_sizes;
    CalculateCacheSizes(gArgs, 0, &cache_sizes);

    ChainstateManager chainman = ChainstateManager{};

    auto rv = ActivateChainstateSequence(fReindex.load(),
                                         chainman,
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
            break;
        }

        std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
        CBlock& block = *blockptr;

        if (!DecodeHexBlk(block, line)) {
            std::cerr << "Block decode failed" << std::endl;
            break;
        }

        if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
            std::cerr << "Block does not start with a coinbase" << std::endl;
            break;
        }

        uint256 hash = block.GetHash();
        {
            LOCK(cs_main);
            const CBlockIndex* pindex = chainman.m_blockman.LookupBlockIndex(hash);
            if (pindex) {
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                    std::cerr << "duplicate" << std::endl;
                    break;
                }
                if (pindex->nStatus & BLOCK_FAILED_MASK) {
                    std::cerr << "duplicate-invalid" << std::endl;
                    break;
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
            break;
        }
        if (!sc->found) {
            std::cerr << "inconclusive" << std::endl;
            break;
        }
    }

    // After everything has been shut down, but before things get flushed, stop the
    // CScheduler/checkqueue, scheduler and load block thread.
    // scheduler.stop();
    // if (chainman.m_load_block.joinable()) chainman.m_load_block.join();
    // StopScriptCheckWorkerThreads();

    // GetMainSignals().FlushBackgroundCallbacks();
    // GetMainSignals().UnregisterBackgroundSignalScheduler();
}
