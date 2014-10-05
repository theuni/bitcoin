// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "coinsbyaddress.h"
#include "core.h"
#include "pow.h"
#include "uint256.h"
#include "script/compressor.h"
#include "ui_interface.h"

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

void static BatchWriteCoins(CLevelDBBatch &batch, const uint256 &hash, const CCoins &coins) {
    if (coins.IsPruned())
        batch.Erase(make_pair('c', hash));
    else
        batch.Write(make_pair('c', hash), coins);
}

void static BatchWriteCoins(CLevelDBBatch &batch, const CScript &script, const CCoinsByAddress &coins) {
    CScriptCompressor cscript(const_cast<CScript&>(script));
    if (coins.IsEmpty())
        batch.Erase(make_pair('d', cscript));
    else
        batch.Write(make_pair('d', cscript), coins);
}

void static BatchWriteHashBestChain(CLevelDBBatch &batch, const uint256 &hash) {
    batch.Write('B', hash);
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe) {
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    return db.Read(make_pair('c', txid), coins);
}

bool CCoinsViewDB::GetCoinsByAddress(CScript &script, CCoinsByAddress &coins) const {
    CScriptCompressor cscript(script);
    return db.Read(make_pair('d', cscript), coins);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    return db.Exists(make_pair('c', txid));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read('B', hashBestChain))
        return uint256(0);
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CLevelDBBatch batch;
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            BatchWriteCoins(batch, it->first, it->second.coins);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (fTxOutsByAddressIndex && pcoinsByAddress) // only if -txoutsbyaddressindex
    {
        for (CCoinsMapByAddress::iterator it = pcoinsByAddress->cacheCoinsByAddress.begin(); it != pcoinsByAddress->cacheCoinsByAddress.end();) {
            BatchWriteCoins(batch, it->first, it->second);
            CCoinsMapByAddress::iterator itOld = it++;
            pcoinsByAddress->cacheCoinsByAddress.erase(itOld);
        }
        pcoinsByAddress->cacheCoinsByAddress.clear();
    }
    if (hashBlock != uint256(0))
        BatchWriteHashBestChain(batch, hashBlock);

    LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.WriteBatch(batch);
}

bool CCoinsViewDB::WriteFlag(const std::string &name, bool fValue) {
    return db.Write(std::make_pair('F', name), fValue ? '1' : '0');
}

bool CCoinsViewDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!db.Read(std::make_pair('F', name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair('b', blockindex.GetBlockHash()), blockindex);
}

bool CBlockTreeDB::WriteBlockFileInfo(int nFile, const CBlockFileInfo &info) {
    return Write(make_pair('f', nFile), info);
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(make_pair('f', nFile), info);
}

bool CBlockTreeDB::WriteLastBlockFile(int nFile) {
    return Write('l', nFile);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write('R', '1');
    else
        return Erase('R');
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists('R');
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read('l', nFile);
}

bool CCoinsViewDB::GetStats(CCoinsStats &stats) const {
    int64_t nTotalCount = GetPrefixCount('c') + GetPrefixCount('d');
    uiInterface.ShowProgress(_("Calculating index stats..."), 0);

    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    leveldb::Iterator *pcursor = const_cast<CLevelDBWrapper*>(&db)->NewIterator();
    pcursor->SeekToFirst();

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = GetBestBlock();
    ss << stats.hashBlock;
    CAmount nTotalAmount = 0;
    int64_t progress = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;

            if (progress % 1000 == 0 && nTotalCount > 0)
                uiInterface.ShowProgress(_("Calculating index stats..."), std::max(1, std::min(99, (int)(((double)(progress)) / (double)nTotalCount * 100))));

            if (chType == 'c') {
                progress++;
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CCoins coins;
                ssValue >> coins;
                uint256 txhash;
                ssKey >> txhash;
                ss << txhash;
                ss << VARINT(coins.nVersion);
                ss << (coins.fCoinBase ? 'c' : 'n');
                ss << VARINT(coins.nHeight);
                stats.nTransactions++;
                for (unsigned int i=0; i<coins.vout.size(); i++) {
                    const CTxOut &out = coins.vout[i];
                    if (!out.IsNull()) {
                        stats.nTransactionOutputs++;
                        ss << VARINT(i+1);
                        ss << out;
                        nTotalAmount += out.nValue;
                    }
                }
                stats.nSerializedSize += 32 + slValue.size();
                ss << VARINT(0);
            }
            if (chType == 'd') {
                progress++;
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CCoinsByAddress coinsByAddress;
                ssValue >> coinsByAddress;
                stats.nAddresses++;
                stats.nAddressesOutputs += coinsByAddress.setCoins.size();
            }
            pcursor->Next();
        } catch (std::exception &e) {
            uiInterface.ShowProgress(_("Calculating index stats..."), 100);
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    if (mapBlockIndex.count(GetBestBlock()))
        stats.nHeight = mapBlockIndex.find(GetBestBlock())->second->nHeight;
    stats.hashSerialized = ss.GetHash();
    stats.nTotalAmount = nTotalAmount;
    delete pcursor;
    uiInterface.ShowProgress(_("Calculating index stats..."), 100);
    return true;
}
int64_t CCoinsViewDB::GetPrefixCount(char prefix) const
{
    leveldb::Iterator *pcursor = const_cast<CLevelDBWrapper*>(&db)->NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << prefix;
    pcursor->Seek(ssKeySet.str());

    int64_t i = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType != prefix)
                break;
            i++;
            pcursor->Next();
        } catch (std::exception &e) {
            return 0;
        }
    }
    delete pcursor;
    return i;
}

