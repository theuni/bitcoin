# Copyright (c) 2024 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

@0xc77d03df6a41b506;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Common = import "common.capnp";
using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/net.h");
$Proxy.includeTypes("ipc/capnp/p2p-types.h");

interface NetEventsInterface $Proxy.wrap("interfaces::NetEventsInterface") {
    destroy @0 (context :Proxy.Context) -> ();
    initializeNode @1 (options: PeerOptions) -> (result: Int64, hasResult: Bool);
    markNodeDisconnected @2 (context :Proxy.Context, node_id: Int64) -> ();
    markSendBufferFull @3 (context :Proxy.Context, node_id: Int64, full: Bool) -> ();
    hasAllDesirableServiceFlags @4 (context :Proxy.Context, serviceFlags: UInt64) -> (result: Bool);
    wakeMessageHandler @5 (context :Proxy.Context) -> ();
}

interface NetManagerEvents $Proxy.wrap("interfaces::NetManagerEvents") {
    destroy @0 (context :Proxy.Context) -> ();
    pushMessage @1 (context: Proxy.Context, node_id: Int64, message: Data) -> (result: Bool);
    disconnectNode @2 (context: Proxy.Context, node_id: Int64) -> (result: Bool);
    outboundTargetReached @3 (context: Proxy.Context, historicalBlockServingLimit: Bool) -> (result: Bool);
    getAddresses @4 (context: Proxy.Context, maxAddresses: UInt64, maxPercent: UInt64) -> (result: List(Data));
    getAddressesForPeer @5 (context: Proxy.Context, node_id: Int64, maxAddresses: UInt64, maxPercent: UInt64) -> (result: List(Data));
    setBootstrapComplete @6 (context: Proxy.Context) -> ();
    pollMessage @7 (context: Proxy.Context, node_id: Int64) -> (result: PollMessageResult, hasResult: Bool);
    setTryNewOutboundPeer @8 (context: Proxy.Context, val: Bool) -> (result: Bool);
    startExtraBlockRelayPeers @9 (context: Proxy.Context) -> ();
    getLocalAddrForPeer @10 (context: Proxy.Context, node_id: Int64, addrLocal: Data) -> (result: Data, hasResult: Bool);
    seenLocal @11 (context: Proxy.Context, addrLocal: Data) -> (result: Bool);
}

struct PeerOptions $Proxy.wrap("PeerOptions") {
    connectionType @0 :Int32 $Proxy.name("conn_type");
    address @1 : Data $Proxy.name("addr");
    addrName @2 : Text $Proxy.name("addr_name");
    permissionFlags @3 : UInt32 $Proxy.name("permission_flags");
    timeConnected @4 : Int64 $Proxy.name("connected");
    inboundOnion @5 : Bool $Proxy.name("inbound_onion");
    mappedAs @6 : UInt32 $Proxy.name("mapped_as");
    keyedNetGroup @7 : UInt64 $Proxy.name("keyed_net_group");
    connectedThroughNet @8 : Int32 $Proxy.name("connected_through_net");
    sendLocalAddress @9 : Bool $Proxy.name("send_local_address");
}

struct PollMessageResult $Proxy.wrap("interfaces::PollMessageResult") {
    message @0 : Data $Proxy.name("m_recv");
    timeReceived @1 : Int64 $Proxy.name("m_time");
    messageSize @2 : UInt32 $Proxy.name("m_message_size");
    messageSizeRaw @3 : UInt32 $Proxy.name("m_raw_message_size");
    messageType @4 : Text $Proxy.name("m_type");
    moreMessages @5 : Bool $Proxy.name("m_more");
}
