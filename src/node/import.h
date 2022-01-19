// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_IMPORT_H
#define BITCOIN_NODE_IMPORT_H

#include <early_exit.h>
#include <fs.h>

#include <atomic>
#include <cstdint>
#include <vector>

class ArgsManager;
class ChainstateManager;

namespace node {
static constexpr bool DEFAULT_STOPAFTERBLOCKIMPORT{false};

extern std::atomic_bool fImporting;

MaybeEarlyExit<> BlockImport(ChainstateManager& chainman, std::vector<fs::path> vImportFiles, const ArgsManager& args);

} // namespace node

#endif // BITCOIN_NODE_IMPORT_H
