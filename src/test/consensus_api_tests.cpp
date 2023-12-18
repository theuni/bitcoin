// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>
#include <script/bitcoinconsensus.h>
#include <script/script.h>
#include <script/script_error.h>
#include <streams.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

struct consensus_tests{};


CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey, int nValue)
{
    CMutableTransaction txCredit;
    txCredit.nVersion = 1;
    txCredit.nLockTime = 0;
    txCredit.vin.resize(1);
    txCredit.vout.resize(1);
    txCredit.vin[0].prevout.SetNull();
    txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txCredit.vout[0].scriptPubKey = scriptPubKey;
    txCredit.vout[0].nValue = nValue;

    return txCredit;
}

CMutableTransaction BuildSpendingTransaction(const CScript& scriptSig, const CScriptWitness& scriptWitness, const CTransaction& txCredit)
{
    CMutableTransaction txSpend;
    txSpend.nVersion = 1;
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vout.resize(1);
    txSpend.vin[0].scriptWitness = scriptWitness;
    txSpend.vin[0].prevout.hash = txCredit.GetHash();
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].scriptSig = scriptSig;
    txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txSpend.vout[0].scriptPubKey = CScript();
    txSpend.vout[0].nValue = txCredit.vout[0].nValue;

    return txSpend;
}


BOOST_FIXTURE_TEST_SUITE(consensus_api_tests, consensus_tests )

BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_returns_true)
{
    unsigned int libconsensus_flags = 0;
    int nIn = 0;

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_1;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << TX_WITH_WITNESS(spendTx);

    bitcoinconsensus_error err;
    int result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 1);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_OK);
}

/* Test bitcoinconsensus_verify_script returns invalid tx index err*/
BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_tx_index_err)
{
    unsigned int libconsensus_flags = 0;
    int nIn = 3;

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_EQUAL;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << TX_WITH_WITNESS(spendTx);

    bitcoinconsensus_error err;
    int result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_TX_INDEX);
}

/* Test bitcoinconsensus_verify_script returns tx size mismatch err*/
BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_tx_size)
{
    unsigned int libconsensus_flags = 0;
    int nIn = 0;

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_EQUAL;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << TX_WITH_WITNESS(spendTx);

    bitcoinconsensus_error err;
    int result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size() * 2, nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_TX_SIZE_MISMATCH);
}

/* Test bitcoinconsensus_verify_script returns invalid tx serialization error */
BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_tx_serialization)
{
    unsigned int libconsensus_flags = 0;
    int nIn = 0;

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_EQUAL;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << 0xffffffff;

    bitcoinconsensus_error err;
    int result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_TX_DESERIALIZE);
}

/* Test bitcoinconsensus_verify_script returns amount required error */
BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_amount_required_err)
{
    unsigned int libconsensus_flags = bitcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS;
    int nIn = 0;

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_EQUAL;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << TX_WITH_WITNESS(spendTx);

    bitcoinconsensus_error err;
    int result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_AMOUNT_REQUIRED);
}

/* Test bitcoinconsensus_verify_script returns invalid flags err */
BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_invalid_flags)
{
    unsigned int libconsensus_flags = 1 << 3;
    int nIn = 0;

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_EQUAL;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << TX_WITH_WITNESS(spendTx);

    bitcoinconsensus_error err;
    int result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_INVALID_FLAGS);
}

/* Test bitcoinconsensus_verify_script returns spent outputs required err */
BOOST_AUTO_TEST_CASE(bitcoinconsensus_verify_script_spent_outputs_required_err)
{
    unsigned int libconsensus_flags{bitcoinconsensus_SCRIPT_FLAGS_VERIFY_TAPROOT};
    const int nIn{0};

    CScript scriptPubKey;
    CScript scriptSig;
    CScriptWitness wit;

    scriptPubKey << OP_EQUAL;
    CTransaction creditTx{BuildCreditingTransaction(scriptPubKey, 1)};
    CTransaction spendTx{BuildSpendingTransaction(scriptSig, wit, creditTx)};

    DataStream stream;
    stream << TX_WITH_WITNESS(spendTx);

    bitcoinconsensus_error err;
    int result{bitcoinconsensus_verify_script_with_spent_outputs(scriptPubKey.data(), scriptPubKey.size(), creditTx.vout[0].nValue, UCharCast(stream.data()), stream.size(), nullptr, 0, nIn, libconsensus_flags, &err)};
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_SPENT_OUTPUTS_REQUIRED);

    result = bitcoinconsensus_verify_script_with_amount(scriptPubKey.data(), scriptPubKey.size(), creditTx.vout[0].nValue, UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_SPENT_OUTPUTS_REQUIRED);

    result = bitcoinconsensus_verify_script(scriptPubKey.data(), scriptPubKey.size(), UCharCast(stream.data()), stream.size(), nIn, libconsensus_flags, &err);
    BOOST_CHECK_EQUAL(result, 0);
    BOOST_CHECK_EQUAL(err, bitcoinconsensus_ERR_SPENT_OUTPUTS_REQUIRED);
}

BOOST_AUTO_TEST_SUITE_END()
