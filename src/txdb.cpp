// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txdb.h>

#include <coins.h>
#include <dbwrapper.h>
#include <logging/timer.h>
#include <primitives/transaction.h>
#include <random.h>
#include <serialize.h>
#include <uint256.h>
#include <util/log.h>
#include <util/vector.h>

#include <cassert>
#include <cstdlib>
#include <iterator>
#include <utility>

static constexpr uint8_t DB_COIN{'C'};
static constexpr uint8_t DB_COINV2{'D'};
static constexpr uint8_t DB_BEST_BLOCK{'B'};
static constexpr uint8_t DB_HEAD_BLOCKS{'H'};
// Keys used in previous version that might still be found in the DB:
static constexpr uint8_t DB_COINS{'c'};

// Threshold for warning when writing this many dirty cache entries to disk.
static constexpr size_t WARN_FLUSH_COINS_COUNT{10'000'000};

bool CCoinsViewDB::NeedsUpgrade()
{
    std::unique_ptr<CDBIterator> cursor{m_db->NewIterator()};
    // DB_COINS was deprecated in v0.15.0, commit
    // 1088b02f0ccd7358d2b7076bb9e122d59d502d02
    cursor->Seek(std::make_pair(DB_COINS, uint256{}));
    return cursor->Valid();
}

namespace {

struct CoinEntry {
    COutPoint* outpoint;
    uint8_t key;
    explicit CoinEntry(const COutPoint* ptr, uint8_t coin_key) : outpoint(const_cast<COutPoint*>(ptr)), key{coin_key} {}

    SERIALIZE_METHODS(CoinEntry, obj) { READWRITE(obj.key, obj.outpoint->hash, VARINT(obj.outpoint->n)); }
};

} // namespace

CCoinsViewDB::CCoinsViewDB(DBParams db_params, CoinsViewOptions options) :
    m_db_params{std::move(db_params)},
    m_options{std::move(options)},
    m_db{std::make_unique<CDBWrapper>(m_db_params)} { }

void CCoinsViewDB::ResizeCache(size_t new_cache_size)
{
    // We can't do this operation with an in-memory DB since we'll lose all the coins upon
    // reset.
    if (!m_db_params.memory_only) {
        // Have to do a reset first to get the original `m_db` state to release its
        // filesystem lock.
        m_db.reset();
        m_db_params.cache_bytes = new_cache_size;
        m_db_params.wipe_data = false;
        m_db = std::make_unique<CDBWrapper>(m_db_params);
    }
}

std::optional<Coin> CCoinsViewDB::GetCoin(const COutPoint& outpoint) const
{
    uint8_t db_coin_key = m_options.use_v1 ? DB_COIN : DB_COINV2;
    Coin coin;
    ParamsWrapper wrap(m_options.use_v1 ? Coin::V1 : Coin::V2, coin);
    if (m_db->Read(CoinEntry(&outpoint, db_coin_key), wrap)) {
        Assert(!coin.IsSpent()); // The UTXO database should never contain spent coins
        return coin;
    }
    return std::nullopt;
}

