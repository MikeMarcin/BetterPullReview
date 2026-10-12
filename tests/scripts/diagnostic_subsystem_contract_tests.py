"""Counterexamples for child bounds and terminal validation in diagnostics."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / "end_to_end" / "diagnostic_subsystem_contract.py"
SPECIFICATION = importlib.util.spec_from_file_location("diagnostic_subsystem_contract", SCRIPT)
CONTRACT = importlib.util.module_from_spec(SPECIFICATION)
SPECIFICATION.loader.exec_module(CONTRACT)


class DiagnosticSubsystemContractTests(unittest.TestCase):
    def run_contract(self, result, arguments=None):
        output = io.StringIO()
        with mock.patch.object(CONTRACT.subprocess, "run", return_value=result) as run:
            with contextlib.redirect_stdout(output):
                CONTRACT.main(arguments or ["--course", "540", "enrollment"])
        return run, output.getvalue()

    def test_each_child_keeps_its_declared_backstop(self):
        run, output = self.run_contract(
            subprocess.CompletedProcess([], 0, b"pass", b""),
            ["--course", "120", "diff", "--course", "540", "enrollment"])
        self.assertEqual([call.kwargs["timeout"] for call in run.call_args_list], [120, 540])
        self.assertEqual([call.args[0] for call in run.call_args_list], [["diff"], ["enrollment"]])
        self.assertIn("diagnostic-subsystem-contract:pass", output)

    def test_nonzero_child_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.run_contract(subprocess.CompletedProcess([], 1, b"", b"failed"))

    def test_canary_in_either_terminal_is_rejected(self):
        for stdout, stderr in ((b"BPR_DIAGNOSTIC_PRIVATE_CANARY", b""),
                               (b"", b"BPR_DIAGNOSTIC_PRIVATE_CANARY")):
            with self.subTest(stdout=bool(stdout)):
                with self.assertRaises(AssertionError):
                    self.run_contract(subprocess.CompletedProcess([], 0, stdout, stderr))

    def test_timeout_reports_child_and_bound_without_partial_content(self):
        failure = subprocess.TimeoutExpired("enrollment", 540, output=b"private partial")
        with mock.patch.object(CONTRACT.subprocess, "run", side_effect=failure):
            with self.assertRaisesRegex(AssertionError, "enrollment: PROCESS_TIMEOUT after 540s") as raised:
                CONTRACT.main(["--course", "540", "enrollment"])
        self.assertNotIn("private partial", str(raised.exception))

    def test_invalid_bound_refuses_before_any_child(self):
        for timeout in ("0", "-1", "nan"):
            with self.subTest(timeout=timeout):
                with mock.patch.object(CONTRACT.subprocess, "run") as run:
                    with contextlib.redirect_stderr(io.StringIO()):
                        with self.assertRaises(SystemExit):
                            CONTRACT.main(["--course", timeout, "enrollment"])
                run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
