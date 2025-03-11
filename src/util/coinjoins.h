// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_COINJOINS_H
#define BITCOIN_UTIL_COINJOINS_H

#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <primitives/block.h>

#include <fstream>
#include <util/fs.h>

class Tx0s {
    fs::path datadir;

    // txid, denomination(s)
    std::map<uint256,std::vector<CAmount>> tx0_set;

public:
    Tx0s(fs::path datadir)
      : datadir(datadir)
    {
        tx0_set = {};
    }

    /*
    ~Tx0s() {
        std::ofstream tx0_file;
        tx0_file.open(datadir + "tx0s.csv");
        for (auto entry = tx0_set.begin(); entry != tx0_set.end(); ++entry) {
            tx0_file << entry->first.ToString() << ",";

            for (unsigned int i = 0; i < (entry->second.size() - 1); i++) {
                tx0_file << entry->second[i] << ",";
            }
            tx0_file << entry->second[entry->second.size() - 1] << "\n";
        }
        tx0_file.close();
    }
    */

    void Update(const uint256& txid, const CAmount& denomination) {
        auto entry = tx0_set.find(txid);
        if (entry != tx0_set.end()) {
            tx0_set[txid].push_back(denomination);
        } else {
            // create new entry
            tx0_set[txid] = {denomination};
        }
    }

    int Size() {
        return tx0_set.size();
    }
};
class CBlockUndo;
CFeeRate GetMedianFeeRateFromBlock(const CBlock& block, const CBlockUndo& undo);

class WhirlpoolTransactions {
    Tx0s tx0s;
    std::set<uint256> cj_transactions;

    std::ofstream cj_file;

    bool isWhirlpool(const CTransactionRef& tx);

public:
    WhirlpoolTransactions(fs::path datadir)
      : tx0s{datadir}
    {
        cj_file.open(datadir + "coinjoins.csv", std::ios_base::app);

        cj_transactions = {};

        // Add all Genesis Whirlpool transactions
        cj_transactions.insert(uint256S("c6c27bef217583cca5f89de86e0cd7d8b546844f800da91d91a74039c3b40fba"));
        cj_transactions.insert(uint256S("94b0da89431d8bd74f1134d8152ed1c7c4f83375e63bc79f19cf293800a83f52"));
        cj_transactions.insert(uint256S("b42df707a3d876b24a22b0199e18dc39aba2eafa6dbeaaf9dd23d925bb379c59"));
        cj_transactions.insert(uint256S("4c906f897467c7ed8690576edfcaf8b1fb516d154ef6506a2c4cab2c48821728"));
        cj_transactions.insert(uint256S("a42596825352055841949a8270eda6fb37566a8780b2aec6b49d8035955d060e"));
        cj_transactions.insert(uint256S("a554db794560458c102bab0af99773883df13bc66ad287c29610ad9bac138926"));
        cj_transactions.insert(uint256S("792c0bfde7f6bf023ff239660fb876315826a0a52fd32e78ea732057789b2be0"));
    }

    ~WhirlpoolTransactions() {
        cj_file.close();
    }

    void Update(const CTransactionRef& tx, int block_height, CFeeRate median_fee_rate);

    int GetNumTx0s() {
        return tx0s.Size();
    }
};

#endif // BITCOIN_UTIL_COINJOINS_H
