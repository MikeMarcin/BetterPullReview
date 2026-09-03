#!/usr/bin/env python3
"""Drive the conditional-branch gate in both directions.

The gate reads the branches this host does not compile and reports an
identifier whose spelling no longer matches the declaration beside it. That is
only worth having if it still fires on a real rename, so every case here pairs
a defect it must catch with a correct source it must stay silent about.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.contained_temporary_support import (  # noqa: E402
    contained_temporary_directory,
)
from scripts.verify_conditional_branches import findings_for  # noqa: E402


WINDOWS = {"_WIN32"}
POSIX = {"__APPLE__", "__unix__"}


def source(body: str) -> str:
    return "namespace bpr {\n" + body + "\n}\n"


class ConditionalBranchIdentifierTests(unittest.TestCase):
    def findings(self, body: str, defined: set[str]) -> list[tuple]:
        with contained_temporary_directory() as directory:
            path = Path(directory) / "unit.cpp"
            path.write_text(source(body), encoding="utf-8")
            return findings_for(path, defined)

    def test_a_renamed_parameter_in_an_inactive_branch_is_caught(self) -> None:
        body = """
        int measure(int aExpectedBytes) noexcept
        {
        #if defined(_WIN32)
            return expectedBytes;
        #else
            return aExpectedBytes;
        #endif
        }
        """
        findings = self.findings(body, POSIX)
        self.assertTrue(findings, "the gate must still catch a real rename")
        self.assertIn("expectedBytes", {entry[1] for entry in findings})

    def test_the_gate_is_silent_when_the_branch_uses_the_current_name(
        self,
    ) -> None:
        body = """
        int measure(int aExpectedBytes) noexcept
        {
        #if defined(_WIN32)
            return aExpectedBytes + 1;
        #else
            return aExpectedBytes;
        #endif
        }
        """
        self.assertEqual(self.findings(body, POSIX), [])

    def test_a_local_declared_before_the_chain_is_not_the_parameter(
        self,
    ) -> None:
        # BPR-0285: `system_browser_opener::open` declares `auto address =
        # validated(aAddress)` above its chain and uses `*address` in the
        # branch this host does not compile. The bare form of `aAddress` is
        # that local, and the gate must read the body's declaration rather
        # than only the branch's.
        body = """
        bool open(int aAddress)
        {
            auto address = validated(aAddress);
            if (!address) {
                return false;
            }
        #if defined(_WIN32)
            return launch(*address);
        #else
            return other(*address);
        #endif
        }
        """
        self.assertEqual(self.findings(body, POSIX), [])

    def test_a_bare_parameter_form_without_a_local_is_still_caught(
        self,
    ) -> None:
        body = """
        bool open(int aAddress)
        {
        #if defined(_WIN32)
            return launch(address);
        #else
            return other(aAddress);
        #endif
        }
        """
        findings = self.findings(body, POSIX)
        self.assertIn("address", {entry[1] for entry in findings})

    def test_a_platform_alias_call_is_not_a_declaration(self) -> None:
        """`return _read(x)` calls a function; it does not declare one.

        Reading it as a declaration made `_read` a sibling of the POSIX `read`
        in the opposite branch, because `spelling_key` strips underscores. Both
        spellings are correct, for two different functions, on two different
        platforms. This is the exact shape of
        `tests/network_isolation/designated_live_mutation_canary.cpp`.
        """
        body = """
        int read_bytes(int aDescriptor, char* aBuffer, unsigned aSize) noexcept
        {
        #if defined(_WIN32)
            return _read(aDescriptor, aBuffer, aSize);
        #else
            return static_cast<int>(read(aDescriptor, aBuffer, aSize));
        #endif
        }
        """
        self.assertEqual(self.findings(body, WINDOWS), [])
        self.assertEqual(self.findings(body, POSIX), [])

    def test_a_using_alias_still_declares_its_name(self) -> None:
        """`using reason = ...;` is a declaration, not a statement.

        Excluding `using` from the type position would lose it, and the
        inactive branch's own `reason` would then look like a rename of the
        `aReason` beside it. `src/app/main.cpp:750` has exactly this shape: the
        alias is declared inside the branch that does not compile here.
        """
        body = """
        void report(int aReason) noexcept
        {
        #if defined(_WIN32)
            using reason = int;
            reason value = aReason;
            (void)value;
        #else
            (void)aReason;
        #endif
        }
        """
        self.assertEqual(self.findings(body, POSIX), [])


if __name__ == "__main__":
    unittest.main()
