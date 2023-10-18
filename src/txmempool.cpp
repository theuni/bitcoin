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

bool CompareTxMemPoolEntryByScore::operator()(const mempool_storage_iterator& a, const mempool_storage_iterator& b) const
{
        double f1 = (double)a->first.GetFee() * b->first.GetTxSize();
        double f2 = (double)b->first.GetFee() * a->first.GetTxSize();
        if (f1 == f2) {
            return b->first.GetTx().GetHash() < a->first.GetTx().GetHash();
        }
        return f1 > f2;
}

bool CompareTxMemPoolEntryByEntryTime::operator()(const mempool_storage_iterator& a, const mempool_storage_iterator& b) const
{
    return a->first.GetTime() < b->first.GetTime();
}

bool CompareTxMemPoolEntryByAncestorFee::operator()(const mempool_storage_iterator& a, const mempool_storage_iterator& b) const
{
    double a_mod_fee, a_size, b_mod_fee, b_size;

    GetModFeeAndSize(a->first, a_mod_fee, a_size);
    GetModFeeAndSize(b->first, b_mod_fee, b_size);

        // Avoid division by rewriting (a/b > c/d) as (a*d > c*b).
    double f1 = a_mod_fee * b_size;
    double f2 = a_size * b_mod_fee;

    if (f1 == f2) {
        return a->first.GetTx().GetHash() < b->first.GetTx().GetHash();
    }
    return f1 > f2;
}
bool CompareTxMemPoolEntryByDescendantScore::operator()(const mempool_storage_iterator& a, const mempool_storage_iterator& b) const
{
    double a_mod_fee, a_size, b_mod_fee, b_size;

    GetModFeeAndSize(a->first, a_mod_fee, a_size);
    GetModFeeAndSize(b->first, b_mod_fee, b_size);

    // Avoid division by rewriting (a/b > c/d) as (a*d > c*b).
    double f1 = a_mod_fee * b_size;
    double f2 = a_size * b_mod_fee;

    if (f1 == f2) {
        return a->first.GetTime() >= b->first.GetTime();
    }
    return f1 < f2;
}

void CTxMemPool::ModifyDescendantState(txiter iter, int32_t modifySize, CAmount modifyFee, int64_t modifyCount)
{

    auto fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_fee_rate);
    auto entry_time_nh = iters_by_fee_rate.extract(iter->second->iter_by_entry_time);
    auto ancestor_fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_ancestor_fee_rate);

    auto txid_it = iters_by_txid.find(iter->first.GetTx().GetHash());
    auto wtxid_it = iters_by_wtxid.find(iter->first.GetTx().GetWitnessHash());
    auto nh = mapTx.extract(iter);
    nh.key().UpdateDescendantState(modifySize, modifyFee, modifyCount);
    auto insert_result = mapTx.insert(std::move(nh));
    assert(insert_result.inserted);
    auto inserted = insert_result.position;

    txid_it->second = inserted;
    wtxid_it->second = inserted;
    fee_rate_nh.value() = inserted;
    entry_time_nh.value() = inserted;
    ancestor_fee_rate_nh.value() = inserted;

    inserted->second->iter_by_fee_rate = iters_by_fee_rate.insert(std::move(fee_rate_nh)).position;
    inserted->second->iter_by_entry_time = iters_by_entry_time.insert(std::move(entry_time_nh)).position;
    inserted->second->iter_by_ancestor_fee_rate = iters_by_ancestor_fee_rate.insert(std::move(ancestor_fee_rate_nh)).position;
}

void CTxMemPool::ModifyAncestorState(txiter iter, int32_t modifySize, CAmount modifyFee, int64_t modifyCount, int64_t modifySigOps)
{

    auto fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_fee_rate);
    assert(fee_rate_nh);
    auto entry_time_nh = iters_by_fee_rate.extract(iter->second->iter_by_entry_time);
    assert(fee_rate_nh);
    auto ancestor_fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_ancestor_fee_rate);
    assert(fee_rate_nh);

    auto txid_it = iters_by_txid.find(iter->first.GetTx().GetHash());
    assert(txid_it != iters_by_txid.end());
    auto wtxid_it = iters_by_wtxid.find(iter->first.GetTx().GetWitnessHash());
    assert(wtxid_it != iters_by_wtxid.end());

    auto nh = mapTx.extract(iter);
    nh.key().UpdateAncestorState(modifySize, modifyFee, modifyCount, modifySigOps);
    auto insert_result = mapTx.insert(std::move(nh));
    assert(insert_result.inserted);
    auto inserted = insert_result.position;

    txid_it->second = inserted;
    wtxid_it->second = inserted;
    fee_rate_nh.value() = inserted;
    entry_time_nh.value() = inserted;
    ancestor_fee_rate_nh.value() = inserted;

    inserted->second->iter_by_fee_rate = iters_by_fee_rate.insert(std::move(fee_rate_nh)).position;
    inserted->second->iter_by_entry_time = iters_by_entry_time.insert(std::move(entry_time_nh)).position;
    inserted->second->iter_by_ancestor_fee_rate = iters_by_ancestor_fee_rate.insert(std::move(ancestor_fee_rate_nh)).position;

}


