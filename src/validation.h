// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <kernel/validation.h>

#include <amount.h>
#include <attributes.h>
#include <coins.h>
#include <consensus/validation.h>
#include <crypto/common.h> // for ReadLE64
#include <fs.h>
#include <node/utxo_snapshot.h>
#include <policy/feerate.h>
#include <policy/packages.h>
#include <protocol.h> // For CMessageHeader::MessageStartChars
#include <script/script_error.h>
#include <sync.h>
#include <txmempool.h> // For CTxMemPool::cs
#include <txdb.h>
#include <serialize.h>
#include <util/check.h>
#include <util/hasher.h>
#include <util/translation.h>

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class BlockValidationState;
class CBlockIndex;
class CBlockUndo;
class CChainParams;
class CInv;
class CConnman;
class CScriptCheck;
class CTxMemPool;
struct ChainTxData;

struct PrecomputedTransactionData;
struct LockPoints;
struct AssumeutxoData;

class CChainState : public KernelCChainState
{
public:
    explicit CChainState(CTxMemPool* mempool,
                         BlockManager& blockman,
                         std::optional<uint256> from_snapshot_blockhash = std::nullopt);

    using KernelCChainState::GetCoinsCacheSizeState;
    CoinsCacheSizeState GetCoinsCacheSizeState(size_t max_coins_cache_size_bytes,
                                               size_t max_mempool_size_bytes) override EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    bool DisconnectTip(BlockValidationState& state, DisconnectedBlockTransactions* disconnectpool) override EXCLUSIVE_LOCKS_REQUIRED(cs_main, MempoolMutex());

    void LoadMempool(const ArgsManager& args) override;
private:
    //! Optional mempool that is kept in sync with the chain.
    //! Only the active chainstate has a mempool.
    CTxMemPool* m_mempool{nullptr};
protected:

    const CChainParams& GetParams() const override {
        return m_params;
    }

    const CChainParams& m_params;

    CTxMemPool* Mempool() const override {
        return m_mempool;
    }

    RecursiveMutex* MempoolMutex() const LOCK_RETURNED(m_mempool->GetMutex()) {
        return KernelCChainState::MempoolMutex();
    }

    bool ActivateBestChainStep(BlockValidationState& state, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, ConnectTrace& connectTrace) override EXCLUSIVE_LOCKS_REQUIRED(cs_main, MempoolMutex());

    void MaybeUpdateMempoolForReorg(DisconnectedBlockTransactions& disconnectpool,
                                    bool fAddToMempool) override EXCLUSIVE_LOCKS_REQUIRED(cs_main, MempoolMutex());

    bool ConnectTip(BlockValidationState& state, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions& disconnectpool) override EXCLUSIVE_LOCKS_REQUIRED(cs_main, MempoolMutex());

    /** Check warning conditions and do some notifications on new chain tip set. */
    void UpdateTip(const CBlockIndex* pindexNew) override EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
};

class ChainstateManager : public KernelChainstateManager {
private:
    //! The chainstate used under normal operation (i.e. "regular" IBD) or, if
    //! a snapshot is in use, for background validation.
    //!
    //! Its contents (including on-disk data) will be deleted *upon shutdown*
    //! after background validation of the snapshot has completed. We do not
    //! free the chainstate contents immediately after it finishes validation
    //! to cautiously avoid a case where some other part of the system is still
    //! using this pointer (e.g. net_processing).
    //!
    //! Once this pointer is set to a corresponding chainstate, it will not
    //! be reset until init.cpp:Shutdown().
    //!
    //! This is especially important when, e.g., calling ActivateBestChain()
    //! on all chainstates because we are not able to hold ::cs_main going into
    //! that call.
    std::unique_ptr<CChainState> m_ibd_chainstate GUARDED_BY(::cs_main);

    //! A chainstate initialized on the basis of a UTXO snapshot. If this is
    //! non-null, it is always our active chainstate.
    //!
    //! Once this pointer is set to a corresponding chainstate, it will not
    //! be reset until init.cpp:Shutdown().
    //!
    //! This is especially important when, e.g., calling ActivateBestChain()
    //! on all chainstates because we are not able to hold ::cs_main going into
    //! that call.
    std::unique_ptr<CChainState> m_snapshot_chainstate GUARDED_BY(::cs_main);

