// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrdb.h>
#include <addrman.h>
#include <common/args.h>
#include <common/system.h>
#include <common/init.h>
#include <common/messages.h>
#include <init/common.h>
#include <init/p2p.h>
#include <netgroup.h>
#include <random.h>
#include <util/translation.h>
#include <logging.h>
#include <node/interface_ui.h>
#include <mapport.h>
#include <net.h>
#include <noui.h>
#include <banman.h>
#include <scheduler.h>
#include <util/thread.h>
#include <chainparamsbase.h>
#include <chainparams.h>
#include <clientversion.h>
#include <util/exception.h>
#include <util/asmap.h>
#include <util/threadnames.h>
#include <util/tokenpipe.h>
#include <util/signalinterrupt.h>
#include <util/syserror.h>
#include <torcontrol.h>

#ifndef WIN32
#include <csignal>
#include <sys/stat.h>
#endif

#include <fstream>

using common::ResolveErrMsg;

static const char* DEFAULT_ASMAP_FILENAME="ip_asn.map";

//! Default value for -daemon option
static constexpr bool DEFAULT_DAEMON = false;
//! Default value for -daemonwait option
static constexpr bool DEFAULT_DAEMONWAIT = false;

const TranslateFn G_TRANSLATION_FUN{nullptr};

/**
 * The PID file facilities.
 */
static const char* BITCOIN_PID_FILENAME = "bitcoin-p2p.pid";
/**
 * True if this process has created a PID file.
 * Used to determine whether we should remove the PID file on shutdown.
 */
static bool g_generated_pid{false};


class IPCMsgProc final : public NetEventsInterface
{
    /** Initialize a peer (setup state) */
    std::optional<NodeId> InitializeNode(PeerOptions options) override
    {
        printf("connected to node\n");
        return std::nullopt;
    }

    /** Handle removal of a peer (clear state) */
    void MarkNodeDisconnected(NodeId) override
    {
    }

    void MarkSendBufferFull(NodeId, bool) override
    {
    }

    /**
     * Callback to determine whether the given set of service flags are sufficient
     * for a peer to be "relevant".
     */
    bool HasAllDesirableServiceFlags(ServiceFlags services) const override
    {
        return true;
    }

    void WakeMessageHandler() override
    {
    }
};

static std::unique_ptr<NetGroupManager> netgroupman;
static std::unique_ptr<AddrMan> addrman;
static std::unique_ptr<CConnman> connman;
static std::unique_ptr<BanMan> banman;
static std::unique_ptr<IPCMsgProc> peerman;
static std::unique_ptr<CScheduler> scheduler;
static std::unique_ptr<ECC_Context> ecc_context;

static fs::path GetPidFile(const ArgsManager& args)
{
    return AbsPathForConfigVal(args, args.GetPathArg("-pid", BITCOIN_PID_FILENAME));
}

[[nodiscard]] static bool CreatePidFile(const ArgsManager& args)
{
    if (args.IsArgNegated("-pid")) return true;

    std::ofstream file{GetPidFile(args)};
    if (file) {
#ifdef WIN32
        tfm::format(file, "%d\n", GetCurrentProcessId());
#else
        tfm::format(file, "%d\n", getpid());
#endif
        g_generated_pid = true;
        return true;
    } else {
        return InitError(strprintf(_("Unable to create the PID file '%s': %s"), fs::PathToString(GetPidFile(args)), SysErrorString(errno)));
    }
}

static void RemovePidFile(const ArgsManager& args)
{
    if (!g_generated_pid) return;
    const auto pid_path{GetPidFile(args)};
    if (std::error_code error; !fs::remove(pid_path, error)) {
        std::string msg{error ? error.message() : "File does not exist"};
        LogPrintf("Unable to remove PID file (%s): %s\n", fs::PathToString(pid_path), msg);
    }
}

[[noreturn]] static void new_handler_terminate()
{
    // Rather than throwing std::bad-alloc if allocation fails, terminate
    // immediately to (try to) avoid chain corruption.
    // Since logging may itself allocate memory, set the handler directly
    // to terminate first.
    std::set_new_handler(std::terminate);
    LogError("Out of memory. Terminating.\n");

    // The log was successful, terminate now.
    std::terminate();
};

