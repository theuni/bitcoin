// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_DISCONNECTED_TRANSACTIONS_H
#define BITCOIN_KERNEL_DISCONNECTED_TRANSACTIONS_H

#include <core_memusage.h>
#include <memusage.h>
#include <primitives/transaction.h>
#include <util/hasher.h>

#include <list>
#include <unordered_map>

/**
 * DisconnectedBlockTransactions

 * During the reorg, it's desirable to re-add previously confirmed transactions
 * to the mempool, so that anything not re-confirmed in the new chain is
 * available to be mined. However, it's more efficient to wait until the reorg
 * is complete and process all still-unconfirmed transactions at that time,
 * since we expect most confirmed transactions to (typically) still be
 * confirmed in the new chain, and re-accepting to the memory pool is expensive
 * (and therefore better to not do in the middle of reorg-processing).
 * Instead, store the disconnected transactions (in order!) as we go, remove any
 * that are included in blocks in the new chain, and then process the remaining
 * still-unconfirmed transactions at the end.
 */
struct DisconnectedBlockTransactions {
    uint64_t cachedInnerUsage = 0;
    std::list<CTransactionRef> queuedTx;
    using Queue = decltype(queuedTx);
    std::unordered_map<uint256, Queue::iterator, SaltedTxidHasher> iters_by_txid;

    // It's almost certainly a logic bug if we don't clear out queuedTx before
    // destruction, as we add to it while disconnecting blocks, and then we
    // need to re-process remaining transactions to ensure mempool consistency.
    // For now, assert() that we've emptied out this object on destruction.
    // This assert() can always be removed if the reorg-processing code were
    // to be refactored such that this assumption is no longer true (for
    // instance if there was some other way we cleaned up the mempool after a
    // reorg, besides draining this object).
    ~DisconnectedBlockTransactions() { assert(queuedTx.empty()); }

    size_t DynamicMemoryUsage() const {
        // std::list has 3 pointers per entry
        return cachedInnerUsage + memusage::DynamicUsage(iters_by_txid) + memusage::MallocUsage(3 * sizeof(void*)) * queuedTx.size();
    }

    void addTransaction(const CTransactionRef& tx)
    {
        // Add new transactions to the end.
        auto it = queuedTx.insert(queuedTx.end(), tx);
        iters_by_txid.emplace(tx->GetHash(), it);
        cachedInnerUsage += RecursiveDynamicUsage(tx);
    }

    // Remove entries that are in this block.
    void removeForBlock(const std::vector<CTransactionRef>& vtx)
    {
        // Short-circuit in the common case of a block being added to the tip
        if (queuedTx.empty()) {
            return;
        }
        for (const auto& tx : vtx) {
            auto iter = iters_by_txid.find(tx->GetHash());
            if (iter != iters_by_txid.end()) {
                auto list_iter = iter->second;
                iters_by_txid.erase(iter);
                cachedInnerUsage -= RecursiveDynamicUsage(*list_iter);
                queuedTx.erase(list_iter);
            }
        }
    }

    // Remove the first entry and update memory usage.
    void remove_first()
    {
        cachedInnerUsage -= RecursiveDynamicUsage(queuedTx.front());
        iters_by_txid.erase(queuedTx.front()->GetHash());
        queuedTx.pop_front();
    }

    void clear()
    {
        cachedInnerUsage = 0;
        iters_by_txid.clear();
        queuedTx.clear();
    }
};

#endif // BITCOIN_KERNEL_DISCONNECTED_TRANSACTIONS_H
