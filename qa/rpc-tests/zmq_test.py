#!/usr/bin/env python2
# Copyright (c) 2015 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test ZMQ interface
#

from test_framework import BitcoinTestFramework
from util import *
import json
import zmq
import binascii
from mininode import hash256

try:
    import http.client as httplib
except ImportError:
    import httplib
try:
    import urllib.parse as urlparse
except ImportError:
    import urlparse

class ZMQTest (BitcoinTestFramework):

    port = 28332

    def handleBLK(self, blk):
        return binascii.hexlify(hash256(blk[:80])[::-1])


    def handleTX(self, tx):
        return binascii.hexlify(hash256(tx)[::-1])

    def setup_nodes(self):
        self.zmqContext = zmq.Context()
        self.zmqSubSocket = self.zmqContext.socket(zmq.SUB)
        self.zmqSubSocket.setsockopt(zmq.SUBSCRIBE, "BLK")
        self.zmqSubSocket.setsockopt(zmq.SUBSCRIBE, "TXN")
        self.zmqSubSocket.connect("tcp://127.0.0.1:%i" % self.port)

        # Note: proxies are not used to connect to local nodes
        # this is because the proxy to use is based on CService.GetNetwork(), which return NET_UNROUTABLE for localhost
        return start_nodes(4, self.options.tmpdir, extra_args=[
            ['-zmqpub=tcp://127.0.0.1:'+str(self.port)], 
            [],
            [],
            []
            ])

    def run_test(self):

        genhashes = self.nodes[0].generate(1);

        msg = self.zmqSubSocket.recv()
        blkhash = self.handleBLK(msg[3:])
        assert_equal(genhashes[0], blkhash) #blockhash from generate must be equal to the hash received over zmq

        genhashes = self.nodes[1].generate(10);
        self.sync_all()

        zmqHashes = []
        for x in range(0,10):
            msg = self.zmqSubSocket.recv()
            zmqHashes.append(self.handleBLK(msg[3:]))

        #sort hashes
        genhashes.sort()
        zmqHashes.sort()
        for x in range(0,9):
            assert_equal(genhashes[x], zmqHashes[x]) #blockhash from generate must be equal to the hash received over zmq

        #test tx from a second node
        hashRPC = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1.0)
        self.sync_all()

        #now we should receive a zmq msg because the tx was broadcastet
        msg = self.zmqSubSocket.recv()
        hashZMQ = self.handleTX(msg[3:])
        assert_equal(hashRPC, hashZMQ) #blockhash from generate must be equal to the hash received over zmq


if __name__ == '__main__':
    ZMQTest ().main ()