void CTxMemPool::ModifyFee(txiter iter, CAmount fee_diff)
{

    auto fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_fee_rate);
    auto entry_time_nh = iters_by_fee_rate.extract(iter->second->iter_by_entry_time);
    auto ancestor_fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_ancestor_fee_rate);

    auto txid_it = iters_by_txid.find(iter->first.GetTx().GetHash());
    auto wtxid_it = iters_by_wtxid.find(iter->first.GetTx().GetWitnessHash());
    auto nh = mapTx.extract(iter);
    nh.key().UpdateModifiedFee(fee_diff);

    auto inserted = mapTx.insert(std::move(nh)).position;

    txid_it->second = inserted;
    wtxid_it->second = inserted;
    fee_rate_nh.value() = inserted;
    entry_time_nh.value() = inserted;
    ancestor_fee_rate_nh.value() = inserted;

    inserted->second->iter_by_fee_rate = iters_by_fee_rate.insert(std::move(fee_rate_nh)).position;
    inserted->second->iter_by_entry_time = iters_by_entry_time.insert(std::move(entry_time_nh)).position;
    inserted->second->iter_by_ancestor_fee_rate = iters_by_ancestor_fee_rate.insert(std::move(ancestor_fee_rate_nh)).position;

}

bool CTxMemPool::visited(const txiter it) const
{
    return m_epoch.visited(it->first.m_epoch_marker);
}

void CTxMemPool::UpdateLockPoints(txiter iter, const LockPoints& lp)
{

    auto fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_fee_rate);
    auto entry_time_nh = iters_by_fee_rate.extract(iter->second->iter_by_entry_time);
    auto ancestor_fee_rate_nh = iters_by_fee_rate.extract(iter->second->iter_by_ancestor_fee_rate);

    auto txid_it = iters_by_txid.find(iter->first.GetTx().GetHash());
    auto wtxid_it = iters_by_wtxid.find(iter->first.GetTx().GetWitnessHash());
    auto nh = mapTx.extract(iter);
    assert(nh);
    nh.key().UpdateLockPoints(lp);

    auto inserted_result = mapTx.insert(std::move(nh));
    auto inserted = inserted_result.position;
    assert(inserted_result.inserted);

    txid_it->second = inserted;
    wtxid_it->second = inserted;
    fee_rate_nh.value() = inserted;
    entry_time_nh.value() = inserted;
    ancestor_fee_rate_nh.value() = inserted;

    inserted->second->iter_by_fee_rate = iters_by_fee_rate.insert(std::move(fee_rate_nh)).position;
    inserted->second->iter_by_entry_time = iters_by_entry_time.insert(std::move(entry_time_nh)).position;
    inserted->second->iter_by_ancestor_fee_rate = iters_by_ancestor_fee_rate.insert(std::move(ancestor_fee_rate_nh)).position;

}

void CTxMemPool::UpdateForDescendants(txiter updateIt, cacheMap& cachedDescendants,
                                      const std::set<uint256>& setExclude, std::set<uint256>& descendants_to_remove)
{
    CTxMemPoolEntry::Children stageEntries, descendants;
    stageEntries = updateIt->first.GetMemPoolChildrenConst();

    while (!stageEntries.empty()) {
        const CTxMemPoolEntry& descendant = *stageEntries.begin();
        descendants.insert(descendant);
        stageEntries.erase(descendant);
        const CTxMemPoolEntry::Children& children = descendant.GetMemPoolChildrenConst();
        for (const CTxMemPoolEntry& childEntry : children) {
            cacheMap::iterator cacheIt = cachedDescendants.find(mapTx.find(childEntry));
            if (cacheIt != cachedDescendants.end()) {
                // We've already calculated this one, just add the entries for this set
                // but don't traverse again.
                for (txiter cacheEntry : cacheIt->second) {
                    descendants.insert(cacheEntry->first);
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
            auto descendant_iter = mapTx.find(descendant);
            cachedDescendants[updateIt].insert(descendant_iter);
            ModifyAncestorState(descendant_iter, updateIt->first.GetTxSize(), updateIt->first.GetModifiedFee(), 1, updateIt->first.GetSigOpCost());
            // Update ancestor state for each descendant
            if (descendant.GetCountWithAncestors() > uint64_t(m_limits.ancestor_count) || descendant.GetSizeWithAncestors() > m_limits.ancestor_size_vbytes) {
                descendants_to_remove.insert(descendant.GetTx().GetHash());
            }
        }
    }
    ModifyDescendantState(updateIt, modifySize, modifyFee, modifyCount);
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
        auto it = iters_by_txid.find(hash);
        if (it == iters_by_txid.end()) {
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
                auto childIter = iters_by_txid.find(childHash);
                assert(childIter != iters_by_txid.end());
                // We can skip updating entries we've encountered before or that
                // are in the block (which are already accounted for).
                if (!visited(childIter->second) && !setAlreadyIncluded.count(childHash)) {
                    UpdateChild(it->second, childIter->second, true);
                    UpdateParent(childIter->second, it->second, true);
                }
            }
        } // release epoch guard for UpdateForDescendants
        UpdateForDescendants(it->second, mapMemPoolDescendantsToUpdate, setAlreadyIncluded, descendants_to_remove);
    }

    for (const auto& txid : descendants_to_remove) {
        // This txid may have been removed already in a prior call to removeRecursive.
        // Therefore we ensure it is not yet removed already.
        if (const std::optional<txiter> txiter = GetIter(txid)) {
            removeRecursive((*txiter)->first.GetTx(), MemPoolRemovalReason::SIZELIMIT);
        }
    }
}