    //! Points to either the ibd or snapshot chainstate; indicates our
    //! most-work chain.
    //!
    //! Once this pointer is set to a corresponding chainstate, it will not
    //! be reset until init.cpp:Shutdown().
    //!
    //! This is especially important when, e.g., calling ActivateBestChain()
    //! on all chainstates because we are not able to hold ::cs_main going into
    //! that call.
    CChainState* m_active_chainstate GUARDED_BY(::cs_main) {nullptr};
public:
    CChainState& InitializeChainstate(CTxMemPool* mempool,
                                      const std::optional<uint256>& snapshot_blockhash = std::nullopt)
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main) override;

    std::vector<CChainState*> GetAll() override;

    void Reset() override;

    CChainState& ActiveChainstate() const override;
    CChainState& ValidatedChainstate() const override;

    [[nodiscard]] bool ActivateSnapshot(CAutoFile& coins_file, const SnapshotMetadata& metadata, bool in_memory) override;

    ~ChainstateManager();
protected:
    CChainState* GetIBDChainState() const override EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return m_ibd_chainstate.get();
    }
    CChainState* GetSnapshotChainState() const override EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return m_snapshot_chainstate.get();
    }
    CChainState* GetActiveChainState() const override EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return m_active_chainstate;
    }

};

/** Default for -minrelaytxfee, minimum relay fee for transactions */
static const unsigned int DEFAULT_MIN_RELAY_TX_FEE = 1000;
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 25;
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 101;
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 25;
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 101;
/** Default for -mempoolexpiry, expiration time for mempool transactions in hours */
static const unsigned int DEFAULT_MEMPOOL_EXPIRY = 336;
/** Maximum number of dedicated script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 15;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
static const int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_TXINDEX = false;
static constexpr bool DEFAULT_COINSTATSINDEX{false};
static const char* const DEFAULT_BLOCKFILTERINDEX = "0";
/** Default for -persistmempool */
static const bool DEFAULT_PERSIST_MEMPOOL = true;
/** Default for -stopatheight */
static const int DEFAULT_STOPATHEIGHT = 0;
/** Block files containing a block-height within MIN_BLOCKS_TO_KEEP of ::ChainActive().Tip() will not be pruned. */
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;
static const signed int DEFAULT_CHECKBLOCKS = 6;
static const unsigned int DEFAULT_CHECKLEVEL = 3;
// Require that user allocate at least 550 MiB for block & undo files (blk???.dat and rev???.dat)
// At 1MB per block, 288 blocks = 288MB.
// Add 15% for Undo data = 331MB
// Add 20% for Orphan block rate = 397MB
// We want the low water mark after pruning to be at least 397 MB and since we prune in
// full block file chunks, we need the high water mark which triggers the prune to be
// one 128MB block file + added 15% undo data = 147MB greater for a total of 545MB
// Setting the target to >= 550 MiB will make it likely we can respect the target.
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

/** Current sync state passed to tip changed callbacks. */
enum class SynchronizationState {
    INIT_REINDEX,
    INIT_DOWNLOAD,
    POST_INIT
};

// extern RecursiveMutex cs_main;
// typedef std::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;
extern Mutex g_best_block_mutex;
extern std::condition_variable g_best_block_cv;
extern uint256 g_best_block;
/** Whether there are dedicated script-checking threads running.
 * False indicates all script checking is done on the main threadMessageHandler thread.
 */
extern bool g_parallel_script_checks;
extern bool fRequireStandard;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
/** A fee rate smaller than this is considered zero fee (for relaying, mining and transaction creation) */
extern CFeeRate minRelayTxFee;
/** If the tip is older than this (in seconds), the node is considered to be in initial block download. */
extern int64_t nMaxTipAge;

/** Block hash whose ancestors we will assume to have valid scripts without checking them. */
extern uint256 hashAssumeValid;

/** Minimum work we will assume exists on some valid chain. */
extern arith_uint256 nMinimumChainWork;

/** Best header we've seen so far (used for getheaders queries' starting points). */
extern CBlockIndex *pindexBestHeader;

/** Documentation for argument 'checklevel'. */
extern const std::vector<std::string> CHECKLEVEL_DOC;

/** Unload database information */
void UnloadBlockIndex(CTxMemPool* mempool, KernelChainstateManager& chainman);
/** Run instances of script checking worker threads */
void StartScriptCheckWorkerThreads(int threads_num);
/** Stop all of the script checking worker threads */
void StopScriptCheckWorkerThreads();

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

