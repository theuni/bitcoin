// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ZMQPORTS_H
#define BITCOIN_ZMQPORTS_H

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include <string>

// Global state
extern bool fZMQPub;

enum ZMQFormat
{
  ZMQ_FORMAT_NETWORK,
  ZMQ_FORMAT_HASH
};

#if ENABLE_ZMQ
void ZMQShutdown();
void ZMQInitialize(const std::string &endp, ZMQFormat format);
#else
static inline void ZMQInitialize(const std::string &endp, ZMQFormat format) {}
static inline void ZMQShutdown() {}
#endif // ENABLE_ZMQ

#endif // BITCOIN_ZMQPORTS_H