util::Result<CTxMemPool::setEntries> CTxMemPool::CalculateAncestorsAndCheckLimits(
    int64_t entry_size,
    size_t entry_count,
    CTxMemPoolEntry::Parents& staged_ancestors,
    const Limits& limits) const
{
    int64_t totalSizeWithAncestors = entry_size;
    setEntries ancestors;

    while (!staged_ancestors.empty()) {
        const CTxMemPoolEntry& stage = staged_ancestors.begin()->get();
        txiter stageit = mapTx.find(stage);

        ancestors.insert(stageit);
        staged_ancestors.erase(stage);
        totalSizeWithAncestors += stageit->first.GetTxSize();

        if (stageit->first.GetSizeWithDescendants() + entry_size > limits.descendant_size_vbytes) {
            return util::Error{Untranslated(strprintf("exceeds descendant size limit for tx %s [limit: %u]", stageit->first.GetTx().GetHash().ToString(), limits.descendant_size_vbytes))};
        } else if (stageit->first.GetCountWithDescendants() + entry_count > static_cast<uint64_t>(limits.descendant_count)) {
            return util::Error{Untranslated(strprintf("too many descendants for tx %s [limit: %u]", stageit->first.GetTx().GetHash().ToString(), limits.descendant_count))};
        } else if (totalSizeWithAncestors > limits.ancestor_size_vbytes) {
            return util::Error{Untranslated(strprintf("exceeds ancestor size limit [limit: %u]", limits.ancestor_size_vbytes))};
        }

        const CTxMemPoolEntry::Parents& parents = stageit->first.GetMemPoolParentsConst();
        for (const CTxMemPoolEntry& parent : parents) {
            txiter parent_it = mapTx.find(parent);

            // If this is a new ancestor, add it.
            if (ancestors.count(parent_it) == 0) {
                staged_ancestors.insert(parent);
            }
            if (staged_ancestors.size() + ancestors.size() + entry_count > static_cast<uint64_t>(limits.ancestor_count)) {
                return util::Error{Untranslated(strprintf("too many unconfirmed ancestors [limit: %u]", limits.ancestor_count))};
            }
        }
    }

    return ancestors;
}

bool CTxMemPool::CheckPackageLimits(const Package& package,
                                    const int64_t total_vsize,
                                    std::string &errString) const
{
    size_t pack_count = package.size();

    // Package itself is busting mempool limits; should be rejected even if no staged_ancestors exist
    if (pack_count > static_cast<uint64_t>(m_limits.ancestor_count)) {
        errString = strprintf("package count %u exceeds ancestor count limit [limit: %u]", pack_count, m_limits.ancestor_count);
        return false;
    } else if (pack_count > static_cast<uint64_t>(m_limits.descendant_count)) {
        errString = strprintf("package count %u exceeds descendant count limit [limit: %u]", pack_count, m_limits.descendant_count);
        return false;
    } else if (total_vsize > m_limits.ancestor_size_vbytes) {
        errString = strprintf("package size %u exceeds ancestor size limit [limit: %u]", total_vsize, m_limits.ancestor_size_vbytes);
        return false;
    } else if (total_vsize > m_limits.descendant_size_vbytes) {
        errString = strprintf("package size %u exceeds descendant size limit [limit: %u]", total_vsize, m_limits.descendant_size_vbytes);
        return false;
    }

    CTxMemPoolEntry::Parents staged_ancestors;
    for (const auto& tx : package) {
        for (const auto& input : tx->vin) {
            std::optional<txiter> piter = GetIter(input.prevout.hash);
            if (piter) {
                staged_ancestors.insert((*piter)->first);
                if (staged_ancestors.size() + package.size() > static_cast<uint64_t>(m_limits.ancestor_count)) {
                    errString = strprintf("too many unconfirmed parents [limit: %u]", m_limits.ancestor_count);
                    return false;
                }
            }
        }
    }
    // When multiple transactions are passed in, the ancestors and descendants of all transactions
    // considered together must be within limits even if they are not interdependent. This may be
    // stricter than the limits for each individual transaction.
    const auto ancestors{CalculateAncestorsAndCheckLimits(total_vsize, package.size(),
                                                          staged_ancestors, m_limits)};
    // It's possible to overestimate the ancestor/descendant totals.
    if (!ancestors.has_value()) errString = "possibly " + util::ErrorString(ancestors).original;
    return ancestors.has_value();
}

util::Result<CTxMemPool::setEntries> CTxMemPool::CalculateMemPoolAncestors(
    const CTxMemPoolEntry &entry,
    const Limits& limits,
    bool fSearchForParents /* = true */) const
{
    CTxMemPoolEntry::Parents staged_ancestors;
    const CTransaction &tx = entry.GetTx();

    if (fSearchForParents) {
        // Get parents of this transaction that are in the mempool
        // GetMemPoolParents() is only valid for entries in the mempool, so we
        // iterate mapTx to find parents.
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            std::optional<txiter> piter = GetIter(tx.vin[i].prevout.hash);
            if (piter) {
                staged_ancestors.insert((*piter)->first);
                if (staged_ancestors.size() + 1 > static_cast<uint64_t>(limits.ancestor_count)) {
                    return util::Error{Untranslated(strprintf("too many unconfirmed parents [limit: %u]", limits.ancestor_count))};
                }
            }
        }
    } else {
        // If we're not searching for parents, we require this to already be an
        // entry in the mempool and use the entry's cached parents.
        txiter it = mapTx.find(entry);
        staged_ancestors = it->first.GetMemPoolParentsConst();
    }

    return CalculateAncestorsAndCheckLimits(entry.GetTxSize(), /*entry_count=*/1, staged_ancestors,
                                            limits);
}

CTxMemPool::setEntries CTxMemPool::AssumeCalculateMemPoolAncestors(
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
    return std::move(result).value_or(CTxMemPool::setEntries{});
}

void CTxMemPool::UpdateAncestorsOf(bool add, txiter it, setEntries &setAncestors)
{
    const CTxMemPoolEntry::Parents& parents = it->first.GetMemPoolParentsConst();
    // add or remove this tx as a child of each parent
    for (const CTxMemPoolEntry& parent : parents) {
        UpdateChild(mapTx.find(parent), it, add);
    }
    const int32_t updateCount = (add ? 1 : -1);
    const int32_t updateSize{updateCount * it->first.GetTxSize()};
    const CAmount updateFee = updateCount * it->first.GetModifiedFee();
    for (txiter ancestorIt : setAncestors) {
        ModifyDescendantState(ancestorIt, updateSize, updateFee, updateCount);
    }
}

