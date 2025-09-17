# Copyright (c) 2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

@0xf2c5cfa319406aa6;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/echo.h");
$Proxy.include("interfaces/init.h");
$Proxy.include("interfaces/mining.h");
$Proxy.include("interfaces/net.h");
$Proxy.includeTypes("ipc/capnp/init-types.h");
$Proxy.includeTypes("ipc/capnp/p2p-types.h");

using Echo = import "echo.capnp";
using Mining = import "mining.capnp";
using P2P = import"p2p.capnp";

interface Init $Proxy.wrap("interfaces::Init") {
    construct @0 (threadMap: Proxy.ThreadMap) -> (threadMap :Proxy.ThreadMap);
    makeEcho @1 (context :Proxy.Context) -> (result :Echo.Echo);
    makeMining @2 (context :Proxy.Context) -> (result :Mining.Mining);
    makePeerMan @3 (context :Proxy.Context) -> (result :P2P.NetEventsInterface);
    makeConnman @4 (context : Proxy.Context) -> (result :P2P.NetManagerEvents);
}
