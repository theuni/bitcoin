#include <iostream> // for cout and shit
#include <functional> // for std::function

#include <node/chainstate.h>          // for LoadChainstate
#include <kernel/init/common.h>              // for SetGlobals, UnsetGlobals
#include <validation.h>               // for ChainstateManager, InitScriptExecutionCache, StopScriptCheckWorkerThreads, UpdateUncommittedBlockStructures, BlockManager, DEFAULT_CHECKBLOCKS, DEFAULT_CHECKLEVEL
#include <validationinterface.h>      // for GetMainSignals, cs_main, CMainSignals, RegisterSharedValidationInterface, UnregisterSharedValidationInterface, CValidationInterface
#include <consensus/validation.h>     // for BlockValidationState
#include <chainparams.h>              // for Params, SelectParams, CChainParams
#include <node/blockstorage.h>        // for fReindex
#include <node/import.h>              // for BlockImport
#include <util/args.h>                // for gArgs, ArgsManager
#include <scheduler.h>                // for CScheduler
#include <script/sigcache.h>          // for InitSignatureCache

using node::fReindex;
using node::LoadChainstate;
using node::BlockImport;
using node::nPruneTarget;
using node::fPruneMode;

const extern std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

bool ValidateArgs()
{
    int64_t nPruneArg = gArgs.GetIntArg("-prune", 0);
    if (nPruneArg < 0) {
        std::cout << "Prune cannot be configured with a negative value." << std::endl;
        return false;
    } else if (nPruneArg > 1 && (nPruneArg * 1024 * 1024) < MIN_DISK_SPACE_FOR_BLOCK_FILES) {
        std::cout << "Prune configured below the minimum of " << MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024 << "MiB.  Please use a higher number." << std::endl;
        return false;
    }
    return true;
}

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

    fReindex = gArgs.GetBoolArg("-reindex", false);
    bool fReindexChainState = gArgs.GetBoolArg("-reindex-chainstate", false);
    std::vector<fs::path> vImportFiles;
    for (const std::string& strFile : gArgs.GetArgs("-loadblock")) {
        vImportFiles.push_back(fs::PathFromString(strFile));
    }


    // block pruning; get the amount of disk space (in MiB) to allot for block & undo files
    int64_t nPruneArg = gArgs.GetIntArg("-prune", 0);
    if (nPruneArg < 0) {
       std::cout << "Prune cannot be configured with a negative value." << std::endl;
       goto done;
    }
    nPruneTarget = (uint64_t) nPruneArg * 1024 * 1024;
    if (nPruneArg == 1) {  // manual pruning: -prune=1
        std::cout << "Block pruning enabled.  Use RPC call pruneblockchain(height) to manually prune block and undo files." << std::endl;
        nPruneTarget = std::numeric_limits<uint64_t>::max();
        fPruneMode = true;
    } else if (nPruneTarget) {
        if (nPruneTarget < MIN_DISK_SPACE_FOR_BLOCK_FILES) {
            std::cout << "Prune configured below the minimum of " << MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024 << "MiB.  Please use a higher number." << std::endl;
            return 1;
        }
        std::cout << "Prune configured to target " << nPruneTarget / 1024 / 1024 << "MiB on disk for block and undo files." << std::endl;
        fPruneMode = true;
    }
    {
        auto rv = chainman.LoadChainstate(false,
                             nullptr,
                             false,
                             chainparams.GetConsensus(),
                             fReindexChainState,
                             2 << 20,
                             2 << 22,
                             (450 << 20) - (2 << 20) - (2 << 22),
                             true,
                             true,
                             [](){ return false; });
        if (rv.has_value()) {
            goto done;
        }
        auto ret = BlockImport(chainman, vImportFiles);
        if (ret.ShouldEarlyExit()) {
            std::cout << "Encountered a fatal error";
            goto done;
        }

        std::cout << "Hello! I'm going to print out some information about your datadir." << std::endl;
        std::cout << "\t" << "Path: " << gArgs.GetDataDirNet() << std::endl;
        std::cout << "\t" << "Snapshot Active: " << std::boolalpha << chainman.IsSnapshotActive() << std::noboolalpha << std::endl;
        std::cout << "\t" << "Active Height: " << chainman.ActiveHeight() << std::endl;
        {
            CBlockIndex* tip = chainman.ActiveTip();
            if (tip) {
                std::cout << "\t" << tip->ToString() << std::endl;
            }
        }
    }
done:
    scheduler.stop();
    if (chainman.m_load_block.joinable()) chainman.m_load_block.join();
    StopScriptCheckWorkerThreads();

    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();

    init::UnsetGlobals();
}
