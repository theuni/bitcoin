#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "netfuzz_connection.h"
#include "streams.h"
#include "chainparams.h"
#include "util.h"
#include "netbase.h"
#include "netaddress.h"
#include "net.h"
#include "clientversion.h"

#include <event2/event.h>

#include <signal.h>

static std::vector<CSerializedNetMsg> ReadMessages(CAutoFile&& debugFile, const CChainParams& params)
{
    std::vector<CSerializedNetMsg> ret;
    while(true)
    {
        try {
            std::vector<unsigned char> msg;
            CMessageHeader hdr(params.MessageStart());
            debugFile >> hdr;
            if (!hdr.nMessageSize || hdr.nMessageSize > 2000000)
                continue;
            msg.resize(hdr.nMessageSize);
            debugFile.read((char*)msg.data(), msg.size());
            ret.push_back({std::move(msg), hdr.pchCommand});
        }
        catch(...)
        {
            break;
        }
    }
    return ret;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();
    signal(SIGPIPE, SIG_IGN);
    ParseParameters(argc, argv);
    try {
        SelectParams(ChainNameFromCommandLine());
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }
    const CChainParams& params = Params();

    std::string messagePath = GetArg("-messagefile", "");
    if (messagePath.empty()) {
        fprintf(stderr, "Error: required argument missing: -messagefile\n");
        return EXIT_FAILURE;
    }

    static const std::string defaultIP("127.0.0.1");
    std::string remoteIP = GetArg("-connect", defaultIP);
    if (remoteIP.empty()) {
        printf("Warning: -connect not specified. Using: %s:%i", defaultIP.c_str(), params.GetDefaultPort());
        return EXIT_FAILURE;
    }
    CService service = LookupNumeric(defaultIP.c_str(), params.GetDefaultPort());

    if (!service.IsValid()) {
        fprintf(stderr, "Error: failed to parse: %s", defaultIP.c_str());
        return EXIT_FAILURE;
    }
    printf("Using node: %s\n", service.ToString().c_str());

    event_base* base = event_base_new();
    FILE* debugFile = fopen(messagePath.c_str(), "r");
    if (!debugFile) {
        fprintf(stderr, "Error: could not open file: %s\n", messagePath.c_str());
        return EXIT_FAILURE;
    }

    std::vector<CSerializedNetMsg> messages = ReadMessages(CAutoFile(debugFile, SER_DISK, CLIENT_VERSION), params);
    if (messages.empty()) {
        fprintf(stderr, "Error: could not read messages from: %s\n", messagePath.c_str());
        return EXIT_FAILURE;
    }

    std::list<CFuzzConnection> conns;
    for (int i = 0; i < 8; i++)
    {
        //conns.push_back(CFuzzConnection(base, service, params, messages));
        conns.emplace_back(base, service, params, messages);
    }

    event_base_loop(base, 0);
    event_base_free(base);
}