bool AbortNode(BlockValidationState& state, const std::string& strMessage, const bilingual_str& userMessage = bilingual_str{});

/** Guess verification progress (as a fraction between 0.0=genesis and 1.0=current tip). */
double GuessVerificationProgress(const ChainTxData& data, const CBlockIndex* pindex);

/** Prune block files up to a given height */
void PruneBlockFilesManual(CChainState& active_chainstate, int nManualPruneHeight);

/**
* Validation result for a single transaction mempool acceptance.
*/
struct MempoolAcceptResult {
    /** Used to indicate the results of mempool validation. */
    enum class ResultType {
        VALID, //!> Fully validated, valid.
        INVALID, //!> Invalid.
    };
    const ResultType m_result_type;
    const TxValidationState m_state;

    // The following fields are only present when m_result_type = ResultType::VALID
    /** Mempool transactions replaced by the tx per BIP 125 rules. */
    const std::optional<std::list<CTransactionRef>> m_replaced_transactions;
    /** Raw base fees in satoshis. */
    const std::optional<CAmount> m_base_fees;
    static MempoolAcceptResult Failure(TxValidationState state) {
        return MempoolAcceptResult(state);
    }

    static MempoolAcceptResult Success(std::list<CTransactionRef>&& replaced_txns, CAmount fees) {
        return MempoolAcceptResult(std::move(replaced_txns), fees);
    }

// Private constructors. Use static methods MempoolAcceptResult::Success, etc. to construct.
private:
    /** Constructor for failure case */
    explicit MempoolAcceptResult(TxValidationState state)
        : m_result_type(ResultType::INVALID), m_state(state) {
            Assume(!state.IsValid()); // Can be invalid or error
        }

    /** Constructor for success case */
    explicit MempoolAcceptResult(std::list<CTransactionRef>&& replaced_txns, CAmount fees)
        : m_result_type(ResultType::VALID),
        m_replaced_transactions(std::move(replaced_txns)), m_base_fees(fees) {}
};

/**
* Validation result for package mempool acceptance.
*/
struct PackageMempoolAcceptResult
{
    const PackageValidationState m_state;
    /**
    * Map from wtxid to finished MempoolAcceptResults. The client is responsible
    * for keeping track of the transaction objects themselves. If a result is not
    * present, it means validation was unfinished for that transaction. If there
    * was a package-wide error (see result in m_state), m_tx_results will be empty.
    */
    std::map<const uint256, const MempoolAcceptResult> m_tx_results;

    explicit PackageMempoolAcceptResult(PackageValidationState state,
                                        std::map<const uint256, const MempoolAcceptResult>&& results)
        : m_state{state}, m_tx_results(std::move(results)) {}

    /** Constructor to create a PackageMempoolAcceptResult from a single MempoolAcceptResult */
    explicit PackageMempoolAcceptResult(const uint256& wtxid, const MempoolAcceptResult& result)
        : m_tx_results{ {wtxid, result} } {}
};

/**
 * (Try to) add a transaction to the memory pool.
 * @param[in]  bypass_limits   When true, don't enforce mempool fee limits.
 * @param[in]  test_accept     When true, run validation checks but don't submit to mempool.
 */
MempoolAcceptResult AcceptToMemoryPool(CChainState& active_chainstate, CTxMemPool& pool, const CTransactionRef& tx,
                                       bool bypass_limits, bool test_accept=false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
* Atomically test acceptance of a package. If the package only contains one tx, package rules still
* apply. Package validation does not allow BIP125 replacements, so the transaction(s) cannot spend
* the same inputs as any transaction in the mempool.
* @param[in]    txns                Group of transactions which may be independent or contain
*                                   parent-child dependencies. The transactions must not conflict
*                                   with each other, i.e., must not spend the same inputs. If any
*                                   dependencies exist, parents must appear anywhere in the list
*                                   before their children.
* @returns a PackageMempoolAcceptResult which includes a MempoolAcceptResult for each transaction.
* If a transaction fails, validation will exit early and some results may be missing.
*/
PackageMempoolAcceptResult ProcessNewPackage(CChainState& active_chainstate, CTxMemPool& pool,
                                                   const Package& txns, bool test_accept)
                                                   EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Apply the effects of this transaction on the UTXO set represented by view */
void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight);

/** Transaction validation functions */