bool CCoinsViewDB::HaveCoin(const COutPoint &outpoint) const {
    uint8_t db_coin_key = m_options.use_v1 ? DB_COIN : DB_COINV2;
    return m_db->Exists(CoinEntry(&outpoint, db_coin_key));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!m_db->Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

std::vector<uint256> CCoinsViewDB::GetHeadBlocks() const {
    std::vector<uint256> vhashHeadBlocks;
    if (!m_db->Read(DB_HEAD_BLOCKS, vhashHeadBlocks)) {
        return std::vector<uint256>();
    }
    return vhashHeadBlocks;
}

void CCoinsViewDB::BatchWrite(CoinsViewCacheCursor& cursor, const uint256& hashBlock)
{
    CDBBatch batch(*m_db);
    size_t count = 0;
    const size_t dirty_count{cursor.GetDirtyCount()};
    assert(!hashBlock.IsNull());

    uint256 old_tip = GetBestBlock();
    if (old_tip.IsNull()) {
        // We may be in the middle of replaying.
        std::vector<uint256> old_heads = GetHeadBlocks();
        if (old_heads.size() == 2) {
            if (old_heads[0] != hashBlock) {
                LogError("The coins database detected an inconsistent state, likely due to a previous crash or shutdown. You will need to restart bitcoind with the -reindex-chainstate or -reindex configuration option.\n");
            }
            assert(old_heads[0] == hashBlock);
            old_tip = old_heads[1];
        }
    }

    if (dirty_count > WARN_FLUSH_COINS_COUNT) LogWarning("Flushing large (%d entries) UTXO set to disk, it may take several minutes", dirty_count);
    LOG_TIME_MILLIS_WITH_CATEGORY(strprintf("write coins cache to disk (%d out of %d cached coins)",
        dirty_count, cursor.GetTotalCount()), BCLog::BENCH);

    // In the first batch, mark the database as being in the middle of a
    // transition from old_tip to hashBlock.
    // A vector is used for future extensibility, as we may want to support
    // interrupting after partial writes from multiple independent reorgs.
    batch.Erase(DB_BEST_BLOCK);
    batch.Write(DB_HEAD_BLOCKS, Vector(hashBlock, old_tip));

    uint8_t db_coin_key = m_options.use_v1 ? DB_COIN : DB_COINV2;
    for (auto it{cursor.Begin()}; it != cursor.End();) {
        if (it->second.IsDirty()) {
            CoinEntry entry(&it->first, db_coin_key);
            if (it->second.coin.IsSpent()) {
                batch.Erase(entry);
            } else {
                ParamsWrapper wrap(m_options.use_v1 ? Coin::V1 : Coin::V2, it->second.coin);
                batch.Write(entry, wrap);
            }
        }
        count++;
        it = cursor.NextAndMaybeErase(*it);
        if (batch.ApproximateSize() > m_options.batch_write_bytes) {
            LogDebug(BCLog::COINDB, "Writing partial batch of %.2f MiB\n", batch.ApproximateSize() * (1.0 / 1048576.0));

            m_db->WriteBatch(batch);
            batch.Clear();
            if (m_options.simulate_crash_ratio) {
                static FastRandomContext rng;
                if (rng.randrange(m_options.simulate_crash_ratio) == 0) {
                    LogError("Simulating a crash. Goodbye.");
                    _Exit(0);
                }
            }
        }
    }

    // In the last batch, mark the database as consistent with hashBlock again.
    batch.Erase(DB_HEAD_BLOCKS);
    batch.Write(DB_BEST_BLOCK, hashBlock);

    LogDebug(BCLog::COINDB, "Writing final batch of %.2f MiB\n", batch.ApproximateSize() * (1.0 / 1048576.0));
    m_db->WriteBatch(batch);
    LogDebug(BCLog::COINDB, "Committed %u changed transaction outputs (out of %u) to coin database...", (unsigned int)dirty_count, (unsigned int)count);
}

size_t CCoinsViewDB::EstimateSize() const
{
    uint8_t db_coin_key = m_options.use_v1 ? DB_COIN : DB_COINV2;
    return m_db->EstimateSize(db_coin_key, uint8_t(db_coin_key + 1));
}

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    // Prefer using CCoinsViewDB::Cursor() since we want to perform some
    // cache warmup on instantiation.
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256&hashBlockIn, bool use_v1):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn), m_use_v1{use_v1} {}
    ~CCoinsViewDBCursor() = default;

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;

    bool Valid() const override;
    void Next() override;

private:
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;

    friend class CCoinsViewDB;
    bool m_use_v1;
};

std::unique_ptr<CCoinsViewCursor> CCoinsViewDB::Cursor() const
{
    uint8_t db_coin_key = m_options.use_v1 ? DB_COIN : DB_COINV2;
    auto i = std::make_unique<CCoinsViewDBCursor>(
        const_cast<CDBWrapper&>(*m_db).NewIterator(), GetBestBlock(), m_options.use_v1);
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(db_coin_key);
    // Cache key of first record
    if (i->pcursor->Valid()) {
        CoinEntry entry(&i->keyTmp.second, db_coin_key);
        i->pcursor->GetKey(entry);
        i->keyTmp.first = entry.key;
    } else {
        i->keyTmp.first = 0; // Make sure Valid() and GetKey() return false
    }
    return i;
}

bool CCoinsViewDBCursor::GetKey(COutPoint &key) const
{
    // Return cached key
    uint8_t db_coin_key = m_use_v1 ? DB_COIN : DB_COINV2;
    if (keyTmp.first == db_coin_key) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(Coin &coin) const
{
    ParamsWrapper wrap(m_use_v1 ? Coin::V1 : Coin::V2, coin);
    return pcursor->GetValue(wrap);
}

bool CCoinsViewDBCursor::Valid() const
{
    uint8_t db_coin_key = m_use_v1 ? DB_COIN : DB_COINV2;
    return keyTmp.first == db_coin_key;
}

void CCoinsViewDBCursor::Next()
{
    uint8_t db_coin_key = m_use_v1 ? DB_COIN : DB_COINV2;
    pcursor->Next();
    CoinEntry entry(&keyTmp.second, db_coin_key);
    if (!pcursor->Valid() || !pcursor->GetKey(entry)) {
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
    } else {
        keyTmp.first = entry.key;
    }
}