static std::optional<util::SignalInterrupt> g_shutdown;

/**
 * Signal handlers are very limited in what they are allowed to do.
 * The execution context the handler is invoked in is not guaranteed,
 * so we restrict handler operations to just touching variables:
 */
#ifndef WIN32
static void HandleSIGTERM(int)
{
    // Return value is intentionally ignored because there is not a better way
    // of handling this failure in a signal handler.
    (void)(*Assert(g_shutdown))();
}

static void HandleSIGHUP(int)
{
    LogInstance().m_reopen_file = true;
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    if (!(*Assert(g_shutdown))()) {
        LogError("Failed to send shutdown signal on Ctrl-C\n");
        return false;
    }
    Sleep(INFINITE);
    return true;
}
#endif

#ifndef WIN32
static void registerSignalHandler(int signal, void(*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}
#endif

/** Custom implementation of daemon(). This implements the same order of operations as glibc.
 * Opens a pipe to the child process to be able to wait for an event to occur.
 *
 * @returns 0 if successful, and in child process.
 *          >0 if successful, and in parent process.
 *          -1 in case of error (in parent process).
 *
 *          In case of success, endpoint will be one end of a pipe from the child to parent process,
 *          which can be used with TokenWrite (in the child) or TokenRead (in the parent).
 */
static int fork_daemon(bool nochdir, bool noclose, TokenPipeEnd& endpoint)
{
    // communication pipe with child process
    std::optional<TokenPipe> umbilical = TokenPipe::Make();
    if (!umbilical) {
        return -1; // pipe or pipe2 failed.
    }

    int pid = fork();
    if (pid < 0) {
        return -1; // fork failed.
    }
    if (pid != 0) {
        // Parent process gets read end, closes write end.
        endpoint = umbilical->TakeReadEnd();
        umbilical->TakeWriteEnd().Close();

        int status = endpoint.TokenRead();
        if (status != 0) { // Something went wrong while setting up child process.
            endpoint.Close();
            return -1;
        }

        return pid;
    }
    // Child process gets write end, closes read end.
    endpoint = umbilical->TakeWriteEnd();
    umbilical->TakeReadEnd().Close();

#if HAVE_DECL_SETSID
    if (setsid() < 0) {
        exit(1); // setsid failed.
    }
#endif

    if (!nochdir) {
        if (chdir("/") != 0) {
            exit(1); // chdir failed.
        }
    }
    if (!noclose) {
        // Open /dev/null, and clone it into STDIN, STDOUT and STDERR to detach
        // from terminal.
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            bool err = dup2(fd, STDIN_FILENO) < 0 || dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0;
            // Don't close if fd<=2 to try to handle the case where the program was invoked without any file descriptors open.
            if (fd > 2) close(fd);
            if (err) {
                exit(1); // dup2 failed.
            }
        } else {
            exit(1); // open /dev/null failed.
        }
    }
    endpoint.TokenWrite(0); // Success
    return 0;
}

