#!/usr/bin/env python3
"""Controls for client disconnects in the release HTTPS fixture."""
from __future__ import annotations

import errno
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

import verify_release_update_transport as fixture


CONNECT = b"CONNECT api.github.com:443 HTTP/1.1\r\n\r\n"


def disconnected_proxy(scenario, *, connect=CONNECT, origin=b"", tls_error=None):
    """Run the real proxy handler against a peer closing at a chosen boundary."""
    connection = MagicMock()
    connection.__enter__.return_value = connection
    connection.recv.return_value = connect
    channel = MagicMock()
    channel.__enter__.return_value = channel
    channel.recv.return_value = origin
    certificate = SimpleNamespace(leaf_certificate="unused", leaf_private_key="unused")
    with patch.object(fixture.socket, "socket") as listener, patch.object(fixture.ssl, "SSLContext") as tls:
        listener.return_value.accept.return_value = (connection, ("127.0.0.1", 1))
        tls.return_value.wrap_socket.return_value = channel
        tls.return_value.wrap_socket.side_effect = tls_error
        proxy = fixture.Proxy(certificate, 0, scenario, b"synthetic-canary")
        connection.__exit__.side_effect = lambda *_: proxy.stop.set()
        proxy.run()
    return proxy


class TimeoutObservationTests(unittest.TestCase):
    def test_timeout_can_close_either_incomplete_header(self):
        for connect in (b"", CONNECT):
            with self.subTest(connect_complete=bool(connect)):
                proxy = disconnected_proxy("timeout", connect=connect)
                self.assertEqual(proxy.requests, [])
                proxy.verify()

    def test_truncated_healthy_and_cancelled_requests_are_rejected(self):
        for scenario in ("asset", "cancel"):
            for connect in (b"", CONNECT):
                with self.subTest(scenario=scenario, connect_complete=bool(connect)):
                    proxy = disconnected_proxy(scenario, connect=connect)
                    self.assertTrue(any("inbound-header-truncated" in error for error in proxy.errors))
                    with self.assertRaises(AssertionError):
                        proxy.verify()

    def test_timeout_does_not_excuse_a_wrong_authority_or_header_limit(self):
        for connect in (CONNECT.replace(b"api.github.com", b"evil.invalid"), b"x" * 16384):
            with self.subTest(connect_length=len(connect)):
                proxy = disconnected_proxy("timeout", connect=connect)
                self.assertTrue(proxy.errors)
                with self.assertRaises(AssertionError):
                    proxy.verify()

    def test_timeout_can_close_during_tls_wrapping(self):
        for code in (errno.EINVAL, errno.ENOTCONN):
            with self.subTest(code=code):
                disconnected_proxy("timeout", tls_error=OSError(code, "peer closed")).verify()
                with self.assertRaises(AssertionError):
                    disconnected_proxy("asset", tls_error=OSError(code, "peer closed")).verify()

    def test_unrelated_tls_error_still_fails_a_timeout(self):
        with self.assertRaises(AssertionError):
            disconnected_proxy("timeout", tls_error=OSError(errno.EACCES, "refused")).verify()

    def test_every_completed_timeout_request_still_checks_credentials(self):
        proxy = disconnected_proxy("timeout")
        request = (
            b"GET /repos/MikeMarcin/BetterPullReview/releases/assets/20 HTTP/1.1\r\n"
            b"X-GitHub-Api-Version: 2022-11-28\r\nAccept-Encoding: identity\r\n"
            b"Authorization: Bearer synthetic-canary\r\n\r\n"
        )
        proxy.requests = [(b"api.github.com:443", request)]
        proxy.verify()
        proxy.requests.append(proxy.requests[0])
        with self.assertRaises(AssertionError):
            proxy.verify()
        proxy.requests = [(b"api.github.com:443", request.replace(b"synthetic-canary", b"wrong"))]
        with self.assertRaises(AssertionError):
            proxy.verify()


if __name__ == "__main__":
    unittest.main()
