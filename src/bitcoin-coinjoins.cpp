// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// The bitcoin-chainstate executable serves to surface the dependencies required
// by a program wishing to use Bitcoin Core's consensus engine as it is right
// now.
//
// DEVELOPER NOTE: Since this is a "demo-only", experimental, etc. executable,
//                 it may diverge from Bitcoin Core's coding style.
//
// It is part of the libbitcoinkernel project.

#include <kernel/chainparams.h>
#include <kernel/chainstatemanager_opts.h>
#include <kernel/checks.h>
#include <kernel/context.h>
#include <kernel/validation_cache_sizes.h>
#include <kernel/warning.h>

#include <consensus/validation.h>
#include <core_io.h>
#include <node/blockstorage.h>
#include <node/caches.h>
#include <node/chainstate.h>
#include <random.h>
#include <script/sigcache.h>
#include <util/chaintype.h>
#include <util/coinjoins.h>
#include <util/fs.h>
#include <util/signalinterrupt.h>
#include <util/task_runner.h>
#include <util/translation.h>
#include <undo.h>
#include <validation.h>
#include <validationinterface.h>

#include <cassert>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>

// this block contains the first whirlpool coinjoin transaction
static const int FIRST_COINJOIN_HEIGHT = 572341; // 00000000000000000028802df15e53c435ae81fad26d4fb3414a5ab00ab90d5c

