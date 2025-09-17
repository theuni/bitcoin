// Copyright (c) 2025 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/init.h>
#include <interfaces/ipc.h>

class CConnman;

namespace init {
namespace {
class P2PServerInit : public interfaces::Init
{
public:
    P2PServerInit(CConnman& connman, const char* exe_name, const char* process_argv0) : m_connman(connman), m_ipc(interfaces::MakeIpc(exe_name, process_argv0, *this)) {}
    interfaces::Ipc* ipc() override { return m_ipc.get(); }
    bool canListenIpc() override { return true; }
    std::unique_ptr<interfaces::NetManagerEvents> makeConnman() override { return interfaces::MakeConnman(m_connman); }
private:
    CConnman& m_connman;
    std::unique_ptr<interfaces::Ipc> m_ipc;
};
} // namespace
} // namespace init

namespace interfaces {
std::unique_ptr<Init> MakeP2PServerInit(CConnman& connman, const char* exe_name, const char* process_argv0)
{
    return std::make_unique<init::P2PServerInit>(connman, exe_name, process_argv0);
}
} // namespace interfaces
