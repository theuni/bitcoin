// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PEERCOUNTLIMITS_H
#define BITCOIN_PEERCOUNTLIMITS_H

#include <algorithm>

/** The maximum number of peer connections to maintain. */
static const unsigned int DEFAULT_MAX_PEER_CONNECTIONS = 125;
/** Maximum number of automatic outgoing nodes over which we'll relay everything (blocks, tx, addrs, etc) */
static const int MAX_OUTBOUND_FULL_RELAY_CONNECTIONS = 8;
/** Maximum number of feeler connections */
static const int MAX_FEELER_CONNECTIONS = 1;
/** Maximum number of block-relay-only outgoing connections */
static const int MAX_BLOCK_RELAY_ONLY_CONNECTIONS = 2;

struct PeerCountLimits {
    PeerCountLimits(int max_automatic_connections = DEFAULT_MAX_PEER_CONNECTIONS) : m_max_automatic_connections(max_automatic_connections){}
/**
     * Maximum number of automatic connections permitted, excluding manual
     * connections but including inbounds. May be changed by the user and is
     * potentially limited by the operating system (number of file descriptors).
     */
    int m_max_automatic_connections;

    /*
     * Maximum number of peers by connection type. Might vary from defaults
     * based on -maxconnections init value.
     */

    // How many full-relay (tx, block, addr) outbound peers we want
    int m_max_outbound_full_relay = std::min(MAX_OUTBOUND_FULL_RELAY_CONNECTIONS, m_max_automatic_connections);

    // How many block-relay only outbound peers we want
    // We do not relay tx or addr messages with these peers
    int m_max_outbound_block_relay = std::min(MAX_BLOCK_RELAY_ONLY_CONNECTIONS, m_max_automatic_connections - m_max_outbound_full_relay);

    int m_max_automatic_outbound = m_max_outbound_full_relay + m_max_outbound_block_relay + MAX_FEELER_CONNECTIONS;
    int m_max_inbound = std::max(0, m_max_automatic_connections - m_max_automatic_outbound);
};

#endif // BITCOIN_PEERCOUNTLIMITS_H