int main(int argc, char* argv[])
{
    // SETUP: Argument parsing and handling
    if (argc != 2) {
        std::cerr
            << "Usage: " << argv[0] << " DATADIR" << std::endl
            << "Display DATADIR information, and process hex-encoded blocks on standard input." << std::endl
            << std::endl
            << "IMPORTANT: THIS EXECUTABLE IS EXPERIMENTAL, FOR TESTING ONLY, AND EXPECTED TO" << std::endl
            << "           BREAK IN FUTURE VERSIONS. DO NOT USE ON YOUR ACTUAL DATADIR." << std::endl;
        return 1;
    }
    fs::path abs_datadir{fs::absolute(argv[1])};
    fs::create_directories(abs_datadir);


    // SETUP: Context
    kernel::Context kernel_context{};
    // We can't use a goto here, but we can use an assert since none of the
    // things instantiated so far requires running the epilogue to be torn down
    // properly
    assert(kernel::SanityChecks(kernel_context));

    // Necessary for CheckInputScripts (eventually called by ProcessNewBlock),
    // which will try the script cache first and fall back to actually
    // performing the check with the signature cache.
    kernel::ValidationCacheSizes validation_cache_sizes{};
    Assert(InitSignatureCache(validation_cache_sizes.signature_cache_bytes));
    Assert(InitScriptExecutionCache(validation_cache_sizes.script_execution_cache_bytes));

    ValidationSignals validation_signals{std::make_unique<util::ImmediateTaskRunner>()};

    class KernelNotifications : public kernel::Notifications
    {
    public:
        kernel::InterruptResult blockTip(SynchronizationState, CBlockIndex&) override
        {
            std::cout << "Block tip changed" << std::endl;
            return {};
        }
        void headerTip(SynchronizationState, int64_t height, int64_t timestamp, bool presync) override
        {
            std::cout << "Header tip changed: " << height << ", " << timestamp << ", " << presync << std::endl;
        }
        void progress(const bilingual_str& title, int progress_percent, bool resume_possible) override
        {
            std::cout << "Progress: " << title.original << ", " << progress_percent << ", " << resume_possible << std::endl;
        }
        void warningSet(kernel::Warning id, const bilingual_str& message) override
        {
            std::cout << "Warning " << static_cast<int>(id) << " set: " << message.original << std::endl;
        }
        void warningUnset(kernel::Warning id) override
        {
            std::cout << "Warning " << static_cast<int>(id) << " unset" << std::endl;
        }
        void flushError(const bilingual_str& message) override
        {
            std::cerr << "Error flushing block data to disk: " << message.original << std::endl;
        }
        void fatalError(const bilingual_str& message) override
        {
            std::cerr << "Error: " << message.original << std::endl;
        }
    };
    auto notifications = std::make_unique<KernelNotifications>();


    // SETUP: Chainstate
    auto chainparams = CChainParams::Main();
    const ChainstateManager::Options chainman_opts{
        .chainparams = *chainparams,
        .datadir = abs_datadir,
        .notifications = *notifications,
        .signals = &validation_signals,
    };
    const node::BlockManager::Options blockman_opts{
        .chainparams = chainman_opts.chainparams,
        .blocks_dir = abs_datadir / "blocks",
        .notifications = chainman_opts.notifications,
    };
    util::SignalInterrupt interrupt;
    ChainstateManager chainman{interrupt, chainman_opts, blockman_opts};

    node::CacheSizes cache_sizes;
    cache_sizes.block_tree_db = 2 << 20;
    cache_sizes.coins_db = 2 << 22;
    cache_sizes.coins = (450 << 20) - (2 << 20) - (2 << 22);
    node::ChainstateLoadOptions options;
    auto [status, error] = node::LoadChainstate(chainman, cache_sizes, options);
    if (status != node::ChainstateLoadStatus::SUCCESS) {
        std::cerr << "Failed to load Chain state from your datadir." << std::endl;
        goto epilogue;
    } else {
        std::tie(status, error) = node::VerifyLoadedChainstate(chainman, options);
        if (status != node::ChainstateLoadStatus::SUCCESS) {
            std::cerr << "Failed to verify loaded Chain state from your datadir." << std::endl;
            goto epilogue;
        }
    }

    for (Chainstate* chainstate : WITH_LOCK(::cs_main, return chainman.GetAll())) {
        BlockValidationState state;
        if (!chainstate->ActivateBestChain(state, nullptr)) {
            std::cerr << "Failed to connect best block (" << state.ToString() << ")" << std::endl;
            goto epilogue;
        }
    }

    	  std::cout << "BEFORE MAIN" << std::endl;

    {
        // Main program logic starts here
      // copied from src/node/transaction.cpp

        int block_height = FIRST_COINJOIN_HEIGHT;
        CBlockIndex* current_block;

        // Keeping track of data:
        WhirlpoolTransactions whirlpool_txs{abs_datadir};

	    	  std::cout << "AFTER WHIRLPOOL" << std::endl;

        {
            LOCK(chainman.GetMutex());
            current_block = chainman.ActiveChain()[block_height];
        }
	std::cout << "AFTER VIEW" << std::endl;
	std::cout << "BEFORE LOOP" << std::endl;

        while (current_block) {
	  std::cout << "TEST" << std::endl;

            CBlock block;
            CBlockUndo undo;
            bool read_success = false;
            read_success = chainman.m_blockman.ReadBlockFromDisk(block, *current_block);
            read_success &= chainman.m_blockman.UndoReadFromDisk(undo, *current_block);
            assert(read_success);
            CFeeRate fee_rate = GetMedianFeeRateFromBlock(block, undo);
            // the get feerate function

            for (const CTransactionRef& tx : block.vtx) {

                whirlpool_txs.Update(tx, current_block->nHeight, fee_rate);
            }

            std::cout << "Block height: " << block_height << "\n";

            // only look at 10 blocks after the first coinjoin block height for now
            if (block_height >= FIRST_COINJOIN_HEIGHT+10) {
                break;
            }

            {
                LOCK(chainman.GetMutex());
                current_block = chainman.ActiveChain().Next(current_block);
                block_height += 1;
            }
        }

        std::cout << "# of tx0s: " << whirlpool_txs.GetNumTx0s() << "\n";
    }

epilogue:
    // Without this precise shutdown sequence, there will be a lot of nullptr
    // dereferencing and UB.
    if (chainman.m_thread_load.joinable()) chainman.m_thread_load.join();

    validation_signals.FlushBackgroundCallbacks();
    {
        LOCK(cs_main);
        for (Chainstate* chainstate : chainman.GetAll()) {
            if (chainstate->CanFlushToDisk()) {
                chainstate->ForceFlushStateToDisk();
                chainstate->ResetCoinsViews();
            }
        }
    }
}
