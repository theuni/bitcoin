// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVICTION_H
#define BITCOIN_EVICTION_H

#include <connection_types.h>
#include <netaddress.h>
#include <sync.h>

#include <atomic>
#include <chrono>
#include <cstdint>

typedef int64_t NodeId;
class CNode;

struct NodeEvictionCandidate
{
    NodeId id;
    int64_t nTimeConnected;
    std::atomic<std::chrono::microseconds> m_min_ping_time;
    std::atomic<int64_t> nLastBlockTime;
    std::atomic<int64_t> nLastTXTime;
    bool fRelevantServices;
    std::atomic<bool> m_relay_txs;
    std::atomic<bool> fBloomFilter;
    uint64_t nKeyedNetGroup;
    bool prefer_evict;
    bool m_is_local;
    Network m_network;
    bool m_is_inbound;
    bool m_has_perm_noban;
    ConnectionType m_conn_type;
    bool fSuccessfullyConnected;
    int nBlocksInFlight;
    int64_t m_last_block_announcement;
    bool m_slow_chain_protected;

    NodeEvictionCandidate() = default;
    NodeEvictionCandidate(const NodeEvictionCandidate& rhs) :
        id(rhs.id),
        nTimeConnected(rhs.nTimeConnected),
        m_min_ping_time(rhs.m_min_ping_time.load()),
        nLastBlockTime(rhs.nLastBlockTime.load()),
        nLastTXTime(rhs.nLastTXTime.load()),
        fRelevantServices(rhs.fRelevantServices),
        m_relay_txs(rhs.m_relay_txs.load()),
        fBloomFilter(rhs.fBloomFilter.load()),
        nKeyedNetGroup(rhs.nKeyedNetGroup),
        prefer_evict(rhs.prefer_evict),
        m_is_local(rhs.m_is_local),
        m_network(rhs.m_network),
        m_is_inbound(rhs.m_is_inbound),
        m_has_perm_noban(rhs.m_has_perm_noban),
        m_conn_type(rhs.m_conn_type),
        fSuccessfullyConnected(rhs.fSuccessfullyConnected),
        nBlocksInFlight(rhs.nBlocksInFlight),
        m_last_block_announcement(rhs.m_last_block_announcement),
        m_slow_chain_protected(rhs.m_slow_chain_protected)
        {}

    NodeEvictionCandidate& operator=(const NodeEvictionCandidate& rhs) {
        id = rhs.id;
        nTimeConnected = rhs.nTimeConnected;
        m_min_ping_time = rhs.m_min_ping_time.load();
        nLastBlockTime = rhs.nLastBlockTime.load();
        nLastTXTime = rhs.nLastTXTime.load();
        fRelevantServices = rhs.fRelevantServices;
        m_relay_txs = rhs.m_relay_txs.load();
        fBloomFilter = rhs.fBloomFilter.load();
        nKeyedNetGroup = rhs.nKeyedNetGroup;
        prefer_evict = rhs.prefer_evict;
        m_is_local = rhs.m_is_local;
        m_network = rhs.m_network;
        m_is_inbound = rhs.m_is_inbound;
        m_has_perm_noban = rhs.m_has_perm_noban;
        m_conn_type = rhs.m_conn_type;
        fSuccessfullyConnected = rhs.fSuccessfullyConnected;
        nBlocksInFlight = rhs.nBlocksInFlight;
        m_last_block_announcement = rhs.m_last_block_announcement;
        m_slow_chain_protected = rhs.m_slow_chain_protected;
        return *this;
    }
};

class EvictionMan
{

static bool ReverseCompareNodeMinPingTime(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b);
static bool ReverseCompareNodeTimeConnected(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b);
static bool CompareNetGroupKeyed(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b);
static bool CompareNodeBlockTime(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b);
static bool CompareNodeTXTime(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b);
static bool CompareNodeBlockRelayOnlyTime(const NodeEvictionCandidate &a, const NodeEvictionCandidate &b);

/** Protect desirable or disadvantaged inbound peers from eviction by ratio.
 *
 * This function protects half of the peers which have been connected the
 * longest, to replicate the non-eviction implicit behavior and preclude attacks
 * that start later.
 *
 * Half of these protected spots (1/4 of the total) are reserved for the
 * following categories of peers, sorted by longest uptime, even if they're not
 * longest uptime overall:
 *
 * - onion peers connected via our tor control service
 *
 * - localhost peers, as manually configured hidden services not using
 *   `-bind=addr[:port]=onion` will not be detected as inbound onion connections
 *
 * - I2P peers
 *
 * This helps protect these privacy network peers, which tend to be otherwise
 * disadvantaged under our eviction criteria for their higher min ping times
 * relative to IPv4/IPv6 peers, and favorise the diversity of peer connections.
 */
static void ProtectEvictionCandidatesByRatio(std::vector<NodeEvictionCandidate>& eviction_candidates);

/**
 * Select an inbound peer to evict after filtering out (protecting) peers having
 * distinct, difficult-to-forge characteristics. The protection logic picks out
 * fixed numbers of desirable peers per various criteria, followed by (mostly)
 * ratios of desirable or disadvantaged peers. If any eviction candidates
 * remain, the selection logic chooses a peer to evict.
 */
[[nodiscard]] static std::optional<NodeId> SelectNodeToEvict(std::vector<NodeEvictionCandidate>&& vEvictionCandidates);

std::vector<NodeEvictionCandidate> vNodes;
Mutex cs_vNodes;

public:

void AddNode(NodeEvictionCandidate candidate);
bool RemoveNode(NodeId id);

/** Try to find a connection to evict when the node is full.
 *  Extreme care must be taken to avoid opening the node to attacker
 *   triggered network partitioning.
 *  The strategy used here is to protect a small number of peers
 *   for each of several distinct characteristics which are difficult
 *   to forge.  In order to partition a node the attacker must be
 *   simultaneously better at all of them than honest peers.
 */
std::optional<NodeId> AttemptToEvictConnection();


/** A ping-pong round trip has completed successfully. Update minimum ping time. */
void PongReceived(NodeId id, std::chrono::microseconds ping_time);
void UpdateLatestBlockTime(NodeId id, int64_t time);
void UpdateLatestTxTime(NodeId id, int64_t time);
void UpdateRelevantServices(NodeId id, bool relevant);
void UpdateRelaysTxs(NodeId id, bool relay);
void UpdateLoadedBloomFilter(NodeId id, bool loaded);
void UpdateSuccessfullyConnected(NodeId id, bool connected);
void UpdateBlocksInFlight(NodeId id, bool add);
void UpdateLastBlockAnnouncementTime(NodeId id, int64_t time);
void UpdateSlowChainProtected(NodeId id, bool is_protected);

};

#endif // BITCOIN_EVICTION_H
