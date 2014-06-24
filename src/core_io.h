#ifndef __BITCOIN_CORE_IO_H__
#define __BITCOIN_CORE_IO_H__

#include <string>
#include "uint256.h"

class CScript;
class CTransaction;
class UniValue;

// core_read.cpp
extern CScript ParseScript(std::string s);
extern bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);

// core_write.cpp
extern std::string EncodeHexTx(const CTransaction& tx);
extern void ScriptPubKeyToUniv(const CScript& scriptPubKey,
                        UniValue& out, bool fIncludeHex);
extern void TxToUniv(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);

#endif // __BITCOIN_CORE_IO_H__
