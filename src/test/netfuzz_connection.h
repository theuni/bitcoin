// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NETFUZZ_CONNECTION_H
#define BITCOIN_NETFUZZ_CONNECTION_H

#include "protocol.h"
#include <list>

struct bufferevent;
struct event_base;
class CChainParams;
class CSerializedNetMsg;
class CNetMessage;

class CFuzzConnection
{
public:
    CFuzzConnection(event_base* base, const CService& serviceIn, const CChainParams& paramsIn, const std::vector<CSerializedNetMsg>& messagesIn);
    ~CFuzzConnection();
    void Disconnect();
private:
    void Connect();
    void SendFirstMessage();
    void SendVersionMessage();
    void SendRandomMessage();
    void Send(const CSerializedNetMsg&);
    void ReceiveMessage(CNetMessage&& msg);
    void read();
    void readmsg(const char *pch, size_t nBytes);
    void write();
    void event(short what);
    static void read_callback(bufferevent *bev, void *ctx);
    static void write_callback(bufferevent *bev, void *ctx);
    static void event_callback(bufferevent *bev, short what, void *ctx);

    bufferevent* bev;
    event_base* base;
    CService service;
    const CChainParams& params;
    const std::vector<CSerializedNetMsg>& messages;

    std::list<CNetMessage> vRecvMsg;
};

#endif // BITCOIN_NETFUZZ_CONNECTION_H
