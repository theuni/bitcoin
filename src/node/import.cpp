// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/import.h>

#include <flatfile.h>
#include <node/blockstorage.h>
#include <util/args.h>
#include <util/syscall_sandbox.h>
#include <util/system.h>
#include <validation.h>

namespace node {

std::atomic_bool fImporting(false);

struct CImportingNow {
    CImportingNow()
    {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow()
    {
        assert(fImporting == true);
        fImporting = false;
    }
};

MaybeEarlyExit<> BlockImport(ChainstateManager& chainman, std::vector<fs::path> vImportFiles, const ArgsManager& args)
{
    SetSyscallSandboxPolicy(SyscallSandboxPolicy::INITIALIZATION_LOAD_BLOCKS);
    ScheduleBatchPriority();

    {
        CImportingNow imp;

        // -reindex
        if (fReindex) {
            int nFile = 0;
            while (true) {
                FlatFilePos pos(nFile, 0);
                if (!fs::exists(GetBlockPosFilename(pos))) {
                    break; // No block files left to reindex
                }
                FILE* file = OpenBlockFile(pos, true);
                if (!file) {
                    break; // This error is logged in OpenBlockFile
                }
                LogPrintf("Reindexing block file blk%05u.dat...\n", (unsigned int)nFile);
                chainman.ActiveChainstate().LoadExternalBlockFile(file, &pos);
                if (chainman.Interrupted()) {
                    LogPrintf("Shutdown requested. Exit %s\n", __func__);
                    return UserInterrupted::UNKNOWN;
                }
                nFile++;
            }
            WITH_LOCK(::cs_main, chainman.m_blockman.m_block_tree_db->WriteReindexing(false));
            fReindex = false;
            LogPrintf("Reindexing finished\n");
            // To avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
            chainman.ActiveChainstate().LoadGenesisBlock();
        }

        // -loadblock=
        for (const fs::path& path : vImportFiles) {
            FILE* file = fsbridge::fopen(path, "rb");
            if (file) {
                LogPrintf("Importing blocks file %s...\n", fs::PathToString(path));
                chainman.ActiveChainstate().LoadExternalBlockFile(file);
                if (chainman.Interrupted()) {
                    LogPrintf("Shutdown requested. Exit %s\n", __func__);
                    return UserInterrupted::UNKNOWN;
                }
            } else {
                LogPrintf("Warning: Could not open blocks file %s\n", fs::PathToString(path));
            }
        }

        // scan for better chains in the block chain database, that are not yet connected in the active best chain

        // We can't hold cs_main during ActivateBestChain even though we're accessing
        // the chainman unique_ptrs since ABC requires us not to be holding cs_main, so retrieve
        // the relevant pointers before the ABC call.
        for (CChainState* chainstate : WITH_LOCK(::cs_main, return chainman.GetAll())) {
            BlockValidationState state;
            auto abc_ret = chainstate->ActivateBestChain(state, nullptr);
            if (abc_ret.ShouldEarlyExit()) return BubbleUp(std::move(abc_ret));
            if (!*abc_ret) {
                LogPrintf("Failed to connect best block (%s)\n", state.ToString());
                return FatalError::UNKNOWN;
            }
        }

        if (args.GetBoolArg("-stopafterblockimport", DEFAULT_STOPAFTERBLOCKIMPORT)) {
            LogPrintf("Stopping after block import\n");
            return UserInterrupted::BLOCK_IMPORT_COMPLETE;
        }
    } // End scope of CImportingNow
    chainman.ActiveChainstate().LoadMempool(args.GetBoolArg("-persistmempool", DEFAULT_PERSIST_MEMPOOL));
    return {};
}
} // namespace node
