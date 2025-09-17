// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_IPC_CAPNP_P2P_TYPES_H
#define BITCOIN_IPC_CAPNP_P2P_TYPES_H

#include <interfaces/net.h>
#include <ipc/capnp/common.capnp.proxy-types.h>
#include <ipc/capnp/common-types.h>
#include <ipc/capnp/p2p.capnp.proxy.h>

namespace mp {
// Custom serializations
//

template <typename Value, typename Output>
void CustomBuildField(TypeList<CAddress>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    DataStream stream;
    auto wrapper{ParamsStream{stream, CAddress::V2_NETWORK}};
    value.Serialize(wrapper);
    auto result = output.init(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}

template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<CAddress>, Priority<1>, InvokeContext& invoke_context, Input&& input,
                               ReadDest&& read_dest)
{
    return read_dest.update([&](auto& value) {
        if (!input.has()) return;
        auto data = input.get();
        DataStream stream({data.begin(), data.end()});
        auto wrapper{ParamsStream{stream, CAddress::V2_NETWORK}};
        value.Unserialize(wrapper);
    });
}

template <typename Value, typename Output>
void CustomBuildField(TypeList<CService>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    DataStream stream;
    auto wrapper{ParamsStream{stream, CNetAddr::V2}};
    value.Serialize(wrapper);
    auto result = output.init(stream.size());
    memcpy(result.begin(), stream.data(), stream.size());
}


template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<CService>, Priority<1>, InvokeContext& invoke_context, Input&& input,
                               ReadDest&& read_dest)
{
    return read_dest.update([&](auto& value) {
        if (!input.has()) return;
        auto data = input.get();
        SpanReader stream({data.begin(), data.end()});
        auto wrapper{ParamsStream{stream, CNetAddr::V2}};
        value.Unserialize(wrapper);
    });
}

}
#endif // BITCOIN_IPC_CAPNP_P2P_TYPES_H
