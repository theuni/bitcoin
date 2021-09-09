// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_BITCOINKERNEL_H
#define BITCOIN_KERNEL_BITCOINKERNEL_H

#include <sync.h> // For RecursiveMutex

#include <optional>
#include <functional>

class CChainState;
class KernelChainstateManager;
class KernelCChainParams;
class CTxMemPool;
struct NodeContext;
class CScheduler;

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


class ChainContextStepZero {
public:
    virtual const KernelCChainParams& GetChainParams() = 0;
    virtual ~ChainContextStepZero() = default;
};

std::unique_ptr<ChainContextStepZero> StepZero(const std::string& network);

class ChainContextStepOne : public ChainContextStepZero {
public:
    virtual CScheduler& GetScheduler() = 0;
    const KernelCChainParams& GetChainParams() override {
        return GetPrevStep()->GetChainParams();
    }
private:
    virtual ChainContextStepZero* GetPrevStep() = 0;
};

std::unique_ptr<ChainContextStepOne> StepOne(std::unique_ptr<ChainContextStepZero> last_step, int num_script_check_threads);

void InitCaches();
void StartScriptThreads(int total_script_threads);
std::unique_ptr<CScheduler> StartScheduler();
void StartMainSignals(CScheduler& scheduler);
void InitGlobals();
std::unique_ptr<KernelChainstateManager> MakeChainstateManager();

std::optional<ChainstateActivationError> ActivateChainstateSequence(bool fReset,
                                                                    KernelChainstateManager& chainman,
                                                                    CTxMemPool* mempool,
                                                                    bool fPruneMode,
                                                                    const KernelCChainParams& chainparams,
                                                                    bool fReindexChainState,
                                                                    int64_t nBlockTreeDBCache,
                                                                    int64_t nCoinDBCache,
                                                                    int64_t nCoinCacheUsage,
                                                                    unsigned int check_blocks,
                                                                    unsigned int check_level,
                                                                    bool block_tree_db_in_memory,
                                                                    std::function<int64_t()> get_time,
                                                                    std::function<bool()> shutdown_requested = [](){ return false; },
                                                                    std::optional<std::function<void()>> coins_error_cb = std::nullopt,
                                                                    std::function<void()> verifying_blocks_cb = [](){});

#endif // BITCOIN_KERNEL_BITCOINKERNEL_H
