// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "netfuzz_connection.h"

#include "serialize.h"
#include "streams.h"
#include "clientversion.h"
#include "protocol.h"
#include "util.h"
#include "chainparams.h"
#include "net.h"
#include "random.h"

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <string>

CFuzzConnection::CFuzzConnection(event_base* baseIn, const CService& serviceIn, const CChainParams& paramsIn, const std::vector<CSerializedNetMsg>& messagesIn) : bev(nullptr), base(baseIn), service(serviceIn), params(paramsIn), messages(messagesIn)
{
    Connect();
}

CFuzzConnection::~CFuzzConnection()
{
    Disconnect();
}

void CFuzzConnection::read_callback(bufferevent *bev, void *ctx)
{
    CFuzzConnection* conn = static_cast<CFuzzConnection*>(ctx);
    conn->read();
}
void CFuzzConnection::write_callback(bufferevent *bev, void *ctx)
{
    CFuzzConnection* conn = static_cast<CFuzzConnection*>(ctx);
    conn->write();
}
void CFuzzConnection::event_callback(bufferevent *bev, short what, void *ctx)
{
    CFuzzConnection* conn = static_cast<CFuzzConnection*>(ctx);
    conn->event(what);
}

void CFuzzConnection::read()
{
    int chance = GetRandInt(10);
    if (chance > 9) {
        evutil_closesocket(bufferevent_getfd(bev));
        return;
    }

    evbuffer* buf = bufferevent_get_input(bev);
    size_t nBytes = evbuffer_get_length(buf);
    std::vector<char> dataCopy(nBytes);
    evbuffer_copyout(buf, dataCopy.data(), nBytes);
    const char* pch = dataCopy.data();
    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() ||
            vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(params.MessageStart(), SER_NETWORK, INIT_PROTO_VERSION));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readHeader(pch, nBytes);
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0) {
                return;
        }

        if (msg.in_data && msg.hdr.nMessageSize > MAX_PROTOCOL_MESSAGE_LENGTH) {
            return;
        }

        pch += handled;
        nBytes -= handled;

        if (msg.complete()) {
            ReceiveMessage(std::move(msg));
            vRecvMsg.clear();
        }
    }
}

void CFuzzConnection::write()
{
    evbuffer* buf = bufferevent_get_input(bev);
    if(!evbuffer_get_length(buf))
        SendRandomMessage();
}

void CFuzzConnection::event(short what)
{
    if (what & BEV_EVENT_TIMEOUT) {
        Disconnect();
        Connect();
    } else if (what & BEV_EVENT_EOF) {
        Disconnect();
        Connect();
    } else if (what & BEV_EVENT_ERROR) {
        Disconnect();
        Connect();
    } else if (what & BEV_EVENT_CONNECTED) {
        SendFirstMessage();
    }
}

void CFuzzConnection::Connect()
{
    sockaddr_storage sstor;
    socklen_t addrlen = sizeof(sstor);
    service.GetSockAddr((sockaddr*)&sstor, &addrlen);

    timeval timeout { 3, 0 };

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_set_timeouts(bev, &timeout, &timeout);

    bufferevent_enable(bev, EV_READ);
    bufferevent_setcb(bev, read_callback, write_callback, event_callback, this);
    bufferevent_socket_connect(bev, (sockaddr*)&sstor, (int)addrlen);

    SendFirstMessage();
}

void CFuzzConnection::Disconnect()
{
    if (bev) {
        bufferevent_setcb(bev, nullptr, nullptr, nullptr, nullptr);
        bufferevent_free(bev);
        bev = nullptr;
    }
}

void CFuzzConnection::ReceiveMessage(CNetMessage&& msg)
{
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    SendRandomMessage();
}

void CFuzzConnection::SendFirstMessage()
{
    // Send a version message first most of the time
    int chance = GetRandInt(10);
    if (chance > 7) {
        SendVersionMessage();
    } else {
        SendRandomMessage();
    }
}

void CFuzzConnection::SendVersionMessage()
{
    // Pick a random index and start there, looking for a version message.
    // If not found, start over at the beginning
    bool found = false;
    int index = GetRandInt(messages.size() - 1);
    for (int i = index; i < messages.size(); ++i)
    {
        if (messages.at(i).command == NetMsgType::VERSION) {
            found = true;
            Send(messages.at(i));
        }
    }
    if (!found) {
        for (int i = 0; i < index; i++)
        {
            if (messages.at(i).command == NetMsgType::VERSION) {
                found = true;
                Send(messages.at(i));
            }
        }
    }
    // No version message found! Just send a random one
    if (!found) {
        SendRandomMessage();
    }
}

void CFuzzConnection::SendRandomMessage()
{
    int index = GetRandInt(messages.size() - 1);
    Send(messages.at(index));
}

void CFuzzConnection::Send(const CSerializedNetMsg& msg)
{

    int chance = GetRandInt(10);

    size_t nMessageSize = msg.data.size();

    // Rarely cut out some data
    if (chance > 8)
    {
        nMessageSize = GetRandInt(msg.data.size());
    }

    std::vector<unsigned char> serializedHeader;
    serializedHeader.reserve(CMessageHeader::HEADER_SIZE);
    uint256 hash = Hash(msg.data.data(), msg.data.data() + nMessageSize);
    CMessageHeader hdr(params.MessageStart(), msg.command.c_str(), nMessageSize);

    // Rarely use a bad hash
    chance = GetRandInt(100);
    if (chance < 99) {
        memcpy(hdr.pchChecksum, hash.begin(), CMessageHeader::CHECKSUM_SIZE);
    }

    CVectorWriter{SER_NETWORK, INIT_PROTO_VERSION, serializedHeader, 0, hdr};
    bufferevent_write(bev, serializedHeader.data(), serializedHeader.size());
    bufferevent_write(bev, msg.data.data(), nMessageSize);
}
