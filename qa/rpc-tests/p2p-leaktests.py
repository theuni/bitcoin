#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.mininode import *
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import logging

class CLazyNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.connection = None
        self.done = False
        self.unrequested_msg = False

    def add_connection(self, conn):
        self.connection = conn

    def check(self):
        def done():
            return self.done
        return wait_until(done, timeout=10) and not self.unrequested_msg

    def send_message(self, message):
        self.connection.send_message(message)

    def bad_message(self, message):
        self.unrequested_msg = True
        raise Exception("should not have received message: %s" % message.command)

    def on_version(self, conn, message): self.bad_message(message)
    def on_verack(self, conn, message): self.bad_message(message)
    def on_reject(self, conn, message): self.bad_message(message)
    def on_inv(self, conn, message): self.bad_message(message)
    def on_addr(self, conn, message): self.bad_message(message)
    def on_alert(self, conn, message): self.bad_message(message)
    def on_getdata(self, conn, message): self.bad_message(message)
    def on_getblocks(self, conn, message): self.bad_message(message)
    def on_tx(self, conn, message): self.bad_message(message)
    def on_block(self, conn, message): self.bad_message(message)
    def on_getaddr(self, conn, message): self.bad_message(message)
    def on_headers(self, conn, message): self.bad_message(message)
    def on_getheaders(self, conn, message): self.bad_message(message)
    def on_ping(self, conn, message): self.bad_message(message)
    def on_mempool(self, conn): self.bad_message(message)
    def on_pong(self, conn, message): self.bad_message(message)
    def on_feefilter(self, conn, message): self.bad_message(message)
    def on_sendheaders(self, conn, message): self.bad_message(message)
    def on_sendcmpct(self, conn, message): self.bad_message(message)
    def on_cmpctblock(self, conn, message): self.bad_message(message)
    def on_getblocktxn(self, conn, message): self.bad_message(message)
    def on_blocktxn(self, conn, message): self.bad_message(message)


# Node that never sends a version. We'll use this to send a bunch of messages
# anyway, and eventually get disconnected.
class CNodeNoVersionBan(CLazyNode):
    def __init__(self):
        CLazyNode.__init__(self)

    def on_close(self, conn):
        self.done = True
    def on_reject(self, conn, message): pass


# Node that never sends a version. This one just sits idle and hopes to receive
# any message (it shouldn't!)
class CNodeNoVersionIdle(CLazyNode):
    def __init__(self):
        CLazyNode.__init__(self)
        self.done = True

# Node that sends a version but not a verack.
class CNodeNoVerackIdle(CLazyNode):
    def __init__(self):
        CLazyNode.__init__(self)

    def on_reject(self, conn, message): pass
    def on_verack(self, conn, message): pass

    # When version is received, don't replay with a verack. Instead, see if the
    # node will give us a message that it shouldn't. This is not an exhaustive
    # list!
    def on_version(self, conn, message):
        self.done = True
        conn.send_message(msg_ping())
        conn.send_message(msg_getaddr())

class P2PLeakTest(BitcoinTestFramework):
    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("BITCOIND", "bitcoind"),
                          help="Binary to test max block requests behavior")

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.banscore = 10
    def setup_network(self):
        extra_args = [['-debug', '-banscore='+str(self.banscore)]
                      for i in range(self.num_nodes)]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args)

    def run_test(self):
        no_version_bannode = CNodeNoVersionBan()
        no_version_idlenode = CNodeNoVersionIdle()
        no_verack_idlenode = CNodeNoVerackIdle()

        connections = []
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], no_version_bannode, send_version=False))
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], no_version_idlenode, send_version=False))
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], no_verack_idlenode))
        no_version_bannode.add_connection(connections[0])
        no_version_idlenode.add_connection(connections[1])
        no_verack_idlenode.add_connection(connections[2])

        NetworkThread().start()  # Start up network handling in another thread
        self.nodes[0].generate(1)

        # send a bunch of veracks without sending a message. This should get us disconnected.
        # NOTE: implementation-specific check here. Remove if bitcoind ban behavior changes
        for i in range(self.banscore):
            no_version_bannode.send_message(msg_verack())

        #Give the node enough time to possibly leak out a message
        time.sleep(5)

        assert(no_version_bannode.check())
        assert(no_version_idlenode.check())
        assert(no_verack_idlenode.check())

if __name__ == '__main__':
    P2PLeakTest().main()
