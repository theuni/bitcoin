#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test P2P handling of TCP urgent bytes and urgent pointers ahead of received data."""

import socket

from test_framework.messages import MAGIC_BYTES, msg_generic, msg_version
from test_framework.netutil import get_tcp_urgent_padding, send_tcp_urgent_data
from test_framework.p2p import P2PInterface, P2P_VERSION
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, p2p_port, assert_raises


class SocketErrorsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        self.message_builder = P2PInterface()
        self.message_builder.magic_bytes = MAGIC_BYTES[self.chain]
        self.version = msg_version()
        self.version.nVersion = P2P_VERSION
        self.check_urgent_bytes()

    def check_urgent_bytes(self):
        self.log.info("Check that sending urgent bytes causes a disconnect")
        node = self.nodes[0]

        with socket.create_connection(("127.0.0.1", p2p_port(0)), timeout=10) as peer:
            peer.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.wait_until(lambda: len(node.getpeerinfo()) == 1)
            assert_equal(peer.send(b"URG", socket.MSG_OOB), 3)
            assert_raises(ConnectionError, lambda: peer.recv(1))

        self.wait_until(lambda: len(node.getpeerinfo()) == 0)

if __name__ == '__main__':
    SocketErrorsTest(__file__).main()