static void SetupP2PArgs(ArgsManager& argsman)
{
    SetupHelpOptions(argsman);
    init::AddLoggingArgs(argsman);
    argsman.AddArg("-version", "Print version and exit", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);

#if HAVE_DECL_FORK
    argsman.AddArg("-daemon", strprintf("Run in the background as a daemon and accept commands (default: %d)", DEFAULT_DAEMON), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    argsman.AddArg("-daemonwait", strprintf("Wait for initialization to be finished before exiting. This implies -daemon (default: %d)", DEFAULT_DAEMONWAIT), ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
#else
    std::vector<std::string> hidden_args = {
    hidden_args.emplace_back("-daemon");
    hidden_args.emplace_back("-daemonwait");

    // Add the hidden options
    argsman.AddHiddenArgs(hidden_args);
#endif

    AddP2POptions(argsman);
    SetupChainParamsBaseOptions(argsman);
}

static void InitLogging(const ArgsManager& args)
{
    init::SetLoggingOptions(args);
    init::LogPackageVersion();
}

static bool ParseArgs(ArgsManager& args, int argc, char* argv[])
{
    std::string error;
    SetupP2PArgs(args);
    if (!args.ParseParameters(argc, argv, error)) {
        return InitError(Untranslated(strprintf("Error parsing command line arguments: %s", error)));
    }

    if (auto error = common::InitConfig(args)) {
        return InitError(error->message, error->details);
    }

    // Error out when loose non-argument tokens are encountered on command line
    for (int i = 1; i < argc; i++) {
        if (!IsSwitchChar(argv[i][0])) {
            return InitError(Untranslated(strprintf("Command line contains unexpected token '%s', see bitcoind -h for a list of options.", argv[i])));
        }
    }
    return true;
}

static bool ProcessInitCommands(ArgsManager& args)
{
    // Process help and version before taking care about datadir
    if (HelpRequested(args) || args.GetBoolArg("-version", false)) {
        std::string strUsage = CLIENT_NAME " p2p version " + FormatFullVersion() + "\n";

        if (args.GetBoolArg("-version", false)) {
            strUsage += FormatParagraph(LicenseInfo());
        } else {
            strUsage += "\n"
                "Usage: bitcoin-p2p [options]\n"
                "\n";
            strUsage += args.GetHelpMessage();
        }

        tfm::format(std::cout, "%s", strUsage);
        return true;
    }

    return false;
}

static bool AppInitP2PBasicSetup(const ArgsManager& args)
{
    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable heap terminate-on-corruption
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
#endif
    if (!SetupNetworking()) {
        return InitError(Untranslated("Initializing networking failed."));
    }

#ifndef WIN32
    // Clean shutdown on SIGTERM
    registerSignalHandler(SIGTERM, HandleSIGTERM);
    registerSignalHandler(SIGINT, HandleSIGTERM);

    // Reopen debug.log on SIGHUP
    registerSignalHandler(SIGHUP, HandleSIGHUP);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#else
    SetConsoleCtrlHandler(consoleCtrlHandler, true);
#endif

    std::set_new_handler(new_handler_terminate);

    return true;
}

static bool AppInitSanityChecks()
{
    // ********************************************************* Step 4: sanity checks
    if (!Random_SanityCheck()) {
        return InitError(Untranslated("OS cryptographic RNG sanity check failure. Aborting."));
    }

    if (!ECC_InitSanityCheck()) {
        return InitError(strprintf(_("Elliptic curve cryptography sanity check failure. %s is shutting down."), CLIENT_NAME));
    }

    //TODO: p2p-specific file locks
    return true;
}

static bool AppInitP2P(const ArgsManager& args)
{
    // ********************************************************* Step 4a: application initialization
    if (!CreatePidFile(args)) {
        // Detailed error printed inside CreatePidFile().
        return false;
    }
    if (!init::StartLogging(args)) {
        // Detailed error printed inside StartLogging().
        return false;
    }

    // Warn about relative -datadir path.
    if (args.IsArgSet("-datadir") && !args.GetPathArg("-datadir").is_absolute()) {
        LogPrintf("Warning: relative datadir option '%s' specified, which will be interpreted relative to the "
                  "current working directory '%s'. This is fragile, because if bitcoin is started in the future "
                  "from a different location, it will be unable to locate the current data files. There could "
                  "also be data loss if bitcoin is started while in a temporary directory.\n",
                  args.GetArg("-datadir", ""), fs::PathToString(fs::current_path()));
    }

    assert(!scheduler);
    scheduler = std::make_unique<CScheduler>();

    // Start the lightweight task scheduler thread
    scheduler->m_service_thread = std::thread(util::TraceThread, "scheduler", [&] { scheduler->serviceQueue(); });

    // Gather some entropy once per minute.
    scheduler->scheduleEvery([]{
        RandAddPeriodic();
    }, std::chrono::minutes{1});

    const auto onlynets = args.GetArgs("-onlynet");
    if (!onlynets.empty()) {
        g_reachable_nets.RemoveAll();
        for (const std::string& snet : onlynets) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(_("Unknown network specified in -onlynet: '%s'"), snet));
            g_reachable_nets.Add(net);
        }
    }

    if (!args.IsArgSet("-cjdnsreachable")) {
        if (!onlynets.empty() && g_reachable_nets.Contains(NET_CJDNS)) {
            return InitError(
                _("Outbound connections restricted to CJDNS (-onlynet=cjdns) but "
                  "-cjdnsreachable is not provided"));
        }
        g_reachable_nets.Remove(NET_CJDNS);
    }

    // ********************************************************* Step 6: network initialization
    // Note that we absolutely cannot open any actual connections
    // until the very end ("start node") as the UTXO/block state
    // is not yet setup and may end up being set up twice if we
    // need to reindex later.

    fListen = args.GetBoolArg("-listen", DEFAULT_LISTEN);
    fDiscover = args.GetBoolArg("-discover", true);

    //CScheduler scheduler;
    {

        // Read asmap file if configured
        std::vector<bool> asmap;
        if (args.IsArgSet("-asmap") && !args.IsArgNegated("-asmap")) {
            fs::path asmap_path = args.GetPathArg("-asmap", DEFAULT_ASMAP_FILENAME);
            if (!asmap_path.is_absolute()) {
                asmap_path = args.GetDataDirNet() / asmap_path;
            }
            if (!fs::exists(asmap_path)) {
                return InitError(strprintf(_("Could not find asmap file %s"), fs::quoted(fs::PathToString(asmap_path))));
            }
            asmap = DecodeAsmap(asmap_path);
            if (asmap.size() == 0) {
                return InitError(strprintf(_("Could not parse asmap file %s"), fs::quoted(fs::PathToString(asmap_path))));
            }
            const uint256 asmap_version = (HashWriter{} << asmap).GetHash();
            LogPrintf("Using asmap version %s for IP bucketing\n", asmap_version.ToString());
        } else {
            LogPrintf("Using /16 prefix for IP bucketing\n");
        }

        // Initialize netgroup manager
        netgroupman = std::make_unique<NetGroupManager>(std::move(asmap));

        // Initialize addrman
        uiInterface.InitMessage(_("Loading P2P addresses…"));
        auto load_addrman{LoadAddrman(*netgroupman, args)};
        if (!load_addrman) {
            return InitError(util::ErrorString(load_addrman));
        }
        addrman = std::move(*load_addrman);
    }

    FastRandomContext rng;
    connman = std::make_unique<CConnman>(rng.rand64(),rng.rand64(), *addrman, *netgroupman, true);
    banman = std::make_unique<BanMan>(args.GetDataDirNet() / "banlist", &uiInterface, args.GetIntArg("-bantime", DEFAULT_MISBEHAVING_BANTIME));

    peerman = std::make_unique<IPCMsgProc>();

    for (const std::string& socket_addr : args.GetArgs("-bind")) {
        std::string host_out;
        uint16_t port_out{0};
        std::string bind_socket_addr = socket_addr.substr(0, socket_addr.rfind('='));
        if (!SplitHostPort(bind_socket_addr, port_out, host_out)) {
            //return InitError(InvalidPortErrMsg("-bind", socket_addr));
        }
    }

    // Requesting DNS seeds entails connecting to IPv4/IPv6, which -onlynet options may prohibit:
    // If -dnsseed=1 is explicitly specified, abort. If it's left unspecified by the user, we skip
    // the DNS seeds by adjusting -dnsseed in InitParameterInteraction.
    if (args.GetBoolArg("-dnsseed") == true && !g_reachable_nets.Contains(NET_IPV4) && !g_reachable_nets.Contains(NET_IPV6)) {
        return InitError(strprintf(_("Incompatible options: -dnsseed=1 was explicitly specified, but -onlynet forbids connections to IPv4/IPv6")));
    };

    // Check for host lookup allowed before parsing any network related parameters
    fNameLookup = args.GetBoolArg("-dns", DEFAULT_NAME_LOOKUP);

    bool proxyRandomize = args.GetBoolArg("-proxyrandomize", DEFAULT_PROXYRANDOMIZE);
    // -proxy sets a proxy for outgoing network traffic, possibly per network.
    // -noproxy, -proxy=0 or -proxy="" can be used to remove the proxy setting, this is the default
    Proxy ipv4_proxy;
    Proxy ipv6_proxy;
    Proxy onion_proxy;
    Proxy name_proxy;
    Proxy cjdns_proxy;
    for (const std::string& param_value : args.GetArgs("-proxy")) {
        const auto eq_pos{param_value.rfind('=')};
        const std::string proxy_str{param_value.substr(0, eq_pos)}; // e.g. 127.0.0.1:9050=ipv4 -> 127.0.0.1:9050
        std::string net_str;
        if (eq_pos != std::string::npos) {
            if (eq_pos + 1 == param_value.length()) {
                return InitError(strprintf(_("Invalid -proxy address or hostname, ends with '=': '%s'"), param_value));
            }
            net_str = ToLower(param_value.substr(eq_pos + 1)); // e.g. 127.0.0.1:9050=ipv4 -> ipv4
        }

        Proxy proxy;
        if (!proxy_str.empty() && proxy_str != "0") {
            if (IsUnixSocketPath(proxy_str)) {
                proxy = Proxy{proxy_str, /*tor_stream_isolation=*/proxyRandomize};
            } else {
                const std::optional<CService> addr{Lookup(proxy_str, DEFAULT_TOR_SOCKS_PORT, fNameLookup)};
                if (!addr.has_value()) {
                    return InitError(strprintf(_("Invalid -proxy address or hostname: '%s'"), proxy_str));
                }
                proxy = Proxy{addr.value(), /*tor_stream_isolation=*/proxyRandomize};
            }
            if (!proxy.IsValid()) {
                return InitError(strprintf(_("Invalid -proxy address or hostname: '%s'"), proxy_str));
            }
        }

        if (net_str.empty()) { // For all networks.
            ipv4_proxy = ipv6_proxy = name_proxy = cjdns_proxy = onion_proxy = proxy;
        } else if (net_str == "ipv4") {
            ipv4_proxy = name_proxy = proxy;
        } else if (net_str == "ipv6") {
            ipv6_proxy = name_proxy = proxy;
        } else if (net_str == "tor" || net_str == "onion") {
            onion_proxy = proxy;
        } else if (net_str == "cjdns") {
            cjdns_proxy = proxy;
        } else {
            return InitError(strprintf(_("Unrecognized network in -proxy='%s': '%s'"), param_value, net_str));
        }
    }
    if (ipv4_proxy.IsValid()) {
        SetProxy(NET_IPV4, ipv4_proxy);
    }
    if (ipv6_proxy.IsValid()) {
        SetProxy(NET_IPV6, ipv6_proxy);
    }
    if (name_proxy.IsValid()) {
        SetNameProxy(name_proxy);
    }
    if (cjdns_proxy.IsValid()) {
        SetProxy(NET_CJDNS, cjdns_proxy);
    }

    const bool onlynet_used_with_onion{!onlynets.empty() && g_reachable_nets.Contains(NET_ONION)};

    // -onion can be used to set only a proxy for .onion, or override normal proxy for .onion addresses
    // -noonion (or -onion=0) disables connecting to .onion entirely
    // An empty string is used to not override the onion proxy (in which case it defaults to -proxy set above, or none)
    std::string onionArg = args.GetArg("-onion", "");
    if (onionArg != "") {
        if (onionArg == "0") { // Handle -noonion/-onion=0
            onion_proxy = Proxy{};
            if (onlynet_used_with_onion) {
                return InitError(
                    _("Outbound connections restricted to Tor (-onlynet=onion) but the proxy for "
                      "reaching the Tor network is explicitly forbidden: -onion=0"));
            }
        } else {
            if (IsUnixSocketPath(onionArg)) {
                onion_proxy = Proxy(onionArg, /*tor_stream_isolation=*/proxyRandomize);
            } else {
                const std::optional<CService> addr{Lookup(onionArg, DEFAULT_TOR_SOCKS_PORT, fNameLookup)};
                if (!addr.has_value() || !addr->IsValid()) {
                    return InitError(strprintf(_("Invalid -onion address or hostname: '%s'"), onionArg));
                }

                onion_proxy = Proxy(addr.value(), /*tor_stream_isolation=*/proxyRandomize);
            }
        }
    }

    if (onion_proxy.IsValid()) {
        SetProxy(NET_ONION, onion_proxy);
    } else {
        // If -listenonion is set, then we will (try to) connect to the Tor control port
        // later from the torcontrol thread and may retrieve the onion proxy from there.
        const bool listenonion_disabled{!args.GetBoolArg("-listenonion", DEFAULT_LISTEN_ONION)};
        if (onlynet_used_with_onion && listenonion_disabled) {
            return InitError(
                _("Outbound connections restricted to Tor (-onlynet=onion) but the proxy for "
                  "reaching the Tor network is not provided: none of -proxy, -onion or "
                  "-listenonion is given"));
        }
        g_reachable_nets.Remove(NET_ONION);
    }


    CConnman::Options connOptions;
    connOptions.uiInterface = &uiInterface;
    connOptions.m_banman = banman.get();
    connOptions.m_msgproc = peerman.get();
    if (!CreateP2POptions(args, connOptions)) return InitError(Untranslated("Failed to create CConnman options"));

    auto listen_port = connOptions.default_listen_port;

    for (const std::string& strAddr : args.GetArgs("-externalip")) {
        const std::optional<CService> addrLocal{Lookup(strAddr, listen_port, fNameLookup)};
        if (addrLocal.has_value() && addrLocal->IsValid())
            AddLocal(addrLocal.value(), LOCAL_MANUAL);
        else
            return InitError(ResolveErrMsg("externalip", strAddr));
    }

    InitializeMapPort(listen_port);
    StartMapPort(args.GetBoolArg("-natpmp", DEFAULT_NATPMP));

    CService onion_service_target;
    if (!connOptions.onion_binds.empty()) {
        onion_service_target = connOptions.onion_binds.front();
    } else if (!connOptions.vBinds.empty()) {
        onion_service_target = connOptions.vBinds.front();
    }

    if (args.GetBoolArg("-listenonion", DEFAULT_LISTEN_ONION)) {
        if (connOptions.onion_binds.size() > 1) {
            InitWarning(strprintf(_("More than one onion bind address is provided. Using %s "
                                    "for the automatically created Tor onion service."),
                                  onion_service_target.ToStringAddrPort()));
        }
        StartTorControl(onion_service_target);
    }

    const std::string& i2psam_arg = args.GetArg("-i2psam", "");
    if (!i2psam_arg.empty()) {
        const std::optional<CService> addr{Lookup(i2psam_arg, 7656, fNameLookup)};
        if (!addr.has_value() || !addr->IsValid()) {
            return InitError(strprintf(_("Invalid -i2psam address or hostname: '%s'"), i2psam_arg));
        }
        SetProxy(NET_I2P, Proxy{addr.value()});
    } else {
        if (!onlynets.empty() && g_reachable_nets.Contains(NET_I2P)) {
            return InitError(
                _("Outbound connections restricted to i2p (-onlynet=i2p) but "
                  "-i2psam is not provided"));
        }
        g_reachable_nets.Remove(NET_I2P);
    }

    if (connOptions.bind_on_any) {
        // Only add all IP addresses of the machine if we would be listening on
        // any address - 0.0.0.0 (IPv4) and :: (IPv6).
        Discover(listen_port);
    }

    if (!connman->Start(*scheduler, connOptions)) {
        return false;
    }

    uiInterface.InitMessage(_("Done loading"));

    scheduler->scheduleEvery([]{
        banman->DumpBanlist();
    }, DUMP_BANS_INTERVAL);

    return true;
}

static bool AppInit(ArgsManager& args)
{
    bool fRet = false;

#if HAVE_DECL_FORK
    // Communication with parent after daemonizing. This is used for signalling in the following ways:
    // - a boolean token is sent when the initialization process (all the Init* functions) have finished to indicate
    // that the parent process can quit, and whether it was successful/unsuccessful.
    // - an unexpected shutdown of the child process creates an unexpected end of stream at the parent
    // end, which is interpreted as failure to start.
    TokenPipeEnd daemon_ep;
#endif
    try
    {
        // Set this early so that parameter interactions go to console
        InitLogging(args);
        InitP2PParameterInteraction(args);
        if (!AppInitP2PBasicSetup(args)) {
            // InitError will have been called with detailed error, which ends up on console
            return false;
        }
        if (!AppInitP2PParameterInteraction(args, 0)) {
            // InitError will have been called with detailed error, which ends up on console
            return false;
        }

        ecc_context = std::make_unique<ECC_Context>();

        if (!AppInitSanityChecks())
        {
            // InitError will have been called with detailed error, which ends up on console
            return false;
        }

        if (args.GetBoolArg("-daemon", DEFAULT_DAEMON) || args.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT)) {
#if HAVE_DECL_FORK
            tfm::format(std::cout, CLIENT_NAME " starting\n");

            // Daemonize
            switch (fork_daemon(1, 0, daemon_ep)) { // don't chdir (1), do close FDs (0)
            case 0: // Child: continue.
                // If -daemonwait is not enabled, immediately send a success token the parent.
                if (!args.GetBoolArg("-daemonwait", DEFAULT_DAEMONWAIT)) {
                    daemon_ep.TokenWrite(1);
                    daemon_ep.Close();
                }
                break;
            case -1: // Error happened.
                return InitError(Untranslated(strprintf("fork_daemon() failed: %s", SysErrorString(errno))));
            default: { // Parent: wait and exit.
                int token = daemon_ep.TokenRead();
                if (token) { // Success
                    exit(EXIT_SUCCESS);
                } else { // fRet = false or token read error (premature exit).
                    tfm::format(std::cerr, "Error during initialization - check debug.log for details\n");
                    exit(EXIT_FAILURE);
                }
            }
            }
#else
            return InitError(Untranslated("-daemon is not supported on this operating system"));
#endif // HAVE_DECL_FORK
        }
        fRet = AppInitP2P(args);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

#if HAVE_DECL_FORK
    if (daemon_ep.IsOpen()) {
        // Signal initialization status to parent, then close pipe.
        daemon_ep.TokenWrite(fRet);
        daemon_ep.Close();
    }
#endif
    return fRet;
}

static void Interrupt()
{
    InterruptTorControl();
    InterruptMapPort();
    if (connman)
        connman->Interrupt();
}

static void Shutdown(ArgsManager& args)
{
    static Mutex g_shutdown_mutex;
    TRY_LOCK(g_shutdown_mutex, lock_shutdown);
    if (!lock_shutdown) return;
    LogPrintf("%s: In progress...\n", __func__);

    /// Note: Shutdown() must be able to handle cases in which initialization failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    util::ThreadRename("shutoff");

    StopMapPort();

    connman->Stop();

    StopTorControl();

    if (scheduler) scheduler->stop();

    // After the threads that potentially access these pointers have been stopped,
    // destruct and reset all to nullptr.
    peerman.reset();
    connman.reset();
    banman.reset();
    addrman.reset();
    netgroupman.reset();
    scheduler.reset();
    ecc_context.reset();
    RemovePidFile(args);

    LogPrintf("%s: done\n", __func__);
}

MAIN_FUNCTION
{
#ifdef WIN32
    common::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif

    int exit_status = EXIT_SUCCESS;

    SetupEnvironment();

    g_shutdown.emplace();
    // Connect bitcoind signal handlers
    noui_connect();

    util::ThreadSetInternalName("init");

    // Interpret command line arguments
    ArgsManager& args(gArgs);
    if (!ParseArgs(args, argc, argv)) return EXIT_FAILURE;
    // Process early info return commands such as -help or -version
    if (ProcessInitCommands(args)) return EXIT_SUCCESS;

    // Start application
    if (!AppInit(args) || !Assert(g_shutdown->wait())) {
        exit_status = EXIT_FAILURE;
    }
    Interrupt();
    Shutdown(args);
    return exit_status;
}
#if 0
int main(int argc, char* argv[])
{
#ifdef WIN32
    common::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();
    if (!SetupNetworking()) {
        tfm::format(std::cerr, "Error: Initializing networking failed\n");
        return EXIT_FAILURE;
    }

    try {
        int ret = AppInitP2P(args, argc, argv);
        if (ret != CONTINUE_EXECUTION)
            return ret;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInitRPC()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInitRPC()");
        return EXIT_FAILURE;
    }
    init::SetLoggingOptions(args);
    init::LogPackageVersion();
    if (!init::StartLogging(args)) {
        // Detailed error printed inside StartLogging().
        return EXIT_FAILURE;
    }

    if (auto result{init::SetLoggingCategories(args)}; !result) return InitError(util::ErrorString(result));
    if (auto result{init::SetLoggingLevel(args)}; !result) return InitError(util::ErrorString(result));

    InitP2PParameterInteraction(args);



    while(true) {
        std::this_thread::sleep_for(1s);
    }
}
#endif
