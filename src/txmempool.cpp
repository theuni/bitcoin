// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txmempool.h>

#include <chain.h>
#include <coins.h>
#include <common/system.h>
#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <logging.h>
#include <mempool_set_definitions.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/settings.h>
#include <reverse_iterator.h>
#include <util/check.h>
#include <util/moneystr.h>
#include <util/overflow.h>
#include <util/result.h>
#include <util/time.h>
#include <util/trace.h>
#include <util/translation.h>
#include <validationinterface.h>

#include <cmath>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>

using MempoolMultiIndex::cacheMap;
using MempoolMultiIndex::indexed_transaction_set;
using MempoolMultiIndex::MapTxImpl;
using MempoolMultiIndex::raw_setEntries;
using MempoolMultiIndex::raw_txiter;
using MempoolMultiIndex::setEntries;
using MempoolMultiIndex::txiter;

bool TestLockPointValidity(CChain& active_chain, const LockPoints& lp)
{
    AssertLockHeld(cs_main);
    // If there are relative lock times then the maxInputBlock will be set
    // If there are no relative lock times, the LockPoints don't depend on the chain
    if (lp.maxInputBlock) {
        // Check whether active_chain is an extension of the block at which the LockPoints
        // calculation was valid.  If not LockPoints are no longer valid
        if (!active_chain.Contains(lp.maxInputBlock)) {
            return false;
        }
    }

    // LockPoints still valid
    return true;
}

/** UpdateForDescendants is used by UpdateTransactionsFromBlock to update
 *  the descendants for a single transaction that has been added to the
 *  mempool but may have child transactions in the mempool, eg during a
 *  chain reorg.
 *
 * @pre CTxMemPoolEntry::m_children is correct for the given tx and all
 *      descendants.
 * @pre cachedDescendants is an accurate cache where each entry has all
 *      descendants of the corresponding key, including those that should
 *      be removed for violation of ancestor limits.
 * @post if updateIt has any non-excluded descendants, cachedDescendants has
 *       a new cache line for updateIt.
 * @post descendants_to_remove has a new entry for any descendant which exceeded
 *       ancestor limits relative to updateIt.
 *
 * @param[in] updateIt the entry to update for its descendants
 * @param[in,out] cachedDescendants a cache where each line corresponds to all
 *     descendants. It will be updated with the descendants of the transaction
 *     being updated, so that future invocations don't need to walk the same
 *     transaction again, if encountered in another transaction chain.
 * @param[in] setExclude the set of descendant transactions in the mempool
 *     that must not be accounted for (because any descendants in setExclude
 *     were added to the mempool after the transaction being updated and hence
 *     their state is already reflected in the parent state).
 * @param[out] descendants_to_remove Populated with the txids of entries that
 *     exceed ancestor limits. It's the responsibility of the caller to
 *     removeRecursive them.
 */
static void UpdateForDescendants(txiter& updateIt,
                                 cacheMap& cachedDescendants,
                                 const std::set<uint256>& setExclude,
                                 std::set<uint256>& descendants_to_remove,
                                 std::unique_ptr<MapTxImpl>& mapTx,
                                 const CTxMemPool::Limits& m_limits) EXCLUSIVE_LOCKS_REQUIRED(CTxMemPool::cs)
{
    CTxMemPoolEntry::Children stageEntries, descendants;
    stageEntries = updateIt.impl->GetMemPoolChildrenConst();

    while (!stageEntries.empty()) {
        const CTxMemPoolEntry& descendant = *stageEntries.begin();
        descendants.insert(descendant);
        stageEntries.erase(descendant);
        const CTxMemPoolEntry::Children& children = descendant.GetMemPoolChildrenConst();
        for (const CTxMemPoolEntry& childEntry : children) {
            cacheMap::iterator cacheIt = cachedDescendants.find(mapTx->impl.iterator_to(childEntry));
            if (cacheIt != cachedDescendants.end()) {
                // We've already calculated this one, just add the entries for this set
                // but don't traverse again.
                for (raw_txiter cacheEntry : cacheIt->second) {
                    descendants.insert(*cacheEntry);
                }
            } else if (!descendants.count(childEntry)) {
                // Schedule for later processing
                stageEntries.insert(childEntry);
            }
        }
    }
    // descendants now contains all in-mempool descendants of updateIt.
    // Update and add to cached descendant map
    int32_t modifySize = 0;
    CAmount modifyFee = 0;
    int64_t modifyCount = 0;
    for (const CTxMemPoolEntry& descendant : descendants) {
        if (!setExclude.count(descendant.GetTx().GetHash())) {
            modifySize += descendant.GetTxSize();
            modifyFee += descendant.GetModifiedFee();
            modifyCount++;
            cachedDescendants[updateIt].insert(mapTx->impl.iterator_to(descendant));
            // Update ancestor state for each descendant
            mapTx->impl.modify(mapTx->impl.iterator_to(descendant), [=](CTxMemPoolEntry& e) {
              e.UpdateAncestorState(updateIt.impl->GetTxSize(), updateIt.impl->GetModifiedFee(), 1, updateIt.impl->GetSigOpCost());
            });
            // Don't directly remove the transaction here -- doing so would
            // invalidate iterators in cachedDescendants. Mark it for removal
            // by inserting into descendants_to_remove.
            if (descendant.GetCountWithAncestors() > uint64_t(m_limits.ancestor_count) || descendant.GetSizeWithAncestors() > m_limits.ancestor_size_vbytes) {
                descendants_to_remove.insert(descendant.GetTx().GetHash());
            }
        }
    }
    mapTx->impl.modify(updateIt, [=](CTxMemPoolEntry& e) { e.UpdateDescendantState(modifySize, modifyFee, modifyCount); });
}

void CTxMemPool::UpdateTransactionsFromBlock(const std::vector<uint256>& vHashesToUpdate)
{
    AssertLockHeld(cs);
    // For each entry in vHashesToUpdate, store the set of in-mempool, but not
    // in-vHashesToUpdate transactions, so that we don't have to recalculate
    // descendants when we come across a previously seen entry.
    cacheMap mapMemPoolDescendantsToUpdate;

    // Use a set for lookups into vHashesToUpdate (these entries are already
    // accounted for in the state of their ancestors)
    std::set<uint256> setAlreadyIncluded(vHashesToUpdate.begin(), vHashesToUpdate.end());

    std::set<uint256> descendants_to_remove;

    // Iterate in reverse, so that whenever we are looking at a transaction
    // we are sure that all in-mempool descendants have already been processed.
    // This maximizes the benefit of the descendant cache and guarantees that
    // CTxMemPoolEntry::m_children will be updated, an assumption made in
    // UpdateForDescendants.
    for (const uint256 &hash : reverse_iterate(vHashesToUpdate)) {
        // calculate children from mapNextTx
        txiter it{mapTx->impl.find(hash)};
        if (it.impl == mapTx->impl.end()) {
            continue;
        }
        auto iter = mapNextTx.lower_bound(COutPoint(hash, 0));
        // First calculate the children, and update CTxMemPoolEntry::m_children to
        // include them, and update their CTxMemPoolEntry::m_parents to include this tx.
        // we cache the in-mempool children to avoid duplicate updates
        {
            WITH_FRESH_EPOCH(m_epoch);
            for (; iter != mapNextTx.end() && iter->first->hash == hash; ++iter) {
                const uint256 &childHash = iter->second->GetHash();
                txiter childIter = mapTx->impl.find(childHash);
                assert(childIter != mapTx->impl.end());
                // We can skip updating entries we've encountered before or that
                // are in the block (which are already accounted for).
                if (!visited(childIter) && !setAlreadyIncluded.count(childHash)) {
                    
                    UpdateChild(it, childIter, true);
                    UpdateParent(childIter, it, true);
                }
            }
        } // release epoch guard for UpdateForDescendants
        UpdateForDescendants(it, mapMemPoolDescendantsToUpdate, setAlreadyIncluded, descendants_to_remove, mapTx, m_limits);
    }

    for (const auto& txid : descendants_to_remove) {
        // This txid may have been removed already in a prior call to removeRecursive.
        // Therefore we ensure it is not yet removed already.
        if (const std::optional<std::unique_ptr<txiter>> txiter = GetIter(txid)) {
            removeRecursive(((**txiter).impl)->GetTx(), MemPoolRemovalReason::SIZELIMIT);
        }
    }
}

