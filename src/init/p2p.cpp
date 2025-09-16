// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/args.h>
#include <init/p2p.h>
#include <net.h>
#include <banman.h>
#include <chainparamsbase.h>
#include <chainparams.h>
#include <torcontrol.h>
#include <mapport.h>
#include <addrman.h>

static const char* DEFAULT_ASMAP_FILENAME="ip_asn.map";

using util::Join;

void AddP2POptions(ArgsManager& argsman)
{

    const auto defaultBaseParams = CreateBaseChainParams(ChainType::MAIN);
    const auto testnetBaseParams = CreateBaseChainParams(ChainType::TESTNET);
    const auto testnet4BaseParams = CreateBaseChainParams(ChainType::TESTNET4);
    const auto signetBaseParams = CreateBaseChainParams(ChainType::SIGNET);
    const auto regtestBaseParams = CreateBaseChainParams(ChainType::REGTEST);
    const auto defaultChainParams = CreateChainParams(argsman, ChainType::MAIN);
    const auto testnetChainParams = CreateChainParams(argsman, ChainType::TESTNET);
    const auto testnet4ChainParams = CreateChainParams(argsman, ChainType::TESTNET4);
    const auto signetChainParams = CreateChainParams(argsman, ChainType::SIGNET);
    const auto regtestChainParams = CreateChainParams(argsman, ChainType::REGTEST);

    argsman.AddArg("-addnode=<ip>", strprintf("Add a node to connect to and attempt to keep the connection open (see the addnode RPC help for more info). This option can be specified multiple times to add multiple nodes; connections are limited to %u at a time and are counted separately from the -maxconnections limit.", MAX_ADDNODE_CONNECTIONS), ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    argsman.AddArg("-asmap=<file>", strprintf("Specify asn mapping used for bucketing of the peers (default: %s). Relative paths will be prefixed by the net-specific datadir location.", DEFAULT_ASMAP_FILENAME), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-bantime=<n>", strprintf("Default duration (in seconds) of manually configured bans (default: %u)", DEFAULT_MISBEHAVING_BANTIME), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-bind=<addr>[:<port>][=onion]", strprintf("Bind to given address and always listen on it (default: 0.0.0.0). Use [host]:port notation for IPv6. Append =onion to tag any incoming connections to that address and port as incoming Tor connections (default: 127.0.0.1:%u=onion, testnet3: 127.0.0.1:%u=onion, testnet4: 127.0.0.1:%u=onion, signet: 127.0.0.1:%u=onion, regtest: 127.0.0.1:%u=onion)", defaultChainParams->GetDefaultPort() + 1, testnetChainParams->GetDefaultPort() + 1, testnet4ChainParams->GetDefaultPort() + 1, signetChainParams->GetDefaultPort() + 1, regtestChainParams->GetDefaultPort() + 1), ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    argsman.AddArg("-cjdnsreachable", "If set, then this host is configured for CJDNS (connecting to fc00::/8 addresses would lead us to the CJDNS network, see doc/cjdns.md) (default: 0)", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-connect=<ip>", "Connect only to the specified node; -noconnect disables automatic connections (the rules for this peer are the same as for -addnode). This option can be specified multiple times to connect to multiple nodes.", ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    argsman.AddArg("-discover", "Discover own IP addresses (default: 1 when listening and no -externalip or -proxy)", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-dns", strprintf("Allow DNS lookups for -addnode, -seednode and -connect (default: %u)", DEFAULT_NAME_LOOKUP), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-dnsseed", strprintf("Query for peer addresses via DNS lookup, if low on addresses (default: %u unless -connect used or -maxconnections=0)", DEFAULT_DNSSEED), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-externalip=<ip>", "Specify your own public address", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-fixedseeds", strprintf("Allow fixed seeds if DNS seeds don't provide peers (default: %u)", DEFAULT_FIXEDSEEDS), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-forcednsseed", strprintf("Always query for peer addresses via DNS lookup (default: %u)", DEFAULT_FORCEDNSSEED), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-listen", strprintf("Accept connections from outside (default: %u if no -proxy, -connect or -maxconnections=0)", DEFAULT_LISTEN), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-listenonion", strprintf("Automatically create Tor onion service (default: %d)", DEFAULT_LISTEN_ONION), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxconnections=<n>", strprintf("Maintain at most <n> automatic connections to peers (default: %u). This limit does not apply to connections manually added via -addnode or the addnode RPC, which have a separate limit of %u.", DEFAULT_MAX_PEER_CONNECTIONS, MAX_ADDNODE_CONNECTIONS), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxreceivebuffer=<n>", strprintf("Maximum per-connection receive buffer, <n>*1000 bytes (default: %u)", DEFAULT_MAXRECEIVEBUFFER), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxsendbuffer=<n>", strprintf("Maximum per-connection memory usage for the send buffer, <n>*1000 bytes (default: %u)", DEFAULT_MAXSENDBUFFER), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-maxuploadtarget=<n>", strprintf("Tries to keep outbound traffic under the given target per 24h. Limit does not apply to peers with 'download' permission or blocks created within past week. 0 = no limit (default: %s). Optional suffix units [k|K|m|M|g|G|t|T] (default: M). Lowercase is 1000 base while uppercase is 1024 base", DEFAULT_MAX_UPLOAD_TARGET), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#ifdef HAVE_SOCKADDR_UN
    argsman.AddArg("-onion=<ip:port|path>", "Use separate SOCKS5 proxy to reach peers via Tor onion services, set -noonion to disable (default: -proxy). May be a local file path prefixed with 'unix:'.", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#else
    argsman.AddArg("-onion=<ip:port>", "Use separate SOCKS5 proxy to reach peers via Tor onion services, set -noonion to disable (default: -proxy)", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#endif
    argsman.AddArg("-i2psam=<ip:port>", "I2P SAM proxy to reach I2P peers and accept I2P connections (default: none)", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-i2pacceptincoming", strprintf("Whether to accept inbound I2P connections (default: %i). Ignored if -i2psam is not set. Listening for inbound I2P connections is done through the SAM proxy, not by binding to a local address and port.", DEFAULT_I2P_ACCEPT_INCOMING), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-onlynet=<net>", "Make automatic outbound connections only to network <net> (" + Join(GetNetworkNames(), ", ") + "). Inbound and manual connections are not affected by this option. It can be specified multiple times to allow multiple networks.", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-v2transport", strprintf("Support v2 transport (default: %u)", DEFAULT_V2_TRANSPORT), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-port=<port>", strprintf("Listen for connections on <port> (default: %u, testnet3: %u, testnet4: %u, signet: %u, regtest: %u). Not relevant for I2P (see doc/i2p.md). If set to a value x, the default onion listening port will be set to x+1.", defaultChainParams->GetDefaultPort(), testnetChainParams->GetDefaultPort(), testnet4ChainParams->GetDefaultPort(), signetChainParams->GetDefaultPort(), regtestChainParams->GetDefaultPort()), ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    const std::string proxy_doc_for_value =
#ifdef HAVE_SOCKADDR_UN
        "<ip>[:<port>]|unix:<path>";
#else
        "<ip>[:<port>]";
#endif
    const std::string proxy_doc_for_unix_socket =
#ifdef HAVE_SOCKADDR_UN
        "May be a local file path prefixed with 'unix:' if the proxy supports it. ";
#else
        "";
#endif
    argsman.AddArg("-proxy=" + proxy_doc_for_value + "[=<network>]",
                   "Connect through SOCKS5 proxy, set -noproxy to disable. " +
                   proxy_doc_for_unix_socket +
                   "Could end in =network to set the proxy only for that network. " +
                   "The network can be any of ipv4, ipv6, tor or cjdns. " +
                   "(default: disabled)",
                   ArgsManager::ALLOW_ANY | ArgsManager::DISALLOW_ELISION,
                   OptionsCategory::CONNECTION);
    argsman.AddArg("-proxyrandomize", strprintf("Randomize credentials for every proxy connection. This enables Tor stream isolation (default: %u)", DEFAULT_PROXYRANDOMIZE), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-seednode=<ip>", "Connect to a node to retrieve peer addresses, and disconnect. This option can be specified multiple times to connect to multiple nodes. During startup, seednodes will be tried before dnsseeds.", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-networkactive", "Enable all P2P network activity (default: 1). Can be changed by the setnetworkactive RPC command", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-timeout=<n>", strprintf("Specify socket connection timeout in milliseconds. If an initial attempt to connect is unsuccessful after this amount of time, drop it (minimum: 1, default: %d)", DEFAULT_CONNECT_TIMEOUT), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-peertimeout=<n>", strprintf("Specify a p2p connection timeout delay in seconds. After connecting to a peer, wait this amount of time before considering disconnection based on inactivity (minimum: 1, default: %d)", DEFAULT_PEER_CONNECT_TIMEOUT), ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CONNECTION);
    argsman.AddArg("-torcontrol=<ip>:<port>", strprintf("Tor control host and port to use if onion listening enabled (default: %s). If no port is specified, the default port of %i will be used.", DEFAULT_TOR_CONTROL, DEFAULT_TOR_CONTROL_PORT), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-torpassword=<pass>", "Tor control port password (default: empty)", ArgsManager::ALLOW_ANY | ArgsManager::SENSITIVE, OptionsCategory::CONNECTION);
    argsman.AddArg("-natpmp", strprintf("Use PCP or NAT-PMP to map the listening port (default: %u)", DEFAULT_NATPMP), ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-whitebind=<[permissions@]addr>", "Bind to the given address and add permission flags to the peers connecting to it. "
        "Use [host]:port notation for IPv6. Allowed permissions: " + Join(NET_PERMISSIONS_DOC, ", ") + ". "
        "Specify multiple permissions separated by commas (default: download,noban,mempool,relay). Can be specified multiple times.", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);

    argsman.AddArg("-whitelist=<[permissions@]IP address or network>", "Add permission flags to the peers using the given IP address (e.g. 1.2.3.4) or "
        "CIDR-notated network (e.g. 1.2.3.0/24). Uses the same permissions as "
        "-whitebind. "
        "Additional flags \"in\" and \"out\" control whether permissions apply to incoming connections and/or manual (default: incoming only). "
        "Can be specified multiple times.", ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    argsman.AddArg("-capturemessages", "Capture all P2P messages to disk", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    argsman.AddArg("-checkaddrman=<n>", strprintf("Run addrman consistency checks every <n> operations. Use 0 to disable. (default: %u)", DEFAULT_ADDRMAN_CONSISTENCY_CHECKS), ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST)  ;
}

// Parameter interaction based on rules
void InitP2PParameterInteraction(ArgsManager& args)
{
    // when specifying an explicit binding address, you want to listen on it
    // even when -connect or -proxy is specified
    if (!args.GetArgs("-bind").empty()) {
        if (args.SoftSetBoolArg("-listen", true))
            LogInfo("parameter interaction: -bind set -> setting -listen=1\n");
    }
    if (!args.GetArgs("-whitebind").empty()) {
        if (args.SoftSetBoolArg("-listen", true))
            LogInfo("parameter interaction: -whitebind set -> setting -listen=1\n");
    }

    if (!args.GetArgs("-connect").empty() || args.IsArgNegated("-connect") || args.GetIntArg("-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS) <= 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        // do the same when connections are disabled
        if (args.SoftSetBoolArg("-dnsseed", false))
            LogInfo("parameter interaction: -connect or -maxconnections=0 set -> setting -dnsseed=0\n");
        if (args.SoftSetBoolArg("-listen", false))
            LogInfo("parameter interaction: -connect or -maxconnections=0 set -> setting -listen=0\n");
    }

    std::string proxy_arg = args.GetArg("-proxy", "");
    if (proxy_arg != "" && proxy_arg != "0") {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (args.SoftSetBoolArg("-listen", false))
            LogInfo("parameter interaction: -proxy set -> setting -listen=0\n");
        // to protect privacy, do not map ports when a proxy is set. The user may still specify -listen=1
        // to listen locally, so don't rely on this happening through -listen below.
        if (args.SoftSetBoolArg("-natpmp", false)) {
            LogInfo("parameter interaction: -proxy set -> setting -natpmp=0\n");
        }
        // to protect privacy, do not discover addresses by default
        if (args.SoftSetBoolArg("-discover", false))
            LogInfo("parameter interaction: -proxy set -> setting -discover=0\n");
    }

    if (!args.GetBoolArg("-listen", DEFAULT_LISTEN)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        if (args.SoftSetBoolArg("-natpmp", false)) {
            LogInfo("parameter interaction: -listen=0 -> setting -natpmp=0\n");
        }
        if (args.SoftSetBoolArg("-discover", false))
            LogInfo("parameter interaction: -listen=0 -> setting -discover=0\n");
        if (args.SoftSetBoolArg("-listenonion", false))
            LogInfo("parameter interaction: -listen=0 -> setting -listenonion=0\n");
        if (args.SoftSetBoolArg("-i2pacceptincoming", false)) {
            LogInfo("parameter interaction: -listen=0 -> setting -i2pacceptincoming=0\n");
        }
    }

    if (!args.GetArgs("-externalip").empty()) {
        // if an explicit public IP is specified, do not try to find others
        if (args.SoftSetBoolArg("-discover", false))
            LogInfo("parameter interaction: -externalip set -> setting -discover=0\n");
    }

    // Forcing relay from whitelisted hosts implies we will accept relays from them in the first place.
    if (args.GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY)) {
        if (args.SoftSetBoolArg("-whitelistrelay", true))
            LogInfo("parameter interaction: -whitelistforcerelay=1 -> setting -whitelistrelay=1\n");
    }
    const auto onlynets = args.GetArgs("-onlynet");
    if (!onlynets.empty()) {
        bool clearnet_reachable = std::any_of(onlynets.begin(), onlynets.end(), [](const auto& net) {
            const auto n = ParseNetwork(net);
            return n == NET_IPV4 || n == NET_IPV6;
        });
        if (!clearnet_reachable && args.SoftSetBoolArg("-dnsseed", false)) {
            LogInfo("parameter interaction: -onlynet excludes IPv4 and IPv6 -> setting -dnsseed=0\n");
        }
    }
}
