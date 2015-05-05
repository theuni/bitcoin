// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqports.h"

#if ENABLE_ZMQ
#include <zmq.h>
#endif

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "version.h"
#include "main.h"
#include "streams.h"
#include "util.h"
#include "netbase.h"
#include "ui_interface.h"

using namespace std;

// Global state
bool fZMQPub = false;

#if ENABLE_ZMQ

// ZMQ related file scope variables
static void *zmqContext;
static void *zmqPubSocket;
static ZMQFormat zmqFormat;

// Internal utility functions
static void ZMQPublishBlock(const uint256 &hash);
static void ZMQPublishTransaction(const CTransaction &tx);

static void zmqError(const char *str)
{
    LogPrint("zmq", "Error: %s, errno=%s\n", str, zmq_strerror(errno));
}

// Called at startup to conditionally set up ZMQ socket(s)
void ZMQInitialize(const std::string &endp, ZMQFormat format)
{
    zmqContext = zmq_init(1);
    if (!zmqContext) {
        zmqError("Unable to initialize ZMQ context");
        return;
    }

    zmqPubSocket = zmq_socket(zmqContext, ZMQ_PUB);
    if (!zmqPubSocket) {
        zmqError("Unable to open ZMQ pub socket");
        return;
    }

    int rc = zmq_bind(zmqPubSocket, endp.c_str());
    if (rc != 0) {
        zmqError("Unable to bind ZMQ socket");
        zmq_close(zmqPubSocket);
        zmqPubSocket = 0;
        return;
    }

    uiInterface.NotifyBlockTip.connect(ZMQPublishBlock);
    uiInterface.NotifyRelayTx.connect(ZMQPublishTransaction);

    zmqFormat = format;
    fZMQPub = true;
    LogPrint("zmq", "PUB socket listening at %s\n", endp);
}

// Internal function to pack a zmq_msg_t and send it
// Note: assumes topic is a valid null terminated C string
static int zmqPublish(const void *data, size_t size, int flags)
{
    zmq_msg_t msg;
    
    int rc = zmq_msg_init_size(&msg, size);
    if (rc != 0)
    {
        zmqError("Unable to initialize ZMQ msg");
        return -1;
    }

    void *buf = zmq_msg_data(&msg);
    memcpy(buf, data, size);

    rc = zmq_msg_send(&msg, zmqPubSocket, flags);
    if (rc == -1)
    {
        zmqError("Unable to send ZMQ msg");
        return -1;
    }

    zmq_msg_close(&msg);

    return 0;
}

// Called after all transaction relay checks are completed
static void ZMQPublishTransaction(const CTransaction &tx)
{
    if (!zmqPubSocket)
        return;

    if (zmqFormat==ZMQ_FORMAT_NETWORK)
    {
        // Serialize transaction
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << tx;
    
        int rc = zmqPublish("TXN", 3, ZMQ_SNDMORE);
        if (rc==0)
            zmqPublish(&ss[0], ss.size(), 0);
    }
    else if (zmqFormat==ZMQ_FORMAT_HASH)
    {
        std::string hex = tx.GetHash().GetHex();
        int rc = zmqPublish("TXN", 3, ZMQ_SNDMORE);
        if (rc==0)
            zmqPublish(&hex[0], hex.size(), 0);
    }
}

// Called after the block chain tip changed
static void ZMQPublishBlock(const uint256 &hash)
{
    if (!zmqPubSocket)
        return;

    if (zmqFormat==ZMQ_FORMAT_NETWORK)
    {
        CBlock blk;
        {
            LOCK(cs_main);
            CBlockIndex* pblockindex = mapBlockIndex[hash];
            if(!ReadBlockFromDisk(blk, pblockindex))
                return;
        }

        // Serialize block
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << blk;

        int rc = zmqPublish("BLK", 3, ZMQ_SNDMORE);
        if (rc==0)
            zmqPublish(&ss[0], ss.size(), 0);
    }
    else if (zmqFormat==ZMQ_FORMAT_HASH)
    {
        std::string hex = hash.GetHex();
        int rc = zmqPublish("BLK", 3, ZMQ_SNDMORE);
        if (rc==0)
            zmqPublish(hex.c_str(), hex.size(), 0);
    }
}

// Called during shutdown sequence
void ZMQShutdown()
{
    if (!fZMQPub)
        return;

    if (zmqContext) {
        if (zmqPubSocket) {
            // Discard any unread messages and close socket
            int linger = 0;
            zmq_setsockopt(zmqPubSocket, ZMQ_LINGER, &linger, sizeof(linger));
            zmq_close(zmqPubSocket);
            zmqPubSocket = 0;
        }

        zmq_ctx_destroy(zmqContext);
        zmqContext = 0;
    }

    uiInterface.NotifyBlockTip.disconnect(ZMQPublishBlock);
    uiInterface.NotifyRelayTx.disconnect(ZMQPublishTransaction);

    fZMQPub = false;
}

#endif // ENABLE_ZMQ