bool CCoinsViewDB::DeleteAllCoinsByAddress()
{
    leveldb::Iterator *pcursor = const_cast<CLevelDBWrapper*>(&db)->NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << 'd';
    pcursor->Seek(ssKeySet.str());

    std::vector<std::string> v;
    int64_t i = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType != 'd')
                break;

            v.push_back(slKey.ToString());
            if (v.size() >= 10000)
            {
                i += v.size();
                leveldb::WriteBatch batch;
                BOOST_FOREACH(const std::string& slice, v)
                    batch.Delete(leveldb::Slice(slice));
                db.WriteBatch(batch);
                v.clear();
            }

            pcursor->Next();
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    if (!v.empty())
    {
        i += v.size();
        leveldb::WriteBatch batch;
        BOOST_FOREACH(const std::string& slice, v)
            batch.Delete(leveldb::Slice(slice));
        db.WriteBatch(batch);
    }
    if (i > 0)
        LogPrintf("Address index with %d addresses successfully deleted.\n", i);

    delete pcursor;
    return true;
}

bool CCoinsViewDB::GenerateAllCoinsByAddress()
{
    LogPrintf("Building address index for -txoutsbyaddressindex. Be patient...\n");
    int64_t nTxCount = GetPrefixCount('c');

    leveldb::Iterator *pcursor = const_cast<CLevelDBWrapper*>(&db)->NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << 'c';
    pcursor->Seek(ssKeySet.str());

    CCoinsMapByAddress mapCoinsByAddress;
    int64_t i = 0;
    int64_t progress = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType != 'c')
                break;

            if (progress % 1000 == 0 && nTxCount > 0)
                uiInterface.ShowProgress(_("Building address index..."), (int)(((double)progress / (double)nTxCount) * (double)100));
            progress++;

            leveldb::Slice slValue = pcursor->value();
            CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
            CCoins coins;
            ssValue >> coins;
            uint256 txhash;
            ssKey >> txhash;

            for (unsigned int j = 0; j < coins.vout.size(); j++)
            {
                if (coins.vout[j].IsNull() || coins.vout[j].scriptPubKey.IsUnspendable())
                    continue;

                if (!mapCoinsByAddress.count(coins.vout[j].scriptPubKey))
                {
                    CCoinsByAddress coinsByAddress;
                    GetCoinsByAddress(const_cast<CScript&>(coins.vout[j].scriptPubKey), coinsByAddress);
                    mapCoinsByAddress.insert(make_pair(coins.vout[j].scriptPubKey, coinsByAddress));
                }
                mapCoinsByAddress[coins.vout[j].scriptPubKey].setCoins.insert(COutPoint(txhash, (uint32_t)j));
            }

            if (mapCoinsByAddress.size() >= 10000)
            {
                i += mapCoinsByAddress.size();
                CLevelDBBatch batch;
                for (CCoinsMapByAddress::iterator it = mapCoinsByAddress.begin(); it != mapCoinsByAddress.end();) {
                    BatchWriteCoins(batch, it->first, it->second);
                    CCoinsMapByAddress::iterator itOld = it++;
                    mapCoinsByAddress.erase(itOld);
                }
                db.WriteBatch(batch);
                mapCoinsByAddress.clear();
            }

            pcursor->Next();
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    if (!mapCoinsByAddress.empty())
    {
       i += mapCoinsByAddress.size();
       CLevelDBBatch batch;
       for (CCoinsMapByAddress::iterator it = mapCoinsByAddress.begin(); it != mapCoinsByAddress.end();) {
           BatchWriteCoins(batch, it->first, it->second);
           CCoinsMapByAddress::iterator itOld = it++;
           mapCoinsByAddress.erase(itOld);
       }
       db.WriteBatch(batch);
    }
    LogPrintf("Address index with %d addresses successfully built.\n", i);
    delete pcursor;
    return true;
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(make_pair('t', txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CLevelDBBatch batch;
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair('t', it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair('F', name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair('F', name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts()
{
    boost::scoped_ptr<leveldb::Iterator> pcursor(NewIterator());

    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << make_pair('b', uint256(0));
    pcursor->Seek(ssKeySet.str());

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'b') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CDiskBlockIndex diskindex;
                ssValue >> diskindex;

                // Construct block index object
                CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nTx            = diskindex.nTx;

                if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits))
                    return error("LoadBlockIndex() : CheckProofOfWork failed: %s", pindexNew->ToString());

                pcursor->Next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }

    return true;
}
