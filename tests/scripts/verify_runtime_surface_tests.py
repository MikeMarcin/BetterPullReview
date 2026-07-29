#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.verify_runtime_surface import (  # noqa: E402
    output_lines,
    windows_child_process_query,
    windows_network_query,
)


class RuntimeSurfaceContractTests(unittest.TestCase):
    def test_output_lines_removes_only_empty_lines_and_outer_whitespace(self) -> None:
        self.assertEqual(
            output_lines("\n  123 \r\n tcp:127.0.0.1:8080 \n"),
            ["123", "tcp:127.0.0.1:8080"],
        )

    def test_windows_child_query_is_scoped_to_the_parent_pid(self) -> None:
        query = windows_child_process_query(4815)

        self.assertIn('ParentProcessId = 4815', query)
        self.assertIn("Select-Object -ExpandProperty ProcessId", query)

    def test_windows_network_query_checks_tcp_listeners_and_udp(self) -> None:
        query = windows_network_query(90210)

        self.assertIn("Get-NetTCPConnection -OwningProcess 90210", query)
        self.assertIn("-State Listen", query)
        self.assertIn("Get-NetUDPEndpoint -OwningProcess 90210", query)
        self.assertIn('"tcp:$($_.LocalAddress):$($_.LocalPort)"', query)
        self.assertIn('"udp:$($_.LocalAddress):$($_.LocalPort)"', query)


if __name__ == "__main__":
    unittest.main()
