// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HTTPRPC_H
#define BITCOIN_HTTPRPC_H

#include <string>
#include <map>

class HTTPRequest;

bool StartHTTPRPC();
void InterruptHTTPRPC(); /// XXX replace by signal
void StopHTTPRPC();

bool HTTPReq_JSONRPC(HTTPRequest* req);
//! Defined in rest.cpp
bool HTTPReq_REST(HTTPRequest* req);

#endif

