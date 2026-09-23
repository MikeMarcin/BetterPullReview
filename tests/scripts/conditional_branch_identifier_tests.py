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

    def test_an_init_capture_declares_the_name_it_introduces(self) -> None:
        """`[moves = std::move(aMoves)]` declares `moves`; it is not a rename.

        BPR-0854: an init-capture names a new variable with no type in front of
        it, so the declaration pattern cannot see it, and `spelling_key` folds
        the capture onto the `aMoves` it came from. The gate reported four such
        captures in `src/app/src/github_application.cpp` -- including the lines
        that introduce them -- against declarations that are the captures
        themselves.
        """
        body = """
        void pick_colour(std::vector<QColor> aMoves, const QColor& aAccepted)
        {
        #if defined(_WIN32)
            set_picker([moves = std::move(aMoves), accepted = aAccepted]() {
                for (const auto& moved : moves) {
                    changed(moved);
                }
                return accepted;
            });
        #else
            set_picker([aAccepted]() { return aAccepted; });
        #endif
        }
        """
        self.assertEqual(self.findings(body, POSIX), [])

    def test_a_stale_spelling_a_lambda_reaches_is_still_caught(self) -> None:
        """Only the captured name is declared, and only inside its own lambda.

        The initializer is ordinary code, so `[bytes = expectedBytes]` beside
        an `aExpectedBytes` is still the rename this gate exists for, and so is
        a bare `limit` in the body the lambda goes on to write. Teaching the
        gate init-captures must not buy silence for either.
        """
        body = """
        int measure(int aExpectedBytes, int aLimit)
        {
        #if defined(_WIN32)
            return run([bytes = expectedBytes](int aValue) {
                return aValue + bytes + limit;
            });
        #else
            return run([bytes = aExpectedBytes](int aValue) {
                return aValue + bytes + aLimit;
            });
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, POSIX)}
        self.assertIn("expectedBytes", caught)
        self.assertIn("limit", caught)

    def test_a_call_of_a_function_this_file_defines_is_not_a_rename(
        self,
    ) -> None:
        """A snake_case helper is not a stale spelling of a camelCase local.

        BPR-0854: `src/app/main.cpp:8621` calls the file's own
        `schedule_native_notification_run` from the branch guarded by the flag
        `scheduleNativeNotificationRun`. `spelling_key` folds away the very
        underscores C++ uses to tell them apart, so the call read as a rename
        of the flag. Both names are correct and both are required.
        """
        body = """
        void schedule_native_notification_run(int aBudget)
        {
            (void)aBudget;
        }

        void start(bool aCourse)
        {
            const auto scheduleNativeNotificationRun = aCourse;
        #if defined(_WIN32)
            if (scheduleNativeNotificationRun) {
                schedule_native_notification_run(1);
            }
        #else
            (void)scheduleNativeNotificationRun;
        #endif
        }
        """
        self.assertEqual(self.findings(body, POSIX), [])

    def test_a_call_of_a_name_this_file_never_defines_is_still_caught(
        self,
    ) -> None:
        """Call position alone excuses nothing.

        A renamed helper is called exactly like a correct one. The exemption
        needs a definition of that spelling in the same file, so a branch
        calling a name that is nowhere defined is still the rename this gate
        exists for.
        """
        body = """
        int start(int aDispatchRun)
        {
        #if defined(_WIN32)
            return dispatch_run(aDispatchRun);
        #else
            return aDispatchRun;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, POSIX)}
        self.assertIn("dispatch_run", caught)

    def test_a_call_of_a_same_spelled_variable_is_still_caught(self) -> None:
        """A variable of that spelling is not a function of that spelling.

        `int dispatch_run(0);` is a variable whose parenthesized initializer
        reads like a declarator to a regular expression. The exemption asks
        instead for a definition -- a parameter list whose closing parenthesis
        reaches a body brace -- which this never has, so the call stays a
        finding.
        """
        body = """
        int dispatch_run(0);

        int start(int aDispatchRun)
        {
        #if defined(_WIN32)
            return dispatch_run(aDispatchRun);
        #else
            return aDispatchRun;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, POSIX)}
        self.assertIn("dispatch_run", caught)

    def test_a_digit_separator_does_not_open_a_character_literal(self) -> None:
        """[[BPR-0854]]. `10'000` is a number, not the start of `'...'`.

        Read as a quote, the first separator blanks everything up to the next
        `'`, here the one in `run`'s Windows branch. `wait`'s body then runs on
        into `run`, and `run`'s own local `window` reads as a stale spelling of
        `wait`'s `aWindow`. This is the shape `workbench_window_tests.cpp` had.
        """
        body = """
        void wait(int aWindow)
        {
            const auto budget = 10'000;
            (void)budget;
            (void)aWindow;
        }

        void run()
        {
            auto window = 1;
        #if defined(_WIN32)
            const auto limit = 10'000;
            window += limit;
        #endif
        }
        """
        self.assertEqual(self.findings(body, POSIX), [])

    def test_a_rename_after_a_digit_separator_is_still_caught(self) -> None:
        """A separator must not hide a real rename either.

        With the separator read as a quote, the literal it opens never closes,
        so the rest of the file is blanked and this rename went unreported.
        """
        body = """
        int measure(int aExpectedBytes)
        {
            const auto limit = 1'000;
        #if defined(_WIN32)
            return expectedBytes + limit;
        #else
            return aExpectedBytes + limit;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, POSIX)}
        self.assertIn("expectedBytes", caught)

    def test_a_prefixed_character_literal_is_still_a_literal(self) -> None:
        """`L'a'` is a literal whose prefix ends in a letter, not a number.

        A rule that took any `'` between two letters or digits for a
        separator would pass over the opening quote here and open a literal at
        the closing one instead, blanking the rest of the file and hiding the
        rename below it.
        """
        body = """
        int count(int aDepth)
        {
            const auto letter = L'a';
            (void)letter;
        #if defined(_WIN32)
            return depth;
        #else
            return aDepth;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, POSIX)}
        self.assertIn("depth", caught)

    def test_a_captured_outer_local_is_not_the_parameter(self) -> None:
        """[[BPR-0854]]. `[&session](const auto& aSession)` compares the two.

        The capture names an outer local whose spelling folds onto the
        lambda's own parameter. The bare-form pass already excused it; the
        same-spelling pass reported it anyway, so the row was red only on the
        hosts that skip this branch. `setup_destination_public_path_tests.cpp`
        has this shape under `__APPLE__`.
        """
        body = """
        #if defined(__APPLE__)
        bool same(int aValue)
        {
            const auto session = aValue;
            const auto match = [&session](const auto& aSession) {
                return aSession == session;
            };
            return match(aValue);
        }
        #endif
        """
        self.assertEqual(self.findings(body, WINDOWS), [])

    def test_a_capture_does_not_excuse_a_different_stale_name(self) -> None:
        """Only the captured name is excused, not every name in the lambda."""
        body = """
        #if defined(__APPLE__)
        bool same(int aValue, int aLimit)
        {
            const auto session = aValue;
            const auto match = [&session](const auto& aSession) {
                return aSession == session && session < limit;
            };
            return match(aLimit);
        }
        #endif
        """
        caught = {entry[1] for entry in self.findings(body, WINDOWS)}
        self.assertIn("limit", caught)
        self.assertNotIn("session", caught)


if __name__ == "__main__":
    unittest.main()
