// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <iostream>

#include <coins.h>
#include <consensus/validation.h>
#include <util/coinjoins.h>

bool WhirlpoolTransactions::isWhirlpool(const CTransactionRef& tx) {
    if (tx->vin.size() == 5 && tx->vout.size() == 5) {
        CAmount amount = tx->vout.at(0).nValue;

        // These are the only whirlpool pools
        if (amount != 5000000 && amount != 1000000 && amount != 50000000) {
            return false;
        }

        for (const auto& tx_out : tx->vout) {
            if (tx_out.nValue != amount) return false;
        }

        for (const CTxIn& tx_in : tx->vin) {
            if (cj_transactions.contains(tx_in.prevout.hash)) return true;
        }
        return false;
    }

    return false;
}

CFeeRate GetMedianFeeRateFromBlock(const CBlock& block, CCoinsViewCache &view) {
    // calculate median fee rate
    std::vector<CFeeRate> feeRates;
    for (const auto& tx : block.vtx) {
      CAmount value_in = 0;
      for (auto &v : tx->vin) {
        const COutPoint &prevout = v.prevout;
        const Coin& coin = view.AccessCoin(prevout);
        value_in += coin.out.nValue;
      }
      CAmount value_out = tx->GetValueOut();
      size_t txSize = GetTransactionWeight(*tx);
      std::cout << tx->GetHash().ToString() << " " << value_in << " " << value_out << std::endl;
      CFeeRate feeRate(value_in - value_out, txSize);
      feeRates.push_back(feeRate);
    }
    std::sort(feeRates.begin(), feeRates.end());
    if (feeRates.size() % 2 == 1) {
      return CFeeRate(feeRates[feeRates.size()/2].GetFeePerK());
    } else {
      return CFeeRate((feeRates[feeRates.size()/2].GetFeePerK()+feeRates[feeRates.size()/2+1].GetFeePerK())/2);
    }
}

void WhirlpoolTransactions::Update(const CTransactionRef& tx, int block_height, CFeeRate median_fee_rate) {
    if (isWhirlpool(tx)) {

        // cj_file << tx->GetHash().ToString() << "," << tx->vout.at(0).nValue << "," << block_height <<"\n"; // this writes denomination instead of median feerate
        cj_file << tx->GetHash().ToString() << "," << median_fee_rate.GetFeePerK() << "," << block_height <<"\n";

        cj_transactions.insert(tx->GetHash());
        for (const CTxIn& tx_in : tx->vin) {
            if (!cj_transactions.contains(tx_in.prevout.hash)) {
                tx0s.Update(tx_in.prevout.hash, tx->vout.at(0).nValue);
            }
        }
    }
}