void CTxMemPool::UpdateEntryForAncestors(txiter it, const setEntries &setAncestors)
{
    int64_t updateCount = setAncestors.size();
    int64_t updateSize = 0;
    CAmount updateFee = 0;
    int64_t updateSigOpsCost = 0;
    for (txiter ancestorIt : setAncestors) {
        updateSize += ancestorIt->first.GetTxSize();
        updateFee += ancestorIt->first.GetModifiedFee();
        updateSigOpsCost += ancestorIt->first.GetSigOpCost();
    }
    assert(it->second);
    ModifyAncestorState(it, updateSize, updateFee, updateCount, updateSigOpsCost);
}

void CTxMemPool::UpdateChildrenForRemoval(txiter it)
{
    const CTxMemPoolEntry::Children& children = it->first.GetMemPoolChildrenConst();
    for (const CTxMemPoolEntry& updateIt : children) {
        UpdateParent(mapTx.find(updateIt), it, false);
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
        for (txiter removeIt : entriesToRemove) {
            setEntries setDescendants;
            CalculateDescendants(removeIt, setDescendants);
            setDescendants.erase(removeIt); // don't update state for self
            int32_t modifySize = -removeIt->first.GetTxSize();
            CAmount modifyFee = -removeIt->first.GetModifiedFee();
            int modifySigOps = -removeIt->first.GetSigOpCost();
            for (txiter dit : setDescendants) {
                ModifyAncestorState(dit, modifySize, modifyFee, -1, modifySigOps);
            }
        }
    }
    for (txiter removeIt : entriesToRemove) {
        const CTxMemPoolEntry &entry = removeIt->first;
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
        UpdateAncestorsOf(false, removeIt, ancestors);
    }
    // After updating all the ancestor sizes, we can now sever the link between each
    // transaction being removed and any mempool children (ie, update CTxMemPoolEntry::m_parents
    // for each direct child of a transaction being removed).
    for (txiter removeIt : entriesToRemove) {
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

    auto emplaced = mapTx.emplace(entry, std::make_unique<mempool_iters>());
    auto newit = emplaced.first;
    assert(emplaced.second);
    const auto& inserted = newit->second;
    assert(inserted);
    iters_by_txid.emplace(entry.GetTx().GetHash(), newit);
    iters_by_wtxid.emplace(entry.GetTx().GetWitnessHash(), newit);
    inserted->iter_by_fee_rate = iters_by_fee_rate.insert(newit).first;
    inserted->iter_by_entry_time = iters_by_entry_time.insert(newit).first;
    inserted->iter_by_ancestor_fee_rate = iters_by_ancestor_fee_rate.insert(newit).first;


    // Update transaction for any feeDelta created by PrioritiseTransaction
    CAmount delta{0};
    ApplyDelta(entry.GetTx().GetHash(), delta);
    // The following call to UpdateModifiedFee assumes no previous fee modifications
    Assume(entry.GetFee() == entry.GetModifiedFee());
    if (delta) {
        ModifyFee(newit, delta);
    }

    // Update cachedInnerUsage to include contained transaction's usage.
    // (When we update the entry for in-mempool parents, memory usage will be
    // further updated.)
    cachedInnerUsage += entry.DynamicMemoryUsage();

    const CTransaction& tx = newit->first.GetTx();
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
    for (const auto& pit : GetIterSet(setParentTransactions)) {
            UpdateParent(newit, pit, true);
    }
    UpdateAncestorsOf(true, newit, setAncestors);
    UpdateEntryForAncestors(newit, setAncestors);

    nTransactionsUpdated++;
    totalTxSize += entry.GetTxSize();
    m_total_fee += entry.GetFee();
    if (minerPolicyEstimator) {
        minerPolicyEstimator->processTransaction(entry, validFeeEstimate);
    }

    vTxHashes.emplace_back(tx.GetWitnessHash(), newit);
    newit->first.vTxHashesIdx = vTxHashes.size() - 1;

    TRACE3(mempool, added,
        entry.GetTx().GetHash().data(),
        entry.GetTxSize(),
        entry.GetFee()
    );
}

void CTxMemPool::removeUnchecked(txiter it, MemPoolRemovalReason reason)
{
    // We increment mempool sequence value no matter removal reason
    // even if not directly reported below.
    uint64_t mempool_sequence = GetAndIncrementSequence();

    if (reason != MemPoolRemovalReason::BLOCK) {
        // Notify clients that a transaction has been removed from the mempool
        // for any reason except being included in a block. Clients interested
        // in transactions included in blocks can subscribe to the BlockConnected
        // notification.
        GetMainSignals().TransactionRemovedFromMempool(it->first.GetSharedTx(), reason, mempool_sequence);
    }
    TRACE5(mempool, removed,
        it->first.GetTx().GetHash().data(),
        RemovalReasonToString(reason).c_str(),
        it->first.GetTxSize(),
        it->first.GetFee(),
        std::chrono::duration_cast<std::chrono::duration<std::uint64_t>>(it->first.GetTime()).count()
    );

    const uint256 hash = it->first.GetTx().GetHash();
    for (const CTxIn& txin : it->first.GetTx().vin)
        mapNextTx.erase(txin.prevout);

    RemoveUnbroadcastTx(hash, true /* add logging because unchecked */ );

    if (vTxHashes.size() > 1) {
        vTxHashes[it->first.vTxHashesIdx] = std::move(vTxHashes.back());
        vTxHashes[it->first.vTxHashesIdx].second->first.vTxHashesIdx = it->first.vTxHashesIdx;
        vTxHashes.pop_back();
        if (vTxHashes.size() * 2 < vTxHashes.capacity())
            vTxHashes.shrink_to_fit();
    } else
        vTxHashes.clear();

    iters_by_txid.erase(hash);
    iters_by_wtxid.erase(hash);
    iters_by_fee_rate.erase(it->second->iter_by_fee_rate);
    iters_by_entry_time.erase(it->second->iter_by_entry_time);
    iters_by_ancestor_fee_rate.erase(it->second->iter_by_ancestor_fee_rate);

    totalTxSize -= it->first.GetTxSize();
    m_total_fee -= it->first.GetFee();
    cachedInnerUsage -= it->first.DynamicMemoryUsage();
    cachedInnerUsage -= memusage::DynamicUsage(it->first.GetMemPoolParentsConst()) + memusage::DynamicUsage(it->first.GetMemPoolChildrenConst());
    mapTx.erase(it);
    nTransactionsUpdated++;
    if (minerPolicyEstimator) {minerPolicyEstimator->removeTx(hash, false);}
}

// Calculates descendants of entry that are not already in setDescendants, and adds to
// setDescendants. Assumes entryit is already a tx in the mempool and CTxMemPoolEntry::m_children
// is correct for tx and all descendants.
// Also assumes that if an entry is in setDescendants already, then all
// in-mempool descendants of it are already in setDescendants as well, so that we
// can save time by not iterating over those entries.
void CTxMemPool::CalculateDescendants(txiter entryit, setEntries& setDescendants) const
{
    setEntries stage;
    if (setDescendants.count(entryit) == 0) {
        stage.insert(entryit);
    }
    // Traverse down the children of entry, only adding children that are not
    // accounted for in setDescendants already (because those children have either
    // already been walked, or will be walked in this iteration).
    while (!stage.empty()) {
        txiter it = *stage.begin();
        setDescendants.insert(it);
        stage.erase(it);

        const CTxMemPoolEntry::Children& children = it->first.GetMemPoolChildrenConst();
        for (const CTxMemPoolEntry& child : children) {
            txiter childiter = mapTx.find(child);
            if (!setDescendants.count(childiter)) {
                stage.insert(childiter);
            }
        }
    }
}

void CTxMemPool::removeRecursive(const CTransaction &origTx, MemPoolRemovalReason reason)
{
    // Remove transaction from memory pool
    AssertLockHeld(cs);
        setEntries txToRemove;
        auto origit = iters_by_txid.find(origTx.GetHash());
        if (origit != iters_by_txid.end()) {
            txToRemove.insert(origit->second);
        } else {
            // When recursively removing but origTx isn't in the mempool
            // be sure to remove any children that are in the pool. This can
            // happen during chain re-orgs if origTx isn't re-accepted into
            // the mempool for any reason.
            for (unsigned int i = 0; i < origTx.vout.size(); i++) {
                auto it = mapNextTx.find(COutPoint(origTx.GetHash(), i));
                if (it == mapNextTx.end())
                    continue;
                auto nextit = iters_by_txid.find(it->second->GetHash());
                assert(nextit != iters_by_txid.end());
                txToRemove.insert(nextit->second);
            }
        }
        setEntries setAllRemoves;
        for (txiter it : txToRemove) {
            CalculateDescendants(it, setAllRemoves);
        }

        RemoveStaged(setAllRemoves, false, reason);
}

void CTxMemPool::removeForReorg(CChain& chain, std::function<bool(txiter)> check_final_and_mature)
{
    // Remove transactions spending a coinbase which are now immature and no-longer-final transactions
    AssertLockHeld(cs);
    AssertLockHeld(::cs_main);

    setEntries txToRemove;
    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        if (check_final_and_mature(it)) txToRemove.insert(it);
    }
    setEntries setAllRemoves;
    for (txiter it : txToRemove) {
        CalculateDescendants(it, setAllRemoves);
    }
    RemoveStaged(setAllRemoves, false, MemPoolRemovalReason::REORG);
    for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
        assert(TestLockPointValidity(chain, it->first.GetLockPoints()));
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

        auto i = iters_by_txid.find(hash);
        if (i != iters_by_txid.end())
            entries.push_back(&i->second->first);
   }
    // Before the txs in the new block have been removed from the mempool, update policy estimates
    if (minerPolicyEstimator) {minerPolicyEstimator->processBlock(nBlockHeight, entries);}
    for (const auto& tx : vtx)
    {
        auto it = iters_by_txid.find(tx->GetHash());
        if (it != iters_by_txid.end()) {
            setEntries stage;
            stage.insert(it->second);
            RemoveStaged(stage, true, MemPoolRemovalReason::BLOCK);
        }
        removeConflicts(*tx);
        ClearPrioritisation(tx->GetHash());
    }
    lastRollingFeeUpdate = GetTime();
    blockSinceLastRollingFeeBump = true;
}