util::Result<std::unique_ptr<setEntries>> CTxMemPool::CalculateAncestorsAndCheckLimits(
    int64_t entry_size,
    size_t entry_count,
    CTxMemPoolEntry::Parents& staged_ancestors,
    const Limits& limits) const
{
    int64_t totalSizeWithAncestors = entry_size;
    raw_setEntries ancestors;

    while (!staged_ancestors.empty()) {
        const CTxMemPoolEntry& stage = staged_ancestors.begin()->get();
        raw_txiter stageit{mapTx->impl.iterator_to(stage)};

        ancestors.insert(stageit);
        staged_ancestors.erase(stage);
        totalSizeWithAncestors += stageit->GetTxSize();

        if (stageit->GetSizeWithDescendants() + entry_size > limits.descendant_size_vbytes) {
            return util::Error{Untranslated(strprintf("exceeds descendant size limit for tx %s [limit: %u]", stageit->GetTx().GetHash().ToString(), limits.descendant_size_vbytes))};
        } else if (stageit->GetCountWithDescendants() + entry_count > static_cast<uint64_t>(limits.descendant_count)) {
            return util::Error{Untranslated(strprintf("too many descendants for tx %s [limit: %u]", stageit->GetTx().GetHash().ToString(), limits.descendant_count))};
        } else if (totalSizeWithAncestors > limits.ancestor_size_vbytes) {
            return util::Error{Untranslated(strprintf("exceeds ancestor size limit [limit: %u]", limits.ancestor_size_vbytes))};
        }

        const CTxMemPoolEntry::Parents& parents = stageit->GetMemPoolParentsConst();
        for (const CTxMemPoolEntry& parent : parents) {
            raw_txiter parent_it = mapTx->impl.iterator_to(parent);

            // If this is a new ancestor, add it.
            if (ancestors.count(parent_it) == 0) {
                staged_ancestors.insert(parent);
            }
            if (staged_ancestors.size() + ancestors.size() + entry_count > static_cast<uint64_t>(limits.ancestor_count)) {
                return util::Error{Untranslated(strprintf("too many unconfirmed ancestors [limit: %u]", limits.ancestor_count))};
            }
        }
    }

    return std::make_unique<setEntries>(ancestors);
}

bool CTxMemPool::CheckPackageLimits(const Package& package,
                                    const Limits& limits,
                                    std::string &errString) const
{
    CTxMemPoolEntry::Parents staged_ancestors;
    int64_t total_size = 0;
    for (const auto& tx : package) {
        total_size += GetVirtualTransactionSize(*tx);
        for (const auto& input : tx->vin) {
            std::optional<std::unique_ptr<txiter>> piter = GetIter(input.prevout.hash);
            if (piter) {
                staged_ancestors.insert(***piter);
                if (staged_ancestors.size() + package.size() > static_cast<uint64_t>(limits.ancestor_count)) {
                    errString = strprintf("too many unconfirmed parents [limit: %u]", limits.ancestor_count);
                    return false;
                }
            }
        }
    }
    // When multiple transactions are passed in, the ancestors and descendants of all transactions
    // considered together must be within limits even if they are not interdependent. This may be
    // stricter than the limits for each individual transaction.
    const auto ancestors{CalculateAncestorsAndCheckLimits(total_size, package.size(),
                                                          staged_ancestors, limits)};
    // It's possible to overestimate the ancestor/descendant totals.
    if (!ancestors.has_value()) errString = "possibly " + util::ErrorString(ancestors).original;
    return ancestors.has_value();
}

util::Result<std::unique_ptr<setEntries>> CTxMemPool::CalculateMemPoolAncestors(
    const CTxMemPoolEntry &entry,
    const Limits& limits,
    bool fSearchForParents /* = true */) const
{
    CTxMemPoolEntry::Parents staged_ancestors;
    const CTransaction &tx = entry.GetTx();

    if (fSearchForParents) {
        // Get parents of this transaction that are in the mempool
        // GetMemPoolParents() is only valid for entries in the mempool, so we
        // iterate mapTx->impl.to find parents.
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            std::optional<std::unique_ptr<txiter>> piter = GetIter(tx.vin[i].prevout.hash);
            if (piter) {
                staged_ancestors.insert(***piter);
                if (staged_ancestors.size() + 1 > static_cast<uint64_t>(limits.ancestor_count)) {
                    return util::Error{Untranslated(strprintf("too many unconfirmed parents [limit: %u]", limits.ancestor_count))};
                }
            }
        }
    } else {
        // If we're not searching for parents, we require this to already be an
        // entry in the mempool and use the entry's cached parents.
        raw_txiter it = mapTx->impl.iterator_to(entry);
        staged_ancestors = it->GetMemPoolParentsConst();
    }

    return CalculateAncestorsAndCheckLimits(entry.GetTxSize(), /*entry_count=*/1, staged_ancestors,
                                            limits);
}

std::unique_ptr<setEntries> CTxMemPool::AssumeCalculateMemPoolAncestors(
    std::string_view calling_fn_name,
    const CTxMemPoolEntry &entry,
    const Limits& limits,
    bool fSearchForParents /* = true */) const
{
    auto result{CalculateMemPoolAncestors(entry, limits, fSearchForParents)};
    if (!Assume(result)) {
        LogPrintLevel(BCLog::MEMPOOL, BCLog::Level::Error, "%s: CalculateMemPoolAncestors failed unexpectedly, continuing with empty ancestor set (%s)\n",
                      calling_fn_name, util::ErrorString(result).original);
    }
    return std::move(result).value_or(std::make_unique<setEntries>(setEntries{}));
}

void CTxMemPool::UpdateAncestorsOf(bool add, txiter& it, setEntries &setAncestors)
{
    const CTxMemPoolEntry::Parents& parents = it.impl->GetMemPoolParentsConst();
    // add or remove this tx as a child of each parent
    for (const CTxMemPoolEntry& parent : parents) {
        txiter entry{mapTx->impl.iterator_to(parent)};
        UpdateChild(entry, it, add);
    }
    const int32_t updateCount = (add ? 1 : -1);
    const int32_t updateSize{updateCount * it.impl->GetTxSize()};
    const CAmount updateFee = updateCount * it.impl->GetModifiedFee();
    for (raw_txiter ancestorIt : setAncestors.impl) {
        mapTx->impl.modify(ancestorIt, [=](CTxMemPoolEntry& e) { e.UpdateDescendantState(updateSize, updateFee, updateCount); });
    }
}

void CTxMemPool::UpdateEntryForAncestors(txiter& it, const setEntries &setAncestors)
{
    int64_t updateCount = setAncestors.impl.size();
    int64_t updateSize = 0;
    CAmount updateFee = 0;
    int64_t updateSigOpsCost = 0;
    for (raw_txiter ancestorIt : setAncestors.impl) {
        updateSize += ancestorIt->GetTxSize();
        updateFee += ancestorIt->GetModifiedFee();
        updateSigOpsCost += ancestorIt->GetSigOpCost();
    }
    mapTx->impl.modify(it, [=](CTxMemPoolEntry& e){ e.UpdateAncestorState(updateSize, updateFee, updateCount, updateSigOpsCost); });
}

void CTxMemPool::UpdateChildrenForRemoval(txiter& it)
{
    const CTxMemPoolEntry::Children& children = it.impl->GetMemPoolChildrenConst();
    for (const CTxMemPoolEntry& updateIt : children) {
        txiter entry{mapTx->impl.iterator_to(updateIt)};
        UpdateParent(entry, it, false);
    }
}

