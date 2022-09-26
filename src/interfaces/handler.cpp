// Copyright (c) 2018-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/handler.h>


#include <btcsignals.h>
#include <utility>

namespace interfaces {
namespace {

class HandlerImpl : public Handler
{
public:
    explicit HandlerImpl(btcsignals::connection connection) : m_connection(std::move(connection)) {}

    void disconnect() override { m_connection.disconnect(); }

    btcsignals::scoped_connection m_connection;
};

class CleanupHandler : public Handler
{
public:
    explicit CleanupHandler(std::function<void()> cleanup) : m_cleanup(std::move(cleanup)) {}
    ~CleanupHandler() override { if (!m_cleanup) return; m_cleanup(); m_cleanup = nullptr; }
    void disconnect() override { if (!m_cleanup) return; m_cleanup(); m_cleanup = nullptr; }
    std::function<void()> m_cleanup;
};

} // namespace

std::unique_ptr<Handler> MakeHandler(btcsignals::connection connection)
{
    return std::make_unique<HandlerImpl>(std::move(connection));
}

std::unique_ptr<Handler> MakeHandler(std::function<void()> cleanup)
{
    return std::make_unique<CleanupHandler>(std::move(cleanup));
}

} // namespace interfaces