void CTxMemPool::check(const CCoinsViewCache& active_coins_tip, int64_t spendheight) const
{
    if (m_check_ratio == 0) return;

    if (GetRand(m_check_ratio) >= 1) return;

    AssertLockHeld(::cs_main);
    LOCK(cs);
    LogPrint(BCLog::MEMPOOL, "Checking mempool with %u transactions and %u inputs\n", (unsigned int)mapTx.size(), (unsigned int)mapNextTx.size());

    uint64_t checkTotal = 0;
    CAmount check_total_fee{0};
    uint64_t innerUsage = 0;
    uint64_t prev_ancestor_count{0};

    CCoinsViewCache mempoolDuplicate(const_cast<CCoinsViewCache*>(&active_coins_tip));

    for (const auto& it : GetSortedDepthAndScore()) {
        checkTotal += it->first.GetTxSize();
        check_total_fee += it->first.GetFee();
        innerUsage += it->first.DynamicMemoryUsage();
        const CTransaction& tx = it->first.GetTx();
        innerUsage += memusage::DynamicUsage(it->first.GetMemPoolParentsConst()) + memusage::DynamicUsage(it->first.GetMemPoolChildrenConst());
        CTxMemPoolEntry::Parents setParentCheck;
        for (const CTxIn &txin : tx.vin) {
            // Check that every mempool transaction's inputs refer to available coins, or other mempool tx's.
            auto it2 = iters_by_txid.find(tx.GetHash());
            if (it2 != iters_by_txid.end()) {

                const CTransaction& tx2 = it2->second->first.GetTx();
                assert(tx2.vout.size() > txin.prevout.n && !tx2.vout[txin.prevout.n].IsNull());
                setParentCheck.insert(it2->second->first);
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
        assert(setParentCheck.size() == it->first.GetMemPoolParentsConst().size());
        assert(std::equal(setParentCheck.begin(), setParentCheck.end(), it->first.GetMemPoolParentsConst().begin(), comp));
        // Verify ancestor state is correct.
        auto ancestors{AssumeCalculateMemPoolAncestors(__func__, it->first, Limits::NoLimits())};
        uint64_t nCountCheck = ancestors.size() + 1;
        int32_t nSizeCheck = it->first.GetTxSize();
        CAmount nFeesCheck = it->first.GetModifiedFee();
        int64_t nSigOpCheck = it->first.GetSigOpCost();

        for (txiter ancestorIt : ancestors) {
            nSizeCheck += ancestorIt->first.GetTxSize();
            nFeesCheck += ancestorIt->first.GetModifiedFee();
            nSigOpCheck += ancestorIt->first.GetSigOpCost();
        }

        assert(it->first.GetCountWithAncestors() == nCountCheck);
        assert(it->first.GetSizeWithAncestors() == nSizeCheck);
        assert(it->first.GetSigOpCostWithAncestors() == nSigOpCheck);
        assert(it->first.GetModFeesWithAncestors() == nFeesCheck);
        // Sanity check: we are walking in ascending ancestor count order.
        assert(prev_ancestor_count <= it->first.GetCountWithAncestors());
        prev_ancestor_count = it->first.GetCountWithAncestors();

        // Check children against mapNextTx
        CTxMemPoolEntry::Children setChildrenCheck;
        auto iter = mapNextTx.lower_bound(COutPoint(it->first.GetTx().GetHash(), 0));
        int32_t child_sizes{0};
        for (; iter != mapNextTx.end() && iter->first->hash == it->first.GetTx().GetHash(); ++iter) {
            auto childit = iters_by_txid.find(iter->second->GetHash());
            assert(childit != iters_by_txid.end()); // mapNextTx points to in-mempool transactions
            if (setChildrenCheck.insert(childit->second->first).second) {
                child_sizes += childit->second->first.GetTxSize();
            }
        }
        assert(setChildrenCheck.size() == it->first.GetMemPoolChildrenConst().size());
        assert(std::equal(setChildrenCheck.begin(), setChildrenCheck.end(), it->first.GetMemPoolChildrenConst().begin(), comp));
        // Also check to make sure size is greater than sum with immediate children.
        // just a sanity check, not definitive that this calc is correct...
        assert(it->first.GetSizeWithDescendants() >= child_sizes + it->first.GetTxSize());

        TxValidationState dummy_state; // Not used. CheckTxInputs() should always pass
        CAmount txfee = 0;
        assert(!tx.IsCoinBase());
        assert(Consensus::CheckTxInputs(tx, dummy_state, mempoolDuplicate, spendheight, txfee));
        for (const auto& input: tx.vin) mempoolDuplicate.SpendCoin(input.prevout);
        AddCoins(mempoolDuplicate, tx, std::numeric_limits<int>::max());
    }
    for (auto it = mapNextTx.cbegin(); it != mapNextTx.cend(); it++) {
        uint256 hash = it->second->GetHash();
        auto it2 = iters_by_txid.find(hash);
        const CTransaction& tx = it2->second->first.GetTx();
        assert(it2 != iters_by_txid.end());
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
    if(wtxid) {
        auto j = iters_by_wtxid.find(hashb);
        if (j == iters_by_wtxid.end()) return false;
        auto i = iters_by_wtxid.find(hasha);
        if (i == iters_by_wtxid.end()) return true;
        uint64_t counta = i->second->first.GetCountWithAncestors();
        uint64_t countb = j->second->first.GetCountWithAncestors();
        if (counta == countb) {
            return CompareTxMemPoolEntryByScore()(i->second->first, j->second->first);
        }
        return counta < countb;
    }

    auto j = iters_by_txid.find(hashb);
    if (j == iters_by_txid.end()) return false;
    auto i = iters_by_txid.find(hasha);
    if (i == iters_by_txid.end()) return true;
    uint64_t counta = i->second->first.GetCountWithAncestors();
    uint64_t countb = j->second->first.GetCountWithAncestors();
    if (counta == countb) {
        return CompareTxMemPoolEntryByScore()(i->second->first, j->second->first);
    }
    return counta < countb;
}

namespace {
class DepthAndScoreComparator
{
public:
    bool operator()(const CTxMemPool::indexed_transaction_set::const_iterator& a, const CTxMemPool::indexed_transaction_set::const_iterator& b)
    {
        uint64_t counta = a->first.GetCountWithAncestors();
        uint64_t countb = b->first.GetCountWithAncestors();
        if (counta == countb) {
            return CompareTxMemPoolEntryByScore()(a->first, b->first);
        }
        return counta < countb;
    }
};
} // namespace

std::vector<CTxMemPool::indexed_transaction_set::const_iterator> CTxMemPool::GetSortedDepthAndScore() const
{
    std::vector<indexed_transaction_set::const_iterator> iters;
    AssertLockHeld(cs);

    iters.reserve(mapTx.size());

    for (auto mi = mapTx.begin(); mi != mapTx.end(); ++mi) {
        iters.push_back(mi);
    }
    std::sort(iters.begin(), iters.end(), DepthAndScoreComparator());
    return iters;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid) const
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore();

    vtxid.clear();
    vtxid.reserve(mapTx.size());

    for (auto it : iters) {
        vtxid.push_back(it->first.GetTx().GetHash());
    }
}

static TxMempoolInfo GetInfo(CTxMemPool::indexed_transaction_set::const_iterator it) {
    return TxMempoolInfo{it->first.GetSharedTx(), it->first.GetTime(), it->first.GetFee(), it->first.GetTxSize(), it->first.GetModifiedFee() - it->first.GetFee()};
}

std::vector<TxMempoolInfo> CTxMemPool::infoAll() const
{
    LOCK(cs);
    auto iters = GetSortedDepthAndScore();

    std::vector<TxMempoolInfo> ret;
    ret.reserve(mapTx.size());
    for (auto it : iters) {
        ret.push_back(GetInfo(it));
    }

    return ret;
}

CTransactionRef CTxMemPool::get(const uint256& hash) const
{
    LOCK(cs);
    auto i = iters_by_txid.find(hash);
    if (i == iters_by_txid.end())
        return nullptr;
    return i->second->first.GetSharedTx();
}

TxMempoolInfo CTxMemPool::info(const GenTxid& gtxid) const
{
    LOCK(cs);
    if (gtxid.IsWtxid()) {
        auto i = iters_by_wtxid.find(gtxid.GetHash());
        if (i != iters_by_wtxid.end())
            return GetInfo(i->second);
    } else {
        auto i = iters_by_txid.find(gtxid.GetHash());
        if (i != iters_by_txid.end())
            return GetInfo(i->second);
    }
    return TxMempoolInfo();
}

TxMempoolInfo CTxMemPool::info_for_relay(const GenTxid& gtxid, uint64_t last_sequence) const
{
    LOCK(cs);
    if (gtxid.IsWtxid()) {
        auto i = iters_by_wtxid.find(gtxid.GetHash());
        if (i != iters_by_wtxid.end() && i->second->first.GetSequence() < last_sequence)
            return GetInfo(i->second);
    } else {
        auto i = iters_by_txid.find(gtxid.GetHash());
        if (i != iters_by_txid.end() && i->second->first.GetSequence() < last_sequence)
            return GetInfo(i->second);
    }
    return TxMempoolInfo();
}

void CTxMemPool::PrioritiseTransaction(const uint256& hash, const CAmount& nFeeDelta)
{
    {
        LOCK(cs);
        CAmount &delta = mapDeltas[hash];
        delta = SaturatingAdd(delta, nFeeDelta);
        auto it = iters_by_txid.find(hash);
        if (it != iters_by_txid.end()) {
            ModifyFee(it->second, delta);
            // Now update all ancestors' modified fees with descendants
            auto ancestors{AssumeCalculateMemPoolAncestors(__func__, it->second->first, Limits::NoLimits(), /*fSearchForParents=*/false)};
            for (txiter ancestorIt : ancestors) {
                ModifyDescendantState(ancestorIt, 0, nFeeDelta, 0);
            }
            // Now update all descendants' modified fees with ancestors
            setEntries setDescendants;
            CalculateDescendants(it->second, setDescendants);
            setDescendants.erase(it->second);
            for (txiter descendantIt : setDescendants) {
                ModifyAncestorState(descendantIt, 0, nFeeDelta, 0, 0);
            }
            ++nTransactionsUpdated;
        }
        if (delta == 0) {
            mapDeltas.erase(hash);
            LogPrintf("PrioritiseTransaction: %s (%sin mempool) delta cleared\n", hash.ToString(), it == iters_by_txid.end() ? "not " : "");
        } else {
            LogPrintf("PrioritiseTransaction: %s (%sin mempool) fee += %s, new delta=%s\n",
                      hash.ToString(),
                      it == iters_by_txid.end() ? "not " : "",
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
        const auto iter = iters_by_txid.find(txid);
        const bool in_mempool{iter != iters_by_txid.end()};
        std::optional<CAmount> modified_fee;
        if (in_mempool) modified_fee = iter->second->first.GetModifiedFee();
        result.emplace_back(delta_info{in_mempool, delta, modified_fee, txid});
    }
    return result;
}

const CTransaction* CTxMemPool::GetConflictTx(const COutPoint& prevout) const
{
    const auto it = mapNextTx.find(prevout);
    return it == mapNextTx.end() ? nullptr : it->second;
}

std::optional<CTxMemPool::txiter> CTxMemPool::GetIter(const uint256& txid) const
{
    auto it = iters_by_txid.find(txid);
    if (it != iters_by_txid.end()) return it->second;
    return std::nullopt;
}

CTxMemPool::setEntries CTxMemPool::GetIterSet(const std::set<uint256>& hashes) const
{
    CTxMemPool::setEntries ret;
    for (const auto& h : hashes) {
        const auto mi = GetIter(h);
        if (mi) ret.insert(*mi);
    }
    return ret;
}

std::vector<CTxMemPool::txiter> CTxMemPool::GetIterVec(const std::vector<uint256>& txids) const
{
    AssertLockHeld(cs);
    std::vector<txiter> ret;
    ret.reserve(txids.size());
    for (const auto& txid : txids) {
        const auto it{GetIter(txid)};
        if (!it) return {};
        ret.push_back(*it);
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
            m_non_base_coins.emplace(outpoint);
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
        m_non_base_coins.emplace(COutPoint(tx->GetHash(), n));
    }
}
void CCoinsViewMemPool::Reset()
{
    m_temp_added.clear();
    m_non_base_coins.clear();
}

size_t CTxMemPool::DynamicMemoryUsage() const {
    LOCK(cs);
    // Estimate the overhead of mapTx to be 15 pointers + an allocation, as no exact formula for boost::multi_index_contained is implemented.
    return memusage::MallocUsage(sizeof(CTxMemPoolEntry) + 15 * sizeof(void*)) * mapTx.size() + memusage::DynamicUsage(mapNextTx) + memusage::DynamicUsage(mapDeltas) + memusage::DynamicUsage(vTxHashes) + cachedInnerUsage;
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
    for (txiter it : stage) {
        removeUnchecked(it, reason);
    }
}

int CTxMemPool::Expire(std::chrono::seconds time)
{
    AssertLockHeld(cs);
    auto it = iters_by_entry_time.begin();
    setEntries toremove;
    while (it != iters_by_entry_time.end() && *it != nullptr && (*it)->first.GetTime() < time) {
        toremove.insert(*it);
        it++;
    }
    setEntries stage;
    for (txiter removeit : toremove) {
        CalculateDescendants(removeit, stage);
    }
    RemoveStaged(stage, false, MemPoolRemovalReason::EXPIRY);
    return stage.size();
}

void CTxMemPool::addUnchecked(const CTxMemPoolEntry &entry, bool validFeeEstimate)
{
    auto ancestors{AssumeCalculateMemPoolAncestors(__func__, entry, Limits::NoLimits())};
    return addUnchecked(entry, ancestors, validFeeEstimate);
}

void CTxMemPool::UpdateChild(txiter entry, txiter child, bool add)
{
    AssertLockHeld(cs);
    CTxMemPoolEntry::Children s;
    if (add && entry->first.GetMemPoolChildren().insert(child->first).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && entry->first.GetMemPoolChildren().erase(child->first)) {
        cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
    }
}

void CTxMemPool::UpdateParent(txiter entry, txiter parent, bool add)
{
    AssertLockHeld(cs);
    CTxMemPoolEntry::Parents s;
    if (add && entry->first.GetMemPoolParents().insert(parent->first).second) {
        cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
    } else if (!add && entry->first.GetMemPoolParents().erase(parent->first)) {
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
    while (!mapTx.empty() && DynamicMemoryUsage() > sizelimit) {
        auto it = iters_by_fee_rate.begin();

        // We set the new mempool min fee to the feerate of the removed set, plus the
        // "minimum reasonable fee rate" (ie some value under which we consider txn
        // to have 0 fee). This way, we don't allow txn to enter mempool with feerate
        // equal to txn which were removed with no block in between.
        CFeeRate removed((*it)->first.GetModFeesWithDescendants(), (*it)->first.GetSizeWithDescendants());
        removed += m_incremental_relay_feerate;
        trackPackageRemoved(removed);
        maxFeeRateRemoved = std::max(maxFeeRateRemoved, removed);

        setEntries stage;
        CalculateDescendants(*it, stage);
        nTxnRemoved += stage.size();

        std::vector<CTransaction> txn;
        if (pvNoSpendsRemaining) {
            txn.reserve(stage.size());
            for (txiter iter : stage)
                txn.push_back(iter->first.GetTx());
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

uint64_t CTxMemPool::CalculateDescendantMaximum(txiter entry) const {
    // find parent with highest descendant count
    std::vector<txiter> candidates;
    setEntries counted;
    candidates.push_back(entry);
    uint64_t maximum = 0;
    while (candidates.size()) {
        txiter candidate = candidates.back();
        candidates.pop_back();
        if (!counted.insert(candidate).second) continue;
        const CTxMemPoolEntry::Parents& parents = candidate->first.GetMemPoolParentsConst();
        if (parents.size() == 0) {
            maximum = std::max(maximum, candidate->first.GetCountWithDescendants());
        } else {
            for (const CTxMemPoolEntry& i : parents) {
                candidates.push_back(mapTx.find(i));
            }
        }
    }
    return maximum;
}

void CTxMemPool::GetTransactionAncestry(const uint256& txid, size_t& ancestors, size_t& descendants, size_t* const ancestorsize, CAmount* const ancestorfees) const {
    LOCK(cs);
    auto it = iters_by_txid.find(txid);
    ancestors = descendants = 0;
    if (it != iters_by_txid.end()) {
        ancestors = it->second->first.GetCountWithAncestors();
        if (ancestorsize) *ancestorsize = it->second->first.GetSizeWithAncestors();
        if (ancestorfees) *ancestorfees = it->second->first.GetModFeesWithAncestors();
        descendants = CalculateDescendantMaximum(it->second);
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

std::vector<CTxMemPool::txiter> CTxMemPool::GatherClusters(const std::vector<uint256>& txids) const
{
    AssertLockHeld(cs);
    std::vector<txiter> clustered_txs{GetIterVec(txids)};
    // Use epoch: visiting an entry means we have added it to the clustered_txs vector. It does not
    // necessarily mean the entry has been processed.
    WITH_FRESH_EPOCH(m_epoch);
    for (const auto& it : clustered_txs) {
        visited(it);
    }
    // i = index of where the list of entries to process starts
    for (size_t i{0}; i < clustered_txs.size(); ++i) {
        // DoS protection: if there are 500 or more entries to process, just quit.
        if (clustered_txs.size() > 500) return {};
        const txiter& tx_iter = clustered_txs.at(i);
        for (const auto& entries : {tx_iter->first.GetMemPoolParentsConst(), tx_iter->first.GetMemPoolChildrenConst()}) {
            for (const CTxMemPoolEntry& entry : entries) {
                const auto entry_it = mapTx.find(entry);
                if (!visited(entry_it)) {
                    clustered_txs.push_back(entry_it);
                }
            }
        }
    }
    return clustered_txs;
}