void CTxMemPool::UpdateForRemoveFromMempool(const setEntries &entriesToRemove, bool updateDescendants)
{
    // For each entry, walk back all ancestors and decrement size associated with this
    // transaction
    if (updateDescendants) {
        // updateDescendants should be true whenever we're not recursively
        // removing a tx and all its descendants, eg when a transaction is
        // confirmed in a block.
        // Here we only update statistics and not data in CTxMemPool::Parents
        // and CTxMemPoolEntry::Children (which we need to preserve until we're
        // finished with all operations that need to traverse the mempool).
        for (raw_txiter removeIt : entriesToRemove.impl) {
            setEntries setDescendants;
            txiter entry{removeIt};
            CalculateDescendants(entry, setDescendants);
            setDescendants.impl.erase(removeIt); // don't update state for self
            int32_t modifySize = -removeIt->GetTxSize();
            CAmount modifyFee = -removeIt->GetModifiedFee();
            int modifySigOps = -removeIt->GetSigOpCost();
            for (raw_txiter dit : setDescendants.impl) {
                mapTx->impl.modify(dit, [=](CTxMemPoolEntry& e){ e.UpdateAncestorState(modifySize, modifyFee, -1, modifySigOps); });
            }
        }
    }
    for (txiter removeIt : entriesToRemove.impl) {
        const CTxMemPoolEntry &entry = *removeIt;
        // Since this is a tx that is already in the mempool, we can call CMPA
        // with fSearchForParents = false.  If the mempool is in a consistent
        // state, then using true or false should both be correct, though false
        // should be a bit faster.
        // However, if we happen to be in the middle of processing a reorg, then
        // the mempool can be in an inconsistent state.  In this case, the set
        // of ancestors reachable via GetMemPoolParents()/GetMemPoolChildren()
        // will be the same as the set of ancestors whose packages include this
        // transaction, because when we add a new transaction to the mempool in
        // addUnchecked(), we assume it has no children, and in the case of a
        // reorg where that assumption is false, the in-mempool children aren't
        // linked to the in-block tx's until UpdateTransactionsFromBlock() is
        // called.
        // So if we're being called during a reorg, ie before
        // UpdateTransactionsFromBlock() has been called, then
        // GetMemPoolParents()/GetMemPoolChildren() will differ from the set of
        // mempool parents we'd calculate by searching, and it's important that
        // we use the cached notion of ancestor transactions as the set of
        // things to update for removal.
        auto ancestors{AssumeCalculateMemPoolAncestors(__func__, entry, Limits::NoLimits(), /*fSearchForParents=*/false)};
        // Note that UpdateAncestorsOf severs the child links that point to
        // removeIt in the entries for the parents of removeIt.
        UpdateAncestorsOf(false, removeIt, *ancestors);
    }
    // After updating all the ancestor sizes, we can now sever the link between each
    // transaction being removed and any mempool children (ie, update CTxMemPoolEntry::m_parents
    // for each direct child of a transaction being removed).
    for (txiter removeIt : entriesToRemove.impl) {
        UpdateChildrenForRemoval(removeIt);
    }
}

void CTxMemPoolEntry::UpdateDescendantState(int32_t modifySize, CAmount modifyFee, int64_t modifyCount)
{
    nSizeWithDescendants += modifySize;
    assert(nSizeWithDescendants > 0);
    nModFeesWithDescendants = SaturatingAdd(nModFeesWithDescendants, modifyFee);
    m_count_with_descendants += modifyCount;
    assert(m_count_with_descendants > 0);
}

void CTxMemPoolEntry::UpdateAncestorState(int32_t modifySize, CAmount modifyFee, int64_t modifyCount, int64_t modifySigOps)
{
    nSizeWithAncestors += modifySize;
    assert(nSizeWithAncestors > 0);
    nModFeesWithAncestors = SaturatingAdd(nModFeesWithAncestors, modifyFee);
    m_count_with_ancestors += modifyCount;
    assert(m_count_with_ancestors > 0);
    nSigOpCostWithAncestors += modifySigOps;
    assert(int(nSigOpCostWithAncestors) >= 0);
}

CTxMemPool::CTxMemPool(const Options& opts)
    : m_check_ratio{opts.check_ratio},
      minerPolicyEstimator{opts.estimator},
      mapTx{std::make_unique<MapTxImpl>()},
      m_max_size_bytes{opts.max_size_bytes},
      m_expiry{opts.expiry},
      m_incremental_relay_feerate{opts.incremental_relay_feerate},
      m_min_relay_feerate{opts.min_relay_feerate},
      m_dust_relay_feerate{opts.dust_relay_feerate},
      m_permit_bare_multisig{opts.permit_bare_multisig},
      m_max_datacarrier_bytes{opts.max_datacarrier_bytes},
      m_require_standard{opts.require_standard},
      m_full_rbf{opts.full_rbf},
      m_limits{opts.limits}
{
}

CTxMemPool::~CTxMemPool() = default;

bool CTxMemPool::isSpent(const COutPoint& outpoint) const
{
    LOCK(cs);
    return mapNextTx.count(outpoint);
}

unsigned int CTxMemPool::GetTransactionsUpdated() const
{
    return nTransactionsUpdated;
}

void CTxMemPool::AddTransactionsUpdated(unsigned int n)
{
    nTransactionsUpdated += n;
}

void CTxMemPool::addUnchecked(const CTxMemPoolEntry &entry, setEntries &setAncestors, bool validFeeEstimate)
{
    // Add to memory pool without checking anything.
    // Used by AcceptToMemoryPool(), which DOES do
    // all the appropriate checks.
    indexed_transaction_set::iterator newit = mapTx->impl.insert(entry).first;

    // Update transaction for any feeDelta created by PrioritiseTransaction
    CAmount delta{0};
    ApplyDelta(entry.GetTx().GetHash(), delta);
    // The following call to UpdateModifiedFee assumes no previous fee modifications
    Assume(entry.GetFee() == entry.GetModifiedFee());
    if (delta) {
        mapTx->impl.modify(newit, [&delta](CTxMemPoolEntry& e) { e.UpdateModifiedFee(delta); });
    }

    // Update cachedInnerUsage to include contained transaction's usage.
    // (When we update the entry for in-mempool parents, memory usage will be
    // further updated.)
    cachedInnerUsage += entry.DynamicMemoryUsage();

    const CTransaction& tx = newit->GetTx();
    std::set<uint256> setParentTransactions;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        mapNextTx.insert(std::make_pair(&tx.vin[i].prevout, &tx));
        setParentTransactions.insert(tx.vin[i].prevout.hash);
    }
    // Don't bother worrying about child transactions of this one.
    // Normal case of a new transaction arriving is that there can't be any
    // children, because such children would be orphans.
    // An exception to that is if a transaction enters that used to be in a block.
    // In that case, our disconnect block logic will call UpdateTransactionsFromBlock
    // to clean up the mess we're leaving here.

    // Update ancestors with information about this tx
    txiter it_entry{newit};
    auto iters{GetIterSet(setParentTransactions)->impl};
    for (const auto& pit : iters) {
        txiter parent{pit};
        UpdateParent(it_entry, parent, true);
    }
    UpdateAncestorsOf(true, it_entry, setAncestors);
    UpdateEntryForAncestors(it_entry, setAncestors);

    nTransactionsUpdated++;
    totalTxSize += entry.GetTxSize();
    m_total_fee += entry.GetFee();
    if (minerPolicyEstimator) {
        minerPolicyEstimator->processTransaction(entry, validFeeEstimate);
    }

    vTxHashes.emplace_back(tx.GetWitnessHash(), std::make_unique<txiter>(newit));
    newit->vTxHashesIdx = vTxHashes.size() - 1;

    TRACE3(mempool, added,
        entry.GetTx().GetHash().data(),
        entry.GetTxSize(),
        entry.GetFee()
    );
}

void CTxMemPool::removeUnchecked(txiter& tx_it, MemPoolRemovalReason reason)
{
    raw_txiter it{tx_it.impl};
    // We increment mempool sequence value no matter removal reason
    // even if not directly reported below.
    uint64_t mempool_sequence = GetAndIncrementSequence();

    if (reason != MemPoolRemovalReason::BLOCK) {
        // Notify clients that a transaction has been removed from the mempool
        // for any reason except being included in a block. Clients interested
        // in transactions included in blocks can subscribe to the BlockConnected
        // notification.
        GetMainSignals().TransactionRemovedFromMempool(it->GetSharedTx(), reason, mempool_sequence);
    }
    TRACE5(mempool, removed,
        it->GetTx().GetHash().data(),
        RemovalReasonToString(reason).c_str(),
        it->GetTxSize(),
        it->GetFee(),
        std::chrono::duration_cast<std::chrono::duration<std::uint64_t>>(it->GetTime()).count()
    );

    const uint256 hash = it->GetTx().GetHash();
    for (const CTxIn& txin : it->GetTx().vin)
        mapNextTx.erase(txin.prevout);

    RemoveUnbroadcastTx(hash, true /* add logging because unchecked */ );

    if (vTxHashes.size() > 1) {
        vTxHashes[it->vTxHashesIdx] = std::move(vTxHashes.back());
        vTxHashes[it->vTxHashesIdx].second->impl->vTxHashesIdx = it->vTxHashesIdx;
        vTxHashes.pop_back();
        if (vTxHashes.size() * 2 < vTxHashes.capacity())
            vTxHashes.shrink_to_fit();
    } else
        vTxHashes.clear();

    totalTxSize -= it->GetTxSize();
    m_total_fee -= it->GetFee();
    cachedInnerUsage -= it->DynamicMemoryUsage();
    cachedInnerUsage -= memusage::DynamicUsage(it->GetMemPoolParentsConst()) + memusage::DynamicUsage(it->GetMemPoolChildrenConst());
    mapTx->impl.erase(it);
    nTransactionsUpdated++;
    if (minerPolicyEstimator) {minerPolicyEstimator->removeTx(hash, false);}
}