/**
 * Check if transaction will be final in the next block to be created.
 *
 * Calls IsFinalTx() with current block height and appropriate block time.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckFinalTx(const CBlockIndex* active_chain_tip, const CTransaction &tx, int flags = -1) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Test whether the LockPoints height and time are still valid on the current chain
 */
bool TestLockPointValidity(CChain& active_chain, const LockPoints* lp) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Check if transaction will be BIP68 final in the next block to be created on top of tip.
 * @param[in]   tip             Chain tip to check tx sequence locks against. For example,
 *                              the tip of the current active chain.
 * @param[in]   coins_view      Any CCoinsView that provides access to the relevant coins for
 *                              checking sequence locks. For example, it can be a CCoinsViewCache
 *                              that isn't connected to anything but contains all the relevant
 *                              coins, or a CCoinsViewMemPool that is connected to the
 *                              mempool and chainstate UTXO set. In the latter case, the caller is
 *                              responsible for holding the appropriate locks to ensure that
 *                              calls to GetCoin() return correct coins.
 * Simulates calling SequenceLocks() with data from the tip passed in.
 * Optionally stores in LockPoints the resulting height and time calculated and the hash
 * of the block needed for calculation or skips the calculation and uses the LockPoints
 * passed in for evaluation.
 * The LockPoints should not be considered valid if CheckSequenceLocks returns false.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckSequenceLocks(CBlockIndex* tip,
                        const CCoinsView& coins_view,
                        const CTransaction& tx,
                        int flags,
                        LockPoints* lp = nullptr,
                        bool useExistingLockPoints = false);

/**
 * Closure representing one script verification
 * Note that this stores references to the spending transaction
 */
class CScriptCheck
{
private:
    CTxOut m_tx_out;
    const CTransaction *ptxTo;
    unsigned int nIn;
    unsigned int nFlags;
    bool cacheStore;
    ScriptError error;
    PrecomputedTransactionData *txdata;

public:
    CScriptCheck(): ptxTo(nullptr), nIn(0), nFlags(0), cacheStore(false), error(SCRIPT_ERR_UNKNOWN_ERROR) {}
    CScriptCheck(const CTxOut& outIn, const CTransaction& txToIn, unsigned int nInIn, unsigned int nFlagsIn, bool cacheIn, PrecomputedTransactionData* txdataIn) :
        m_tx_out(outIn), ptxTo(&txToIn), nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn), error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) { }

    bool operator()();

    void swap(CScriptCheck &check) {
        std::swap(ptxTo, check.ptxTo);
        std::swap(m_tx_out, check.m_tx_out);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};

/** Initializes the script-execution cache */
void InitScriptExecutionCache();

/** Functions for validating blocks and updating the block tree */

/** Context-independent validity checks */
bool CheckBlock(const CBlock& block, BlockValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW = true, bool fCheckMerkleRoot = true);

/** Check a block is completely valid from start to finish (only works on top of our current best block) */
bool TestBlockValidity(BlockValidationState& state,
                       const CChainParams& chainparams,
                       CChainState& chainstate,
                       const CBlock& block,
                       CBlockIndex* pindexPrev,
                       bool fCheckPOW = true,
                       bool fCheckMerkleRoot = true) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Produce the necessary coinbase commitment for a block (modifies the hash, don't call for mined blocks). */
std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/** RAII wrapper for VerifyDB: Verify consistency of the block and coin databases */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(
        CChainState& chainstate,
        const Consensus::Params& consensus_params,
        CCoinsView& coinsview,
        int nCheckLevel,
        int nCheckDepth) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
};

using FopenFn = std::function<FILE*(const fs::path&, const char*)>;

/** Dump the mempool to disk. */
bool DumpMempool(const CTxMemPool& pool, FopenFn mockable_fopen_function = fsbridge::fopen, bool skip_file_commit = false);

/** Load the mempool from disk. */
bool LoadMempool(CTxMemPool& pool, CChainState& active_chainstate, FopenFn mockable_fopen_function = fsbridge::fopen);

/**
 * Return the expected assumeutxo value for a given height, if one exists.
 *
 * @param[in] height Get the assumeutxo value for this height.
 *
 * @returns empty if no assumeutxo configuration exists for the given height.
 */
const AssumeutxoData* ExpectedAssumeutxo(const int height, const CChainParams& params);

#endif // BITCOIN_VALIDATION_H
