#include <iostream> // for cout and shit
#include <functional> // for std::function

#include <node/chainstate.h>          // for LoadChainstate
#include <init/common.h>              // for SetGlobals, UnsetGlobals
#include <validation.h>               // for ChainstateManager, InitScriptExecutionCache, StopScriptCheckWorkerThreads, UpdateUncommittedBlockStructures, BlockManager, DEFAULT_CHECKBLOCKS, DEFAULT_CHECKLEVEL
#include <validationinterface.h>      // for GetMainSignals, cs_main, CMainSignals, RegisterSharedValidationInterface, UnregisterSharedValidationInterface, CValidationInterface
#include <consensus/validation.h>     // for BlockValidationState
#include <chainparams.h>              // for Params, SelectParams, CChainParams
#include <node/blockstorage.h>        // for fReindex
#include <util/system.h>              // for gArgs, ArgsManager
#include <scheduler.h>                // for CScheduler
#include <script/sigcache.h>          // for InitSignatureCache

using node::fReindex;
using node::LoadChainstate;

const extern std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

static std::vector<uint8_t> DecodeHex(const std::string& hex_string) {
    std::vector<uint8_t> rv;
    assert(hex_string.size() % 2 == 0);
    rv.reserve(hex_string.size() / 2);

    for (size_t i = 0; i < hex_string.length(); i += 2) {
        std::string sub_hex = hex_string.substr(i, 2);
        auto ul = strtoul(sub_hex.c_str(), NULL, 16);
        assert(ul <= UINT8_MAX);
        rv.push_back(ul);
    }
    return std::move(rv);
}

static bool DecodeHexBlk(CBlock& block, const std::string& hex_string) {
    CDataStream ssBlock(DecodeHex(hex_string), SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssBlock >> block;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

// Adapted from rpc/mining.cpp
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
    SelectParams(CBaseChainParams::MAIN);
    const CChainParams& chainparams = Params();

    init::SetGlobals();  // ECC_Start, etc.

    // Initialize signatureCache cuz it's used by...
    //     <- VerifyECDSASignature
    //     <- CheckECDSASignature
    //     <- EvalChecksigPreTapscript
    //     <- EvalScript
    //     <- VerifyScript
    //     <- CScriptCheck::()
    //     <- CheckInputScripts
    //     <- ConnectBlock
    //     <- ConnectTip
    //     <- ActivateBestChainStep
    //     <- ActivateBestChain
    //     <- ProcessNewBlock
    InitSignatureCache();

    // Initialize g_scriptExecutionCache{,Hasher} cuz it's used by...
    //     <- CheckInputScripts
    //     <- ConnectBlock
    //     <- ConnectTip
    //     <- ActivateBestChainStep
    //     <- ActivateBestChain
    //     <- ProcessNewBlock
    InitScriptExecutionCache();

    // START scheduler for RegisterSharedValidationInterface
    CScheduler scheduler{};
    // Start the lightweight task scheduler thread
    scheduler.m_service_thread = std::thread([&scheduler] { scheduler.serviceQueue(); });

    // Gather some entropy once per minute.
    scheduler.scheduleEvery([]{
        RandAddPeriodic();
    }, std::chrono::minutes{1});
    // END scheduler for RegisterSharedValidationInterface

    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

    ChainstateManager chainman;

    auto rv = LoadChainstate(false,
                             std::ref(chainman),
                             nullptr,
                             false,
                             chainparams.GetConsensus(),
                             false,
                             2 << 20,
                             2 << 22,
                             (450 << 20) - (2 << 20) - (2 << 22),
                             true,
                             true,
                             [](){ return false; });
    if (rv.has_value()) {
        std::cerr << "Failed to load Chain state from your datadir." << std::endl;
        goto prologue;
    }

    for (CChainState* chainstate : WITH_LOCK(::cs_main, return chainman.GetAll())) {
        BlockValidationState state;
        if (!chainstate->ActivateBestChain(state, nullptr)) {
            std::cerr << "Failed to connect best block (" << state.ToString() << ")" << std::endl;
            goto prologue;
        }
    }

    std::cout << "Hello! I'm going to print out some information about your datadir." << std::endl;
    std::cout << "\t" << "Path: " << gArgs.GetDataDirNet() << std::endl;
    std::cout << "\t" << "Reindexing: " << std::boolalpha << fReindex.load() << std::noboolalpha << std::endl;
    std::cout << "\t" << "Snapshot Active: " << std::boolalpha << chainman.IsSnapshotActive() << std::noboolalpha << std::endl;
    std::cout << "\t" << "Active Height: " << chainman.ActiveHeight() << std::endl;
    {
        CBlockIndex* tip = chainman.ActiveTip();
        if (tip) {
            std::cout << "\t" << tip->ToString() << std::endl;
        }
    }

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
        auto sc = std::make_shared<submitblock_StateCatcher>(block.GetHash());
        RegisterSharedValidationInterface(sc);
        bool accepted = chainman.ProcessNewBlock(chainparams, blockptr, /* fForceProcessing */ true, /* fNewBlock */ &new_block);
        UnregisterSharedValidationInterface(sc);
        if (!new_block && accepted) {
            std::cerr << "duplicate" << std::endl;
            break;
        }
        if (!sc->found) {
            std::cerr << "inconclusive" << std::endl;
            break;
        }
        std::cout << sc->state.ToString() << std::endl;
        switch (sc->state.GetResult()) {
        case BlockValidationResult::BLOCK_RESULT_UNSET:
            std::cerr << "initial value. Block has not yet been rejected" << std::endl;
            break;
        case BlockValidationResult::BLOCK_CONSENSUS:
            std::cerr << "invalid by consensus rules (excluding any below reasons)" << std::endl;
            break;
        case BlockValidationResult::BLOCK_RECENT_CONSENSUS_CHANGE:
            std::cerr << "Invalid by a change to consensus rules more recent than SegWit." << std::endl;
            break;
        case BlockValidationResult::BLOCK_CACHED_INVALID:
            std::cerr << "this block was cached as being invalid and we didn't store the reason why" << std::endl;
            break;
        case BlockValidationResult::BLOCK_INVALID_HEADER:
            std::cerr << "invalid proof of work or time too old" << std::endl;
            break;
        case BlockValidationResult::BLOCK_MUTATED:
            std::cerr << "the block's data didn't match the data committed to by the PoW" << std::endl;
            break;
        case BlockValidationResult::BLOCK_MISSING_PREV:
            std::cerr << "We don't have the previous block the checked one is built on" << std::endl;
            break;
        case BlockValidationResult::BLOCK_INVALID_PREV:
            std::cerr << "A block this one builds on is invalid" << std::endl;
            break;
        case BlockValidationResult::BLOCK_TIME_FUTURE:
            std::cerr << "block timestamp was > 2 hours in the future (or our clock is bad)" << std::endl;
            break;
        case BlockValidationResult::BLOCK_CHECKPOINT:
            std::cerr << "the block failed to meet one of our checkpoints" << std::endl;
            break;
        }
    }

 prologue:
    scheduler.stop();
    if (chainman.m_load_block.joinable()) chainman.m_load_block.join();
    StopScriptCheckWorkerThreads();

    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();

    init::UnsetGlobals();
}