bool CTxMemPool::visited(const txiter& it) const
{
    return m_epoch.visited(it.impl->m_epoch_marker);
}

// Calculates descendants of entry that are not already in setDescendants, and adds to
// setDescendants. Assumes entryit is already a tx in the mempool and CTxMemPoolEntry::m_children
// is correct for tx and all descendants.
// Also assumes that if an entry is in setDescendants already, then all
// in-mempool descendants of it are already in setDescendants as well, so that we
// can save time by not iterating over those entries.
void CTxMemPool::CalculateDescendants(txiter& entryit, setEntries& setDescendants) const
{
    raw_setEntries stage;
    if (setDescendants.impl.count(entryit) == 0) {
        stage.insert(entryit);
    }
    // Traverse down the children of entry, only adding children that are not
    // accounted for in setDescendants already (because those children have either
    // already been walked, or will be walked in this iteration).
    while (!stage.empty()) {
        raw_txiter it = *stage.begin();
        setDescendants.impl.insert(it);
        stage.erase(it);

        const CTxMemPoolEntry::Children& children = it->GetMemPoolChildrenConst();
        for (const CTxMemPoolEntry& child : children) {
            raw_txiter childiter = mapTx->impl.iterator_to(child);
            if (!setDescendants.impl.count(childiter)) {
                stage.insert(childiter);
            }
        }
    }
}

void CTxMemPool::removeRecursive(const CTransaction &origTx, MemPoolRemovalReason reason)
{
    // Remove transaction from memory pool
    AssertLockHeld(cs);
        raw_setEntries txToRemove;
        txiter origit = mapTx->impl.find(origTx.GetHash());
        if (origit != mapTx->impl.end()) {
            txToRemove.insert(origit);
        } else {
            // When recursively removing but origTx isn't in the mempool
            // be sure to remove any children that are in the pool. This can
            // happen during chain re-orgs if origTx isn't re-accepted into
            // the mempool for any reason.
            for (unsigned int i = 0; i < origTx.vout.size(); i++) {
                auto it = mapNextTx.find(COutPoint(origTx.GetHash(), i));
                if (it == mapNextTx.end())
                    continue;
                txiter nextit = mapTx->impl.find(it->second->GetHash());
                assert(nextit != mapTx->impl.end());
                txToRemove.insert(nextit);
            }
        }
        setEntries setAllRemoves;
        for (txiter it : txToRemove) {
            CalculateDescendants(it, setAllRemoves);
        }

        RemoveStaged(setAllRemoves, false, reason);
}

void CTxMemPool::removeForReorg(CChain& chain, std::function<bool(txiter&)> check_final_and_mature)
{
    // Remove transactions spending a coinbase which are now immature and no-longer-final transactions
    AssertLockHeld(cs);
    AssertLockHeld(::cs_main);

    raw_setEntries txToRemove;
    for (indexed_transaction_set::const_iterator it = mapTx->impl.begin(); it != mapTx->impl.end(); it++) {
            txiter iter{std::move(it)};
            if (check_final_and_mature(iter)) txToRemove.insert(it);
    }
    setEntries setAllRemoves;
    for (txiter it : txToRemove) {
        CalculateDescendants(it, setAllRemoves);
    }
    RemoveStaged(setAllRemoves, false, MemPoolRemovalReason::REORG);
    for (indexed_transaction_set::const_iterator it = mapTx->impl.begin(); it != mapTx->impl.end(); it++) {
        assert(TestLockPointValidity(chain, it->GetLockPoints()));
    }
}

void CTxMemPool::removeConflicts(const CTransaction &tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    AssertLockHeld(cs);
    for (const CTxIn &txin : tx.vin) {
        auto it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second;
            if (txConflict != tx)
            {
                ClearPrioritisation(txConflict.GetHash());
                removeRecursive(txConflict, MemPoolRemovalReason::CONFLICT);
            }
        }
    }
}

/**
 * Called when a block is connected. Removes from mempool and updates the miner fee estimator.
 */
void CTxMemPool::removeForBlock(const std::vector<CTransactionRef>& vtx, unsigned int nBlockHeight)
{
    AssertLockHeld(cs);
    std::vector<const CTxMemPoolEntry*> entries;
    for (const auto& tx : vtx)
    {
        uint256 hash = tx->GetHash();

        indexed_transaction_set::iterator i = mapTx->impl.find(hash);
        if (i != mapTx->impl.end())
            entries.push_back(&*i);
    }
    // Before the txs in the new block have been removed from the mempool, update policy estimates
    if (minerPolicyEstimator) {minerPolicyEstimator->processBlock(nBlockHeight, entries);}
    for (const auto& tx : vtx)
    {
        txiter it = mapTx->impl.find(tx->GetHash());
        if (it != mapTx->impl.end()) {
            setEntries stage;
            stage.impl.insert(it);
            RemoveStaged(stage, true, MemPoolRemovalReason::BLOCK);
        }
        removeConflicts(*tx);
        ClearPrioritisation(tx->GetHash());
    }
    lastRollingFeeUpdate = GetTime();
    blockSinceLastRollingFeeBump = true;
}

namespace {
class DepthAndScoreComparator
{
public:
    bool operator()(const indexed_transaction_set::const_iterator& a, const indexed_transaction_set::const_iterator& b)
    {
        uint64_t counta = a->GetCountWithAncestors();
        uint64_t countb = b->GetCountWithAncestors();
        if (counta == countb) {
            return CompareTxMemPoolEntryByScore()(*a, *b);
        }
        return counta < countb;
    }
};
} // namespace

static std::vector<indexed_transaction_set::const_iterator> GetSortedDepthAndScore(
    const std::unique_ptr<MempoolMultiIndex::MapTxImpl>& mapTx, RecursiveMutex& cs) EXCLUSIVE_LOCKS_REQUIRED(cs)
{
    std::vector<indexed_transaction_set::const_iterator> iters;
    AssertLockHeld(cs);

    iters.reserve(mapTx->impl.size());

    for (indexed_transaction_set::iterator mi = mapTx->impl.begin(); mi != mapTx->impl.end(); ++mi) {
        iters.push_back(mi);
    }
    std::sort(iters.begin(), iters.end(), DepthAndScoreComparator());
    return iters;
}

