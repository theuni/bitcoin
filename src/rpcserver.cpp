// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"

#include "base58.h"
#include "init.h"
#include "main.h"
#include "ui_interface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_upper()
#include "json/json_spirit_writer_template.h"

using namespace json_spirit;
using namespace std;

static bool fRPCRunning = false;
static bool fRPCInWarmup = true;
static std::string rpcWarmupStatus("RPC server started");
static CCriticalSection cs_rpcWarmup;
/* Timer-creating functions */
static std::vector<RPCTimerInterface*> timerInterfaces;
/* Map of name to timer.
 * @note Can be changed to std::unique_ptr when C++11 */
static std::map<std::string, boost::shared_ptr<RPCTimerBase> > deadlineTimers;

void RPCTypeCheck(const Array& params,
                  const list<Value_type>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    BOOST_FOREACH(Value_type t, typesExpected)
    {
        if (params.size() <= i)
            break;

        const Value& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   Value_type_name[t], Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheck(const Object& o,
                  const map<string, Value_type>& typesExpected,
                  bool fAllowNull)
{
    BOOST_FOREACH(const PAIRTYPE(string, Value_type)& t, typesExpected)
    {
        const Value& v = find_value(o, t.first);
        if (!fAllowNull && v.type() == null_type)
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!((v.type() == t.second) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   Value_type_name[t.second], t.first, Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

static inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

CAmount AmountFromValue(const Value& value)
{
    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > 21000000.0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    CAmount nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(const CAmount& amount)
{
    return (double)amount / (double)COIN;
}

uint256 ParseHashV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const Object& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
vector<unsigned char> ParseHexV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}
vector<unsigned char> ParseHexO(const Object& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}


/**
 * Note: This interface may still be subject to change.
 */

string CRPCTable::help(string strCommand) const
{
    string strRet;
    string category;
    set<rpcfn_type> setDone;
    vector<pair<string, const CRPCCommand*> > vCommands;

    for (map<string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
        vCommands.push_back(make_pair(mi->second->category + mi->first, mi->second));
    sort(vCommands.begin(), vCommands.end());

    BOOST_FOREACH(const PAIRTYPE(string, const CRPCCommand*)& command, vCommands)
    {
        const CRPCCommand *pcmd = command.second;
        string strMethod = pcmd->name;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != string::npos)
            continue;
        if ((strCommand != "" || pcmd->category == "hidden") && strMethod != strCommand)
            continue;
#ifdef ENABLE_WALLET
        if (pcmd->reqWallet && !pwalletMain)
            continue;
#endif

        try
        {
            Array params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (const std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strCommand == "")
            {
                if (strHelp.find('\n') != string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));

                if (category != pcmd->category)
                {
                    if (!category.empty())
                        strRet += "\n";
                    category = pcmd->category;
                    string firstLetter = category.substr(0,1);
                    boost::to_upper(firstLetter);
                    strRet += "== " + firstLetter + category.substr(1) + " ==\n";
                }
            }
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "help ( \"command\" )\n"
            "\nList all commands, or get help for a specified command.\n"
            "\nArguments:\n"
            "1. \"command\"     (string, optional) The command to get help on\n"
            "\nResult:\n"
            "\"text\"     (string) The help text\n"
        );

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    return tableRPC.help(strCommand);
}


Value stop(const Array& params, bool fHelp)
{
    // Accept the deprecated and ignored 'detach' boolean argument
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "stop\n"
            "\nStop Bitcoin server.");
    // Shutdown will take long enough that the response should get back
    StartShutdown();
    return "Bitcoin server stopping";
}



/**
 * Call Table
 */
static const CRPCCommand vRPCCommands[] =
{ //  category              name                      actor (function)         okSafeMode threadSafe reqWallet
  //  --------------------- ------------------------  -----------------------  ---------- ---------- ---------
    /* Overall control/query calls */
    { "control",            "getinfo",                &getinfo,                true,      false,      false }, /* uses wallet if enabled */
    { "control",            "help",                   &help,                   true,      true,       false },
    { "control",            "stop",                   &stop,                   true,      true,       false },

    /* P2P networking */
    { "network",            "getnetworkinfo",         &getnetworkinfo,         true,      false,      false },
    { "network",            "addnode",                &addnode,                true,      true,       false },
    { "network",            "getaddednodeinfo",       &getaddednodeinfo,       true,      true,       false },
    { "network",            "getconnectioncount",     &getconnectioncount,     true,      false,      false },
    { "network",            "getnettotals",           &getnettotals,           true,      true,       false },
    { "network",            "getpeerinfo",            &getpeerinfo,            true,      false,      false },
    { "network",            "ping",                   &ping,                   true,      false,      false },

    /* Block chain and UTXO */
    { "blockchain",         "getblockchaininfo",      &getblockchaininfo,      true,      false,      false },
    { "blockchain",         "getbestblockhash",       &getbestblockhash,       true,      false,      false },
    { "blockchain",         "getblockcount",          &getblockcount,          true,      false,      false },
    { "blockchain",         "getblock",               &getblock,               true,      false,      false },
    { "blockchain",         "getblockhash",           &getblockhash,           true,      false,      false },
    { "blockchain",         "getchaintips",           &getchaintips,           true,      false,      false },
    { "blockchain",         "getdifficulty",          &getdifficulty,          true,      false,      false },
    { "blockchain",         "getmempoolinfo",         &getmempoolinfo,         true,      true,       false },
    { "blockchain",         "getrawmempool",          &getrawmempool,          true,      false,      false },
    { "blockchain",         "gettxout",               &gettxout,               true,      false,      false },
    { "blockchain",         "gettxoutsetinfo",        &gettxoutsetinfo,        true,      false,      false },
    { "blockchain",         "verifychain",            &verifychain,            true,      false,      false },
    { "blockchain",         "invalidateblock",        &invalidateblock,        true,      true,       false },
    { "blockchain",         "reconsiderblock",        &reconsiderblock,        true,      true,       false },

    /* Mining */
    { "mining",             "getblocktemplate",       &getblocktemplate,       true,      false,      false },
    { "mining",             "getmininginfo",          &getmininginfo,          true,      false,      false },
    { "mining",             "getnetworkhashps",       &getnetworkhashps,       true,      false,      false },
    { "mining",             "prioritisetransaction",  &prioritisetransaction,  true,      false,      false },
    { "mining",             "submitblock",            &submitblock,            true,      true,       false },

#ifdef ENABLE_WALLET
    /* Coin generation */
    { "generating",         "getgenerate",            &getgenerate,            true,      false,      false },
    { "generating",         "gethashespersec",        &gethashespersec,        true,      false,      false },
    { "generating",         "setgenerate",            &setgenerate,            true,      true,       false },
#endif

    /* Raw transactions */
    { "rawtransactions",    "createrawtransaction",   &createrawtransaction,   true,      false,      false },
    { "rawtransactions",    "decoderawtransaction",   &decoderawtransaction,   true,      false,      false },
    { "rawtransactions",    "decodescript",           &decodescript,           true,      false,      false },
    { "rawtransactions",    "getrawtransaction",      &getrawtransaction,      true,      false,      false },
    { "rawtransactions",    "sendrawtransaction",     &sendrawtransaction,     false,     false,      false },
    { "rawtransactions",    "signrawtransaction",     &signrawtransaction,     false,     false,      false }, /* uses wallet if enabled */

    /* Utility functions */
    { "util",               "createmultisig",         &createmultisig,         true,      true ,      false },
    { "util",               "validateaddress",        &validateaddress,        true,      false,      false }, /* uses wallet if enabled */
    { "util",               "verifymessage",          &verifymessage,          true,      false,      false },
    { "util",               "estimatefee",            &estimatefee,            true,      true,       false },
    { "util",               "estimatepriority",       &estimatepriority,       true,      true,       false },

    /* Not shown in help */
    { "hidden",             "invalidateblock",        &invalidateblock,        true,      true,       false },
    { "hidden",             "reconsiderblock",        &reconsiderblock,        true,      true,       false },
    { "hidden",             "setmocktime",            &setmocktime,            true,      false,      false },

#ifdef ENABLE_WALLET
    /* Wallet */
    { "wallet",             "addmultisigaddress",     &addmultisigaddress,     true,      false,      true },
    { "wallet",             "backupwallet",           &backupwallet,           true,      false,      true },
    { "wallet",             "dumpprivkey",            &dumpprivkey,            true,      false,      true },
    { "wallet",             "dumpwallet",             &dumpwallet,             true,      false,      true },
    { "wallet",             "encryptwallet",          &encryptwallet,          true,      false,      true },
    { "wallet",             "getaccountaddress",      &getaccountaddress,      true,      false,      true },
    { "wallet",             "getaccount",             &getaccount,             true,      false,      true },
    { "wallet",             "getaddressesbyaccount",  &getaddressesbyaccount,  true,      false,      true },
    { "wallet",             "getbalance",             &getbalance,             false,     false,      true },
    { "wallet",             "getnewaddress",          &getnewaddress,          true,      false,      true },
    { "wallet",             "getrawchangeaddress",    &getrawchangeaddress,    true,      false,      true },
    { "wallet",             "getreceivedbyaccount",   &getreceivedbyaccount,   false,     false,      true },
    { "wallet",             "getreceivedbyaddress",   &getreceivedbyaddress,   false,     false,      true },
    { "wallet",             "gettransaction",         &gettransaction,         false,     false,      true },
    { "wallet",             "getunconfirmedbalance",  &getunconfirmedbalance,  false,     false,      true },
    { "wallet",             "getwalletinfo",          &getwalletinfo,          false,     false,      true },
    { "wallet",             "importprivkey",          &importprivkey,          true,      false,      true },
    { "wallet",             "importwallet",           &importwallet,           true,      false,      true },
    { "wallet",             "importaddress",          &importaddress,          true,      false,      true },
    { "wallet",             "keypoolrefill",          &keypoolrefill,          true,      false,      true },
    { "wallet",             "listaccounts",           &listaccounts,           false,     false,      true },
    { "wallet",             "listaddressgroupings",   &listaddressgroupings,   false,     false,      true },
    { "wallet",             "listlockunspent",        &listlockunspent,        false,     false,      true },
    { "wallet",             "listreceivedbyaccount",  &listreceivedbyaccount,  false,     false,      true },
    { "wallet",             "listreceivedbyaddress",  &listreceivedbyaddress,  false,     false,      true },
    { "wallet",             "listsinceblock",         &listsinceblock,         false,     false,      true },
    { "wallet",             "listtransactions",       &listtransactions,       false,     false,      true },
    { "wallet",             "listunspent",            &listunspent,            false,     false,      true },
    { "wallet",             "lockunspent",            &lockunspent,            true,      false,      true },
    { "wallet",             "move",                   &movecmd,                false,     false,      true },
    { "wallet",             "sendfrom",               &sendfrom,               false,     false,      true },
    { "wallet",             "sendmany",               &sendmany,               false,     false,      true },
    { "wallet",             "sendtoaddress",          &sendtoaddress,          false,     false,      true },
    { "wallet",             "setaccount",             &setaccount,             true,      false,      true },
    { "wallet",             "settxfee",               &settxfee,               true,      false,      true },
    { "wallet",             "signmessage",            &signmessage,            true,      false,      true },
    { "wallet",             "walletlock",             &walletlock,             true,      false,      true },
    { "wallet",             "walletpassphrasechange", &walletpassphrasechange, true,      false,      true },
    { "wallet",             "walletpassphrase",       &walletpassphrase,       true,      false,      true },
#endif // ENABLE_WALLET
};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](string name) const
{
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}

bool StartRPC()
{
    LogPrint("rpc", "Starting RPC\n");
    fRPCRunning = true;
    return true;
}

void InterruptRPC()
{
    LogPrint("rpc", "Interrupting RPC\n");
    // Interrupt e.g. running longpolls
    fRPCRunning = false;
}

void StopRPC()
{
    LogPrint("rpc", "Stopping RPC\n");
    deadlineTimers.clear();
}

bool IsRPCRunning()
{
    return fRPCRunning;
}

void SetRPCWarmupStatus(const std::string& newStatus)
{
    LOCK(cs_rpcWarmup);
    rpcWarmupStatus = newStatus;
}

void SetRPCWarmupFinished()
{
    LOCK(cs_rpcWarmup);
    assert(fRPCInWarmup);
    fRPCInWarmup = false;
}

bool RPCIsInWarmup(std::string *outStatus)
{
    LOCK(cs_rpcWarmup);
    if (outStatus)
        *outStatus = rpcWarmupStatus;
    return fRPCInWarmup;
}

void JSONRequest::parse(const Value& valRequest)
{
    // Parse request
    if (valRequest.type() != obj_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const Object& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    Value valMethod = find_value(request, "method");
    if (valMethod.type() == null_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (valMethod.type() != str_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getblocktemplate")
        LogPrint("rpc", "ThreadRPCServer method=%s\n", strMethod);

    // Parse params
    Value valParams = find_value(request, "params");
    if (valParams.type() == array_type)
        params = valParams.get_array();
    else if (valParams.type() == null_type)
        params = Array();
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}


static Object JSONRPCExecOne(const Value& req)
{
    Object rpc_result;

    JSONRequest jreq;
    try {
        jreq.parse(req);

        Value result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (const Object& objError)
    {
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (const std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(Value::null,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

string JSONRPCExecBatch(const Array& vReq)
{
    Array ret;
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return write_string(Value(ret), false) + "\n";
}

json_spirit::Value CRPCTable::execute(const std::string &strMethod, const json_spirit::Array &params) const
{
    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
#ifdef ENABLE_WALLET
    if (pcmd->reqWallet && !pwalletMain)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
#endif

    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode", false) &&
        !pcmd->okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

    try
    {
        // Execute
        Value result;
        {
            if (pcmd->threadSafe)
                result = pcmd->actor(params, false);
#ifdef ENABLE_WALLET
            else if (!pwalletMain) {
                LOCK(cs_main);
                result = pcmd->actor(params, false);
            } else {
                LOCK2(cs_main, pwalletMain->cs_wallet);
                result = pcmd->actor(params, false);
            }
#else // ENABLE_WALLET
            else {
                LOCK(cs_main);
                result = pcmd->actor(params, false);
            }
#endif // !ENABLE_WALLET
        }
        return result;
    }
    catch (const std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

std::string HelpExampleCli(string methodname, string args){
    return "> bitcoin-cli " + methodname + " " + args + "\n";
}

std::string HelpExampleRpc(string methodname, string args){
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
        "\"method\": \"" + methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/\n";
}

void RPCRegisterTimerInterface(RPCTimerInterface *iface)
{
    timerInterfaces.push_back(iface);
}

void RPCUnregisterTimerInterface(RPCTimerInterface *iface)
{
    std::vector<RPCTimerInterface*>::iterator i = std::find(timerInterfaces.begin(), timerInterfaces.end(), iface);
    assert(i != timerInterfaces.end());
    timerInterfaces.erase(i);
}

void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds)
{
    if (timerInterfaces.empty())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No timer handler registered for RPC");
    deadlineTimers.erase(name);
    RPCTimerInterface* timerInterface = timerInterfaces[0];
    LogPrint("rpc", "queue run of timer %s in %i seconds (using %s)\n", name, nSeconds, timerInterface->Name());
    deadlineTimers.insert(std::make_pair(name, timerInterface->NewTimer(func, nSeconds)));
}

const CRPCTable tableRPC;
