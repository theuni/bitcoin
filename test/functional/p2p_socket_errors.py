#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test P2P handling of TCP urgent data and resets while sending."""

import socket

from test_framework.messages import (
    CInv,
    MAGIC_BYTES,
    MSG_BLOCK,
    MSG_WITNESS_FLAG,
    msg_getdata,
    msg_ping,
    msg_version,
)
from test_framework.netutil import pack_linger
from test_framework.p2p import NetworkThread, P2PInterface, P2P_VERSION
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises, mine_large_block, p2p_port
from test_framework.wallet import MiniWallet


def reset_on_close(sock):
    """Make a subsequent close send a TCP reset and discard unsent data."""
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, pack_linger(enabled=True, timeout=0))


class SocketErrorsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-maxsendbuffer=1", "-maxreceivebuffer=1"]]

    def run_test(self):
        self.message_builder = P2PInterface()
        self.message_builder.magic_bytes = MAGIC_BYTES[self.chain]
        self.version = msg_version()
        self.version.nVersion = P2P_VERSION
        self.check_urgent_bytes()
        self.check_reset_while_sending()

    def check_urgent_bytes(self):
        self.log.info("Check that sending urgent bytes causes a disconnect")
        node = self.nodes[0]

        with socket.create_connection(("127.0.0.1", p2p_port(0)), timeout=10) as peer:
            peer.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.wait_until(lambda: len(node.getpeerinfo()) == 1)
            assert_equal(peer.send(b"URG", socket.MSG_OOB), 3)
            assert_raises(ConnectionError, lambda: peer.recv(1))

        self.wait_until(lambda: len(node.getpeerinfo()) == 0)

    def check_reset_while_sending(self):
        self.log.info("Check reset while the node has unsent block data")
        node = self.nodes[0]
        wallet = MiniWallet(node)
        mine_large_block(self, wallet, node)
        block_hash = node.getbestblockhash()
        block_size = len(node.getblock(block_hash, 0)) // 2 + 24
        peer = node.add_p2p_connection(P2PInterface(), supports_v2_p2p=False)
        peer.sync_with_ping()
        NetworkThread.network_event_loop.call_soon_threadsafe(peer._transport.pause_reading)
        requests = 16
        peer.send_without_ping(msg_getdata([CInv(MSG_BLOCK | MSG_WITNESS_FLAG, int(block_hash, 16))] * requests))
        sent_bytes = self.wait_for_send_stall(node)
        assert_greater_than(requests * block_size, sent_bytes)
        # Exceed the receive-queue limit while processing is blocked by the
        # outgoing data. Wait until the node has read that first batch.
        pings = peer.build_message(msg_ping(nonce=123)) * 100
        received = node.getnettotals()["totalbytesrecv"] + len(pings)
        peer.send_raw_message(pings)
        self.wait_until(lambda: node.getnettotals()["totalbytesrecv"] == received)
        # More input is now queued in the kernel while receiving is paused.
        peer.send_raw_message(pings)
        reset_on_close(peer._transport.get_extra_info("socket"))
        expected_msgs = ["socket send error"]
        with node.assert_debug_log(expected_msgs):
            peer.peer_disconnect()
            self.wait_until(lambda: len(node.getpeerinfo()) == 0, timeout=10)
        assert_equal(node.getnettotals()["totalbytesrecv"], received)
        node.disconnect_p2ps()

if __name__ == '__main__':
    SocketErrorsTest(__file__).main()