void CTxMemPool::check(const CCoinsViewCache& active_coins_tip, int64_t spendheight) const
{
    if (m_check_ratio == 0) return;

    if (GetRand(m_check_ratio) >= 1) return;

    AssertLockHeld(::cs_main);
    LOCK(cs);
    LogPrint(BCLog::MEMPOOL, "Checking mempool with %u transactions and %u inputs\n", (unsigned int)mapTx->impl.size(), (unsigned int)mapNextTx.size());

    uint64_t checkTotal = 0;
    CAmount check_total_fee{0};
    uint64_t innerUsage = 0;
    uint64_t prev_ancestor_count{0};

    CCoinsViewCache mempoolDuplicate(const_cast<CCoinsViewCache*>(&active_coins_tip));

    for (const auto& it : GetSortedDepthAndScore(mapTx, cs)) {
        checkTotal += it->GetTxSize();
        check_total_fee += it->GetFee();
        innerUsage += it->DynamicMemoryUsage();
        const CTransaction& tx = it->GetTx();
        innerUsage += memusage::DynamicUsage(it->GetMemPoolParentsConst()) + memusage::DynamicUsage(it->GetMemPoolChildrenConst());
        CTxMemPoolEntry::Parents setParentCheck;
        for (const CTxIn &txin : tx.vin) {
            // Check that every mempool transaction's inputs refer to available coins, or other mempool tx's.
            indexed_transaction_set::const_iterator it2 = mapTx->impl.find(txin.prevout.hash);
            if (it2 != mapTx->impl.end()) {
                const CTransaction& tx2 = it2->GetTx();
                assert(tx2.vout.size() > txin.prevout.n && !tx2.vout[txin.prevout.n].IsNull());
                setParentCheck.insert(*it2);
            }
            // We are iterating through the mempool entries sorted in order by ancestor count.
            // All parents must have been checked before their children and their coins added to
            // the mempoolDuplicate coins cache.
            assert(mempoolDuplicate.HaveCoin(txin.prevout));
            // Check whether its inputs are marked in mapNextTx.
            auto it3 = mapNextTx.find(txin.prevout);
            assert(it3 != mapNextTx.end());
            assert(it3->first == &txin.prevout);
            assert(it3->second == &tx);
        }
        auto comp = [](const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) -> bool {
            return a.GetTx().GetHash() == b.GetTx().GetHash();
        };
        assert(setParentCheck.size() == it->GetMemPoolParentsConst().size());
        assert(std::equal(setParentCheck.begin(), setParentCheck.end(), it->GetMemPoolParentsConst().begin(), comp));
        // Verify ancestor state is correct.
        auto ancestors{AssumeCalculateMemPoolAncestors(__func__, *it, Limits::NoLimits())};
        uint64_t nCountCheck = ancestors->impl.size() + 1;
        int32_t nSizeCheck = it->GetTxSize();
        CAmount nFeesCheck = it->GetModifiedFee();
        int64_t nSigOpCheck = it->GetSigOpCost();

        for (raw_txiter ancestorIt : ancestors->impl) {
            nSizeCheck += ancestorIt->GetTxSize();
            nFeesCheck += ancestorIt->GetModifiedFee();
            nSigOpCheck += ancestorIt->GetSigOpCost();
        }

        assert(it->GetCountWithAncestors() == nCountCheck);
        assert(it->GetSizeWithAncestors() == nSizeCheck);
        assert(it->GetSigOpCostWithAncestors() == nSigOpCheck);
        assert(it->GetModFeesWithAncestors() == nFeesCheck);
        // Sanity check: we are walking in ascending ancestor count order.
        assert(prev_ancestor_count <= it->GetCountWithAncestors());
        prev_ancestor_count = it->GetCountWithAncestors();

        // Check children against mapNextTx
        CTxMemPoolEntry::Children setChildrenCheck;
        auto iter = mapNextTx.lower_bound(COutPoint(it->GetTx().GetHash(), 0));
        int32_t child_sizes{0};
        for (; iter != mapNextTx.end() && iter->first->hash == it->GetTx().GetHash(); ++iter) {
            raw_txiter childit = mapTx->impl.find(iter->second->GetHash());
            assert(childit != mapTx->impl.end()); // mapNextTx points to in-mempool transactions
            if (setChildrenCheck.insert(*childit).second) {
                child_sizes += childit->GetTxSize();
            }
        }
        assert(setChildrenCheck.size() == it->GetMemPoolChildrenConst().size());
        assert(std::equal(setChildrenCheck.begin(), setChildrenCheck.end(), it->GetMemPoolChildrenConst().begin(), comp));
        // Also check to make sure size is greater than sum with immediate children.
        // just a sanity check, not definitive that this calc is correct...
        assert(it->GetSizeWithDescendants() >= child_sizes + it->GetTxSize());

        TxValidationState dummy_state; // Not used. CheckTxInputs() should always pass
        CAmount txfee = 0;
        assert(!tx.IsCoinBase());
        assert(Consensus::CheckTxInputs(tx, dummy_state, mempoolDuplicate, spendheight, txfee));
        for (const auto& input: tx.vin) mempoolDuplicate.SpendCoin(input.prevout);
        AddCoins(mempoolDuplicate, tx, std::numeric_limits<int>::max());
    }
    for (auto it = mapNextTx.cbegin(); it != mapNextTx.cend(); it++) {
        uint256 hash = it->second->GetHash();
        indexed_transaction_set::const_iterator it2 = mapTx->impl.find(hash);
        const CTransaction& tx = it2->GetTx();
        assert(it2 != mapTx->impl.end());
        assert(&tx == it->second);
    }

    assert(totalTxSize == checkTotal);
    assert(m_total_fee == check_total_fee);
    assert(innerUsage == cachedInnerUsage);
}

bool CTxMemPool::CompareDepthAndScore(const uint256& hasha, const uint256& hashb, bool wtxid)
{
    /* Return `true` if hasha should be considered sooner than hashb. Namely when:
     *   a is not in the mempool, but b is
     *   both are in the mempool and a has fewer ancestors than b
     *   both are in the mempool and a has a higher score than b
     */
    LOCK(cs);
    indexed_transaction_set::const_iterator j = wtxid ? (*get_iter_from_wtxid(hashb)).impl : mapTx->impl.find(hashb);
    if (j == mapTx->impl.end()) return false;
    indexed_transaction_set::const_iterator i = wtxid ? (*get_iter_from_wtxid(hasha)).impl : mapTx->impl.find(hasha);
    if (i == mapTx->impl.end()) return true;
    uint64_t counta = i->GetCountWithAncestors();
    uint64_t countb = j->GetCountWithAncestors();
    if (counta == countb) {
        return CompareTxMemPoolEntryByScore()(*i, *j);
    }
    return counta < countb;
}


void CTxMemPool::queryHashes(std::vector<uint256>& vtxid) const
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore(mapTx, cs);

    vtxid.clear();
    vtxid.reserve(mapTx->impl.size());

    for (auto it : iters) {
        vtxid.push_back(it->GetTx().GetHash());
    }
}

static TxMempoolInfo GetInfo(indexed_transaction_set::const_iterator it) {
    return TxMempoolInfo{it->GetSharedTx(), it->GetTime(), it->GetFee(), it->GetTxSize(), it->GetModifiedFee() - it->GetFee()};
}

std::vector<TxMempoolInfo> CTxMemPool::infoAll() const
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore(mapTx, cs);

    std::vector<TxMempoolInfo> ret;
    ret.reserve(mapTx->impl.size());
    for (auto it : iters) {
        ret.push_back(GetInfo(it));
    }

    return ret;
}

CTransactionRef CTxMemPool::get(const uint256& hash) const
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = mapTx->impl.find(hash);
    if (i == mapTx->impl.end())
        return nullptr;
    return i->GetSharedTx();
}

TxMempoolInfo CTxMemPool::info(const GenTxid& gtxid) const
{
    LOCK(cs);
    indexed_transaction_set::const_iterator i = (gtxid.IsWtxid() ? (*get_iter_from_wtxid(gtxid.GetHash())).impl : mapTx->impl.find(gtxid.GetHash()));
    if (i == mapTx->impl.end())
        return TxMempoolInfo();
    return GetInfo(i);
}

void CTxMemPool::PrioritiseTransaction(const uint256& hash, const CAmount& nFeeDelta)
{
    {
        LOCK(cs);
        CAmount &delta = mapDeltas[hash];
        delta = SaturatingAdd(delta, nFeeDelta);
        raw_txiter it = mapTx->impl.find(hash);
        if (it != mapTx->impl.end()) {
            mapTx->impl.modify(it, [&nFeeDelta](CTxMemPoolEntry& e) { e.UpdateModifiedFee(nFeeDelta); });
            // Now update all ancestors' modified fees with descendants
            auto ancestors{AssumeCalculateMemPoolAncestors(__func__, *it, Limits::NoLimits(), /*fSearchForParents=*/false)};
            for (txiter ancestorIt : ancestors->impl) {
                mapTx->impl.modify(ancestorIt, [=](CTxMemPoolEntry& e){ e.UpdateDescendantState(0, nFeeDelta, 0);});
            }
            // Now update all descendants' modified fees with ancestors
            setEntries setDescendants;
            txiter entryit{it};
            CalculateDescendants(entryit, setDescendants);
            setDescendants.impl.erase(it);
            for (raw_txiter descendantIt : setDescendants.impl) {
                mapTx->impl.modify(descendantIt, [=](CTxMemPoolEntry& e){ e.UpdateAncestorState(0, nFeeDelta, 0, 0); });
            }
            ++nTransactionsUpdated;
        }
        if (delta == 0) {
            mapDeltas.erase(hash);
            LogPrintf("PrioritiseTransaction: %s (%sin mempool) delta cleared\n", hash.ToString(), it == mapTx->impl.end() ? "not " : "");
        } else {
            LogPrintf("PrioritiseTransaction: %s (%sin mempool) fee += %s, new delta=%s\n",
                      hash.ToString(),
                      it == mapTx->impl.end() ? "not " : "",
                      FormatMoney(nFeeDelta),
                      FormatMoney(delta));
        }
    }
}

