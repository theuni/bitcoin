// Copyright (c) 2021-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/args.h>
#include <init.h>
#include <interfaces/chain.h>
#include <interfaces/echo.h>
#include <interfaces/init.h>
#include <interfaces/ipc.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <node/context.h>
#include <util/check.h>
#include <net_processing.h>

#include <memory>

namespace init {
namespace {
const char* EXE_NAME = "bitcoin-node";

std::unique_ptr<interfaces::Init> node_init;
std::unique_ptr<interfaces::NetManagerEvents> connman;

class BitcoinNodeInit : public interfaces::Init
{
public:
    BitcoinNodeInit(node::NodeContext& node, const char* arg0)
        : m_node(node),
          m_ipc(interfaces::MakeIpc(EXE_NAME, arg0, *this))
    {
        InitContext(m_node);
        m_node.init = this;
    }
    std::unique_ptr<interfaces::Node> makeNode() override { return interfaces::MakeNode(m_node); }
    std::unique_ptr<interfaces::Chain> makeChain() override { return interfaces::MakeChain(m_node); }
    std::unique_ptr<interfaces::Mining> makeMining() override { return interfaces::MakeMining(m_node); }
    std::unique_ptr<interfaces::NetEventsInterface> makePeerMan() override {

        // This is a MASSIVE temporary hack. A callback interface should be sent over instead.
        std::string sockpath{"unix:" + std::string{m_node.args->GetDataDirNet() / "p2p.sock"}};
        LogDebug(BCLog::IPC, "Attempting connection to: %s\n", sockpath);
        try {
            node_init = m_ipc->connectAddress(sockpath);
        } catch (const std::exception& exception) {
            tfm::format(std::cerr, "Error: %s\n", exception.what());
            return nullptr;
        }
        assert(node_init);
        LogDebug(BCLog::IPC, "Connected to bitcoin-p2p.\n");
        connman = node_init->makeConnman();
        assert(connman);
        m_node.peerman->SetConnman(connman.get());
        return interfaces::MakePeerMan(m_node);
    }
    std::unique_ptr<interfaces::WalletLoader> makeWalletLoader(interfaces::Chain& chain) override
    {
        return MakeWalletLoader(chain, *Assert(m_node.args));
    }
    std::unique_ptr<interfaces::Echo> makeEcho() override { return interfaces::MakeEcho(); }
    interfaces::Ipc* ipc() override { return m_ipc.get(); }
    bool canListenIpc() override { return true; }
    node::NodeContext& m_node;
    std::unique_ptr<interfaces::Ipc> m_ipc;
};
} // namespace
} // namespace init

namespace interfaces {
std::unique_ptr<Init> MakeNodeInit(node::NodeContext& node, int argc, char* argv[], int& exit_status)
{
    auto init = std::make_unique<init::BitcoinNodeInit>(node, argc > 0 ? argv[0] : "");
    // Check if bitcoin-node is being invoked as an IPC server. If so, then
    // bypass normal execution and just respond to requests over the IPC
    // channel and return null.
    if (init->m_ipc->startSpawnedProcess(argc, argv, exit_status)) {
        return nullptr;
    }
    return init;
}
} // namespace interfaces
