// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_P2P_H
#define BITCOIN_INIT_P2P_H

#include <net.h>

class ArgsManager;

void AddP2POptions(ArgsManager& argsman);
void InitP2PParameterInteraction(ArgsManager& args);
bool CreateP2POptions(const ArgsManager& args, CConnman::Options& connOptions);
bool AppInitP2PParameterInteraction(const ArgsManager& args, int reserved_fds);

#endif // BITCOIN_INIT_P2P_H