void CTxMemPool::ApplyDelta(const uint256& hash, CAmount &nFeeDelta) const
{
    AssertLockHeld(cs);
    std::map<uint256, CAmount>::const_iterator pos = mapDeltas.find(hash);
    if (pos == mapDeltas.end())
        return;
    const CAmount &delta = pos->second;
    nFeeDelta += delta;
}

void CTxMemPool::ClearPrioritisation(const uint256& hash)
{
    AssertLockHeld(cs);
    mapDeltas.erase(hash);
}

std::vector<CTxMemPool::delta_info> CTxMemPool::GetPrioritisedTransactions() const
{
    AssertLockNotHeld(cs);
    LOCK(cs);
    std::vector<delta_info> result;
    result.reserve(mapDeltas.size());
    for (const auto& [txid, delta] : mapDeltas) {
        const auto iter{mapTx->impl.find(txid)};
        const bool in_mempool{iter != mapTx->impl.end()};
        std::optional<CAmount> modified_fee;
        if (in_mempool) modified_fee = iter->GetModifiedFee();
        result.emplace_back(delta_info{in_mempool, delta, modified_fee, txid});
    }
    return result;
}

const CTransaction* CTxMemPool::GetConflictTx(const COutPoint& prevout) const
{
    const auto it = mapNextTx.find(prevout);
    return it == mapNextTx.end() ? nullptr : it->second;
}

std::optional<std::unique_ptr<txiter>> CTxMemPool::GetIter(const uint256& txid) const
{
    auto it{std::make_unique<txiter>(mapTx->impl.find(txid))};
    if (*it != mapTx->impl.end()) return it;
    return std::nullopt;
}

std::unique_ptr<setEntries> CTxMemPool::GetIterSet(const std::set<uint256>& hashes) const
{
    auto ret{std::make_unique<setEntries>()};
    for (const auto& h : hashes) {
        auto mi = GetIter(h);
        if (mi) {
            raw_txiter iter = mi.value()->impl;
            ret->impl.insert(iter);
        }
    }
    return ret;
}

std::vector<std::unique_ptr<txiter>> CTxMemPool::GetIterVec(const std::vector<uint256>& txids) const
{
    AssertLockHeld(cs);
    std::vector<std::unique_ptr<txiter>> ret;
    ret.reserve(txids.size());
    for (const auto& txid : txids) {
        auto it{GetIter(txid)};
        if (!it) return {};
        ret.push_back(std::move(*it));
    }
    return ret;
}

bool CTxMemPool::HasNoInputsOf(const CTransaction &tx) const
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        if (exists(GenTxid::Txid(tx.vin[i].prevout.hash)))
            return false;
    return true;
}

CCoinsViewMemPool::CCoinsViewMemPool(CCoinsView* baseIn, const CTxMemPool& mempoolIn) : CCoinsViewBacked(baseIn), mempool(mempoolIn) { }

bool CCoinsViewMemPool::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    // Check to see if the inputs are made available by another tx in the package.
    // These Coins would not be available in the underlying CoinsView.
    if (auto it = m_temp_added.find(outpoint); it != m_temp_added.end()) {
        coin = it->second;
        return true;
    }

    // If an entry in the mempool exists, always return that one, as it's guaranteed to never
    // conflict with the underlying cache, and it cannot have pruned entries (as it contains full)
    // transactions. First checking the underlying cache risks returning a pruned entry instead.
    CTransactionRef ptx = mempool.get(outpoint.hash);
    if (ptx) {
        if (outpoint.n < ptx->vout.size()) {
            coin = Coin(ptx->vout[outpoint.n], MEMPOOL_HEIGHT, false);
            return true;
        } else {
            return false;
        }
    }
    return base->GetCoin(outpoint, coin);
}

void CCoinsViewMemPool::PackageAddTransaction(const CTransactionRef& tx)
{
    for (unsigned int n = 0; n < tx->vout.size(); ++n) {
        m_temp_added.emplace(COutPoint(tx->GetHash(), n), Coin(tx->vout[n], MEMPOOL_HEIGHT, false));
    }
}

size_t CTxMemPool::DynamicMemoryUsage() const {
    LOCK(cs);
    // Estimate the overhead of mapTx->impl.to be 15 pointers + an allocation, as no exact formula for boost::multi_index_contained is implemented.
    return memusage::MallocUsage(sizeof(CTxMemPoolEntry) + 15 * sizeof(void*)) * mapTx->impl.size() + memusage::DynamicUsage(mapNextTx) + memusage::DynamicUsage(mapDeltas) + memusage::DynamicUsage(vTxHashes) + cachedInnerUsage;
}

void CTxMemPool::RemoveUnbroadcastTx(const uint256& txid, const bool unchecked) {
    LOCK(cs);

    if (m_unbroadcast_txids.erase(txid))
    {
        LogPrint(BCLog::MEMPOOL, "Removed %i from set of unbroadcast txns%s\n", txid.GetHex(), (unchecked ? " before confirmation that txn was sent out" : ""));
    }
}

void CTxMemPool::RemoveStaged(setEntries &stage, bool updateDescendants, MemPoolRemovalReason reason) {
    AssertLockHeld(cs);
    UpdateForRemoveFromMempool(stage, updateDescendants);
    for (txiter it : stage.impl) {
        removeUnchecked(it, reason);
    }
}

int CTxMemPool::Expire(std::chrono::seconds time)
{
    AssertLockHeld(cs);
    indexed_transaction_set::index<entry_time>::type::iterator it = mapTx->impl.get<entry_time>().begin();
    raw_setEntries toremove;
    while (it != mapTx->impl.get<entry_time>().end() && it->GetTime() < time) {
        toremove.insert(mapTx->impl.project<0>(it));
        it++;
    }
    setEntries stage;
    for (txiter removeit : toremove) {
        CalculateDescendants(removeit, stage);
    }
    RemoveStaged(stage, false, MemPoolRemovalReason::EXPIRY);
    return stage.impl.size();
}

void CTxMemPool::addUnchecked(const CTxMemPoolEntry &entry, bool validFeeEstimate)
{
    auto ancestors{AssumeCalculateMemPoolAncestors(__func__, entry, Limits::NoLimits())};
    return addUnchecked(entry, *ancestors, validFeeEstimate);
}

void CTxMemPool::UpdateChild(txiter& entry, txiter& child, bool add)
{
    AssertLockHeld(cs);
    CTxMemPoolEntry::Children s;
    if (add && entry.impl->GetMemPoolChildren().insert(*child).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && entry.impl->GetMemPoolChildren().erase(*child)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

void CTxMemPool::UpdateParent(txiter& entry, txiter& parent, bool add)
{
    AssertLockHeld(cs);
    CTxMemPoolEntry::Parents s;
    if (add && entry.impl->GetMemPoolParents().insert(*parent).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && entry.impl->GetMemPoolParents().erase(*parent)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

CFeeRate CTxMemPool::GetMinFee(size_t sizelimit) const {
    LOCK(cs);
    if (!blockSinceLastRollingFeeBump || rollingMinimumFeeRate == 0)
        return CFeeRate(llround(rollingMinimumFeeRate));

    int64_t time = GetTime();
    if (time > lastRollingFeeUpdate + 10) {
        double halflife = ROLLING_FEE_HALFLIFE;
        if (DynamicMemoryUsage() < sizelimit / 4)
            halflife /= 4;
        else if (DynamicMemoryUsage() < sizelimit / 2)
            halflife /= 2;

        rollingMinimumFeeRate = rollingMinimumFeeRate / pow(2.0, (time - lastRollingFeeUpdate) / halflife);
        lastRollingFeeUpdate = time;

        if (rollingMinimumFeeRate < (double)m_incremental_relay_feerate.GetFeePerK() / 2) {
            rollingMinimumFeeRate = 0;
            return CFeeRate(0);
        }
    }
    return std::max(CFeeRate(llround(rollingMinimumFeeRate)), m_incremental_relay_feerate);
}

void CTxMemPool::trackPackageRemoved(const CFeeRate& rate) {
    AssertLockHeld(cs);
    if (rate.GetFeePerK() > rollingMinimumFeeRate) {
        rollingMinimumFeeRate = rate.GetFeePerK();
        blockSinceLastRollingFeeBump = false;
    }
}

void CTxMemPool::TrimToSize(size_t sizelimit, std::vector<COutPoint>* pvNoSpendsRemaining) {
    AssertLockHeld(cs);

    unsigned nTxnRemoved = 0;
    CFeeRate maxFeeRateRemoved(0);
    while (!mapTx->impl.empty() && DynamicMemoryUsage() > sizelimit) {
        indexed_transaction_set::index<descendant_score>::type::iterator it = mapTx->impl.get<descendant_score>().begin();

        // We set the new mempool min fee to the feerate of the removed set, plus the
        // "minimum reasonable fee rate" (ie some value under which we consider txn
        // to have 0 fee). This way, we don't allow txn to enter mempool with feerate
        // equal to txn which were removed with no block in between.
        CFeeRate removed(it->GetModFeesWithDescendants(), it->GetSizeWithDescendants());
        removed += m_incremental_relay_feerate;
        trackPackageRemoved(removed);
        maxFeeRateRemoved = std::max(maxFeeRateRemoved, removed);

        setEntries stage;
        txiter iter{mapTx->impl.project<0>(it)};
        CalculateDescendants(iter, stage);
        nTxnRemoved += stage.impl.size();

        std::vector<CTransaction> txn;
        if (pvNoSpendsRemaining) {
            txn.reserve(stage.impl.size());
            for (raw_txiter iter : stage.impl)
                txn.push_back(iter->GetTx());
        }
        RemoveStaged(stage, false, MemPoolRemovalReason::SIZELIMIT);
        if (pvNoSpendsRemaining) {
            for (const CTransaction& tx : txn) {
                for (const CTxIn& txin : tx.vin) {
                    if (exists(GenTxid::Txid(txin.prevout.hash))) continue;
                    pvNoSpendsRemaining->push_back(txin.prevout);
                }
            }
        }
    }

    if (maxFeeRateRemoved > CFeeRate(0)) {
        LogPrint(BCLog::MEMPOOL, "Removed %u txn, rolling minimum fee bumped to %s\n", nTxnRemoved, maxFeeRateRemoved.ToString());
    }
}

uint64_t CTxMemPool::CalculateDescendantMaximum(txiter& entry) const {
    // find parent with highest descendant count
    std::vector<txiter> candidates;
    raw_setEntries counted;
    candidates.push_back(entry);
    uint64_t maximum = 0;
    while (candidates.size()) {
        txiter candidate = candidates.back();
        candidates.pop_back();
        if (!counted.insert(candidate).second) continue;
        const CTxMemPoolEntry::Parents& parents = candidate.impl->GetMemPoolParentsConst();
        if (parents.size() == 0) {
            maximum = std::max(maximum, candidate.impl->GetCountWithDescendants());
        } else {
            for (const CTxMemPoolEntry& i : parents) {
                candidates.push_back(mapTx->impl.iterator_to(i));
            }
        }
    }
    return maximum;
}

void CTxMemPool::GetTransactionAncestry(const uint256& txid, size_t& ancestors, size_t& descendants, size_t* const ancestorsize, CAmount* const ancestorfees) const {
    LOCK(cs);
    auto it = mapTx->impl.find(txid);
    ancestors = descendants = 0;
    if (it != mapTx->impl.end()) {
        ancestors = it->GetCountWithAncestors();
        if (ancestorsize) *ancestorsize = it->GetSizeWithAncestors();
        if (ancestorfees) *ancestorfees = it->GetModFeesWithAncestors();
        txiter iter{it};
        descendants = CalculateDescendantMaximum(iter);
    }
}

bool CTxMemPool::GetLoadTried() const
{
    LOCK(cs);
    return m_load_tried;
}

void CTxMemPool::SetLoadTried(bool load_tried)
{
    LOCK(cs);
    m_load_tried = load_tried;
}

unsigned long CTxMemPool::size() const
{
    LOCK(cs);
    return mapTx->impl.size();
}

bool CTxMemPool::exists(const GenTxid& gtxid) const
{
    LOCK(cs);
    if (gtxid.IsWtxid()) {
        return (mapTx->impl.get<index_by_wtxid>().count(gtxid.GetHash()) != 0);
    }
    return (mapTx->impl.count(gtxid.GetHash()) != 0);
}

std::unique_ptr<txiter> CTxMemPool::get_iter_from_wtxid(const uint256& wtxid) const
{
    AssertLockHeld(cs);
    return std::make_unique<txiter>(mapTx->impl.project<0>(mapTx->impl.get<index_by_wtxid>().find(wtxid)));
}

std::string RemovalReasonToString(const MemPoolRemovalReason& r) noexcept
{
    switch (r) {
        case MemPoolRemovalReason::EXPIRY: return "expiry";
        case MemPoolRemovalReason::SIZELIMIT: return "sizelimit";
        case MemPoolRemovalReason::REORG: return "reorg";
        case MemPoolRemovalReason::BLOCK: return "block";
        case MemPoolRemovalReason::CONFLICT: return "conflict";
        case MemPoolRemovalReason::REPLACED: return "replaced";
    }
    assert(false);
}

std::vector<std::unique_ptr<txiter>> CTxMemPool::GatherClusters(const std::vector<uint256>& txids) const
{
    AssertLockHeld(cs);
    std::vector<std::unique_ptr<txiter>> clustered_txs{GetIterVec(txids)};
    // Use epoch: visiting an entry means we have added it to the clustered_txs vector. It does not
    // necessarily mean the entry has been processed.
    WITH_FRESH_EPOCH(m_epoch);
    for (const auto& it : clustered_txs) {
        visited(*it);
    }
    // i = index of where the list of entries to process starts
    for (size_t i{0}; i < clustered_txs.size(); ++i) {
        // DoS protection: if there are 500 or more entries to process, just quit.
        if (clustered_txs.size() > 500) return {};
        const std::unique_ptr<txiter>& tx_iter = clustered_txs.at(i);
        for (const auto& entries : {(*tx_iter).impl->GetMemPoolParentsConst(), (*tx_iter).impl->GetMemPoolChildrenConst()}) {
            for (const CTxMemPoolEntry& entry : entries) {
                const txiter entry_it{mapTx->impl.iterator_to(entry)};
                if (!visited(entry_it)) {
                    clustered_txs.push_back(std::make_unique<txiter>(entry_it));
                }
            }
        }
    }
    return clustered_txs;
}



// -------------------- Now starts the miner part

void ClearSetEntry(const std::unique_ptr<SetEntriesImpl>& setEntries) {
    setEntries->impl.impl.clear();
}

// Container for tracking updates to ancestor feerate as we include (parent)
// transactions in a block
struct CTxMemPoolModifiedEntry {
    explicit CTxMemPoolModifiedEntry(raw_txiter entry)
    {
        iter = entry;
        nSizeWithAncestors = entry->GetSizeWithAncestors();
        nModFeesWithAncestors = entry->GetModFeesWithAncestors();
        nSigOpCostWithAncestors = entry->GetSigOpCostWithAncestors();
    }

    CAmount GetModifiedFee() const { return iter->GetModifiedFee(); }
    uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
    CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
    size_t GetTxSize() const { return iter->GetTxSize(); }
    const CTransaction& GetTx() const { return iter->GetTx(); }

    raw_txiter iter;
    uint64_t nSizeWithAncestors;
    CAmount nModFeesWithAncestors;
    int64_t nSigOpCostWithAncestors;
};

struct update_for_parent_inclusion
{
    explicit update_for_parent_inclusion(raw_txiter it) : iter(it) {}

    void operator() (CTxMemPoolModifiedEntry &e)
    {
        e.nModFeesWithAncestors -= iter->GetModifiedFee();
        e.nSizeWithAncestors -= iter->GetTxSize();
        e.nSigOpCostWithAncestors -= iter->GetSigOpCost();
    }

    raw_txiter iter;
};

/** Comparator for CTxMemPool::txiter objects.
 *  It simply compares the internal memory address of the CTxMemPoolEntry object
 *  pointed to. This means it has no meaning, and is only useful for using them
 *  as key in other indexes.
 */
struct CompareCTxMemPoolIter {
    bool operator()(const raw_txiter& a, const raw_txiter& b) const
    {
        return &(*a) < &(*b);
    }
};

struct modifiedentry_iter {
    typedef raw_txiter result_type;
    result_type operator() (const CTxMemPoolModifiedEntry &entry) const
    {
        return entry.iter;
    }
};

// A comparator that sorts transactions based on number of ancestors.
// This is sufficient to sort an ancestor package in an order that is valid
// to appear in a block.
struct CompareTxIterByAncestorCount {
    bool operator()(const raw_txiter& a, const raw_txiter& b) const
    {
        if (a->GetCountWithAncestors() != b->GetCountWithAncestors()) {
            return a->GetCountWithAncestors() < b->GetCountWithAncestors();
        }
        return CompareIteratorByHash()(a, b);
    }
};

typedef boost::multi_index_container<
    CTxMemPoolModifiedEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            modifiedentry_iter,
            CompareCTxMemPoolIter
        >,
        // sorted by modified ancestor fee rate
        boost::multi_index::ordered_non_unique<
            // Reuse same tag from CTxMemPool's similar index
            boost::multi_index::tag<ancestor_score>,
            boost::multi_index::identity<CTxMemPoolModifiedEntry>,
            CompareTxMemPoolEntryByAncestorFee
        >
    >
> indexed_modified_transaction_set;

typedef indexed_modified_transaction_set::nth_index<0>::type::iterator modtxiter;
typedef indexed_modified_transaction_set::index<ancestor_score>::type::iterator modtxscoreiter;

/** Add descendants of given transactions to mapModifiedTx with ancestor
 * state updated assuming given transactions are inBlock. Returns number
 * of updated descendants. */
static int UpdatePackagesForAdded(const CTxMemPool& mempool,
                                  const CTxMemPool::setEntries& alreadyAdded,
                                  indexed_modified_transaction_set& mapModifiedTx) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs)
{
    AssertLockHeld(mempool.cs);

    int nDescendantsUpdated = 0;
    for (CTxMemPool::txiter it : alreadyAdded.impl) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants.impl) {
            if (alreadyAdded.impl.count(desc)) {
                continue;
            }
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                mit = mapModifiedTx.insert(modEntry).first;
            }
            mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
        }
    }
    return nDescendantsUpdated;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
static bool TestPackageTransactions(const CTxMemPool::setEntries& package, int nHeight, int64_t m_lock_time_cutoff)
{
    for (raw_txiter it : package.impl) {
        if (!IsFinalTx(it->GetTx(), nHeight, m_lock_time_cutoff)) {
            return false;
        }
    }
    return true;
}

static void AddToBlock(std::unique_ptr<node::CBlockTemplate>& block_template, 
    raw_txiter iter,
    bool fPrintPriority,
    uint64_t& nBlockWeight,
    uint64_t& nBlockTx,
    uint64_t& nBlockSigOpsCost,
    CAmount& nFees,
    CTxMemPool::setEntries& inBlock)
{
    block_template->block.vtx.emplace_back(iter->GetSharedTx());
    block_template->vTxFees.push_back(iter->GetFee());
    block_template->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.impl.insert(iter);

    // bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee rate %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

static void SortForBlock(const CTxMemPool::setEntries& package, std::vector<raw_txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.impl.begin(), package.impl.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

static bool TestPackage(const uint64_t packageSize, 
    const int64_t packageSigOpsCost,
    const uint64_t nBlockWeight,
    const size_t nBlockMaxWeight,
    const uint64_t nBlockSigOpsCost)
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight) {
        return false;
    }
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST) {
        return false;
    }
    return true;
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void CTxMemPool::addPackageTxs(int& nPackagesSelected, 
    int& nDescendantsUpdated,
    std::unique_ptr<node::CBlockTemplate>& block_template,
    const std::unique_ptr<SetEntriesImpl>& inBlock,
    const size_t nBlockMaxWeight,
    const CFeeRate blockMinFeeRate,
    uint64_t& nBlockWeight,
    uint64_t& nBlockTx,
    uint64_t& nBlockSigOpsCost,
    CAmount& nFees,
    const int nHeight,
    const int64_t m_lock_time_cutoff,
    bool fPrintPriority) const
{
    AssertLockHeld(cs);

    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    raw_setEntries failedTx;

    indexed_transaction_set::index<ancestor_score>::type::iterator mi = mapTx->impl.get<ancestor_score>().begin();
    raw_txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mapTx->impl.get<ancestor_score>().end() || !mapModifiedTx.empty()) {
        // First try to find a new transaction in mapTx->impl.to evaluate.
        //
        // Skip entries in mapTx->impl.that are already in a block or are present
        // in mapModifiedTx (which implies that the mapTx->impl.ancestor state is
        // stale due to ancestor inclusion in the block)
        // Also skip transactions that we've already failed to add. This can happen if
        // we consider a transaction in mapModifiedTx and it fails: we can then
        // potentially consider it again while walking mapTx->impl.  It's currently
        // guaranteed to fail again, but as a belt-and-suspenders check we put it in
        // failedTx and avoid re-evaluation, since the re-evaluation would be using
        // cached size/sigops/fee values that are not actually correct.
        /** Return true if given transaction from mapTx->impl.has already been evaluated,
         * or if the transaction's cached data in mapTx->impl.is incorrect. */
        if (mi != mapTx->impl.get<ancestor_score>().end()) {
            auto it = mapTx->impl.project<0>(mi);
            assert(it != mapTx->impl.end());
            if (mapModifiedTx.count(it) || inBlock->impl.impl.count(it) || failedTx.count(it)) {
                ++mi;
                continue;
            }
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx->impl. or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mapTx->impl.get<ancestor_score>().end()) {
            // We're out of entries in mapTx->impl. use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx->impl.entry to the mapModifiedTx entry
            iter = mapTx->impl.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx->impl.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx->impl.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx->impl.entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock->impl.impl.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost, nBlockWeight, nBlockMaxWeight, nBlockSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        auto ancestors{AssumeCalculateMemPoolAncestors(__func__, *iter, CTxMemPool::Limits::NoLimits(), /*fSearchForParents=*/false)};

        for (raw_setEntries::iterator iit = ancestors->impl.begin(); iit != ancestors->impl.end(); ) {
            // Only test txs not already in the block
            if (inBlock->impl.impl.count(*iit)) {
                ancestors->impl.erase(iit++);
            } else {
                iit++;
            }
        }

        ancestors->impl.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(*ancestors, nHeight, m_lock_time_cutoff)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<raw_txiter> sortedEntries;
        SortForBlock(*ancestors, sortedEntries);

        for (size_t i = 0; i < sortedEntries.size(); ++i) {
            AddToBlock(block_template, sortedEntries[i], fPrintPriority, nBlockWeight, nBlockTx, nBlockSigOpsCost, nFees, inBlock->impl);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(*this, *ancestors, mapModifiedTx);
    }
}

DisconnectedBlockTransactions::DisconnectedBlockTransactions() :
    queuedTx{std::make_unique<MempoolMultiIndex::IndexedDisconnectedTransactionsImpl>()} {}

DisconnectedBlockTransactions::~DisconnectedBlockTransactions() {
    assert(queuedTx->impl.empty());
}

size_t DisconnectedBlockTransactions::DynamicMemoryUsage() const {
    return memusage::MallocUsage(sizeof(CTransactionRef) + 6 * sizeof(void*)) * queuedTx->impl.size() + cachedInnerUsage;
}

void DisconnectedBlockTransactions::addTransaction(const CTransactionRef& tx)
{
    queuedTx->impl.insert(tx);
    cachedInnerUsage += RecursiveDynamicUsage(tx);
}

// Remove entries based on txid_index, and update memory usage.
void DisconnectedBlockTransactions::removeForBlock(const std::vector<CTransactionRef>& vtx)
{
    // Short-circuit in the common case of a block being added to the tip
    if (queuedTx->impl.empty()) {
        return;
    }
    for (auto const &tx : vtx) {
        auto it = queuedTx->impl.find(tx->GetHash());
        if (it != queuedTx->impl.end()) {
            cachedInnerUsage -= RecursiveDynamicUsage(*it);
            queuedTx->impl.erase(it);
        }
    }
}

// Remove an entry by insertion_order index, and update memory usage.
void DisconnectedBlockTransactions::removeEntry(MempoolMultiIndex::DisconnectedTransactionsIteratorImpl* entry)
{
    cachedInnerUsage -= RecursiveDynamicUsage(**entry);
    queuedTx->impl.get<MempoolMultiIndex::insertion_order>().erase(*entry);
}

void DisconnectedBlockTransactions::clear()
{
    cachedInnerUsage = 0;
    queuedTx->impl.clear();
}
