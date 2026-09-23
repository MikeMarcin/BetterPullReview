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
from unittest import mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts.contained_temporary_support import (  # noqa: E402
    contained_temporary_directory,
)
from scripts.verify_conditional_branches import (  # noqa: E402
    PLATFORM_MACROS,
    findings_for,
    host_macros,
    unknown_platform_macros,
)


WINDOWS = {"_WIN32"}
POSIX = {"__APPLE__", "__unix__"}
# The sets `host_macros()` gives on those hosts, Qt's spellings included.
LINUX = {"__linux__", "__unix__", "Q_OS_LINUX", "Q_OS_UNIX"}
WINDOWS_QT = {"_WIN32", "_MSC_VER", "Q_OS_WIN"}


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

    def test_a_testing_region_is_left_to_the_compiler(self) -> None:
        """[[BPR-0866]]. A build switch does not decide what the host compiles.

        The `dev` build compiles `BPR_BUILD_TESTING` regions, so the compiler
        already rejects this stale spelling. The tests-disabled build that
        `application_composition_contract` makes compiles the other side. The
        gate polices neither side on any host. Reading the switch as undefined
        is how BPR-0854 got eleven findings in code a compiler had already
        accepted.
        """
        body = """
        int measure(int aExpectedBytes)
        {
        #if defined(BPR_BUILD_TESTING)
            return expectedBytes;
        #else
            return expected_bytes;
        #endif
        }
        """
        for host in (POSIX, WINDOWS, LINUX):
            self.assertEqual(self.findings(body, host), [])

    def test_a_platform_branch_inside_a_testing_region_is_still_policed(
        self,
    ) -> None:
        """The testing region is compiled here, but its Windows branch is not."""
        body = """
        #if defined(BPR_BUILD_TESTING)
        int measure(int aExpectedBytes)
        {
        #if defined(_WIN32)
            return expectedBytes;
        #else
            return aExpectedBytes;
        #endif
        }
        #endif
        """
        caught = {entry[1] for entry in self.findings(body, LINUX)}
        self.assertIn("expectedBytes", caught)
        self.assertEqual(self.findings(body, WINDOWS), [])

    def test_a_platform_macro_decides_a_condition_it_shares_with_a_switch(
        self,
    ) -> None:
        """`defined(_WIN32) && defined(BPR_BUILD_TESTING)` is false off Windows.

        No value of the switch makes it true where `_WIN32` is undefined, so the
        branch is compiled by no build here. On Windows the switch still
        decides it, so it stays with the compiler there.
        """
        body = """
        int measure(int aExpectedBytes)
        {
        #if defined(_WIN32) && defined(BPR_BUILD_TESTING)
            return expectedBytes;
        #else
            return aExpectedBytes;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, LINUX)}
        self.assertIn("expectedBytes", caught)
        self.assertEqual(self.findings(body, WINDOWS), [])

    def test_an_else_after_a_taken_elif_is_policed(self) -> None:
        """[[BPR-0866]]. `#else` is compiled only when no earlier branch was.

        After a taken `#elif defined(__linux__)`, Linux does not compile the
        `#else`. The gate used to read that `#else` as the opposite of the
        `#if` alone and skip it. That is how the fallbacks after Unix and
        Windows chains in `installed_git_cache.cpp` and
        `installed_git_process.cpp` kept thirty stale parameter names that no
        build compiles.
        """
        body = """
        int measure(int aExpectedBytes)
        {
        #if defined(_WIN32)
            return aExpectedBytes + 1;
        #elif defined(__linux__)
            return aExpectedBytes;
        #else
            return expectedBytes;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, LINUX)}
        self.assertIn("expectedBytes", caught)

    def test_a_false_elif_after_an_undecided_if_is_policed(self) -> None:
        """The switch cannot make `#elif defined(_WIN32)` compile off Windows."""
        body = """
        int measure(int aExpectedBytes)
        {
        #if defined(BPR_BUILD_TESTING)
            return aExpectedBytes;
        #elif defined(_WIN32)
            return expectedBytes;
        #else
            return aExpectedBytes + 1;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, LINUX)}
        self.assertIn("expectedBytes", caught)

    def test_qt_platform_names_follow_the_host(self) -> None:
        """`Q_OS_UNIX` is compiled on Linux and macOS and nowhere on Windows.

        Before, the gate knew only the compilers' spellings, so it policed a
        `Q_OS_UNIX` branch on the very hosts that compile it. It also treated
        that branch's `#else` as compiled there, so its Windows code went
        unread.
        """
        body = """
        int measure(int aExpectedBytes)
        {
        #if defined(Q_OS_UNIX)
            return aExpectedBytes;
        #else
            return expectedBytes;
        #endif
        }
        """
        caught = {entry[1] for entry in self.findings(body, LINUX)}
        self.assertIn("expectedBytes", caught)
        self.assertEqual(self.findings(body, WINDOWS_QT), [])
        stale_unix = body.replace("return aExpectedBytes;", "return expected_bytes;")
        stale_unix = stale_unix.replace("return expectedBytes;", "return aExpectedBytes;")
        self.assertEqual(self.findings(stale_unix, LINUX), [])
        caught = {entry[1] for entry in self.findings(stale_unix, WINDOWS_QT)}
        self.assertIn("expected_bytes", caught)

    def test_a_statement_is_not_a_function(self) -> None:
        """[[BPR-0866]]. `for (; prefix != aPrefix.end(); ...)` declares nothing.

        Read as a function, the loop had a parameter `aPrefix` and a block
        that did not hold the `auto prefix` declared above it, so `prefix`
        looked like a stale spelling. `github_application.cpp`'s `within`
        lambda has this shape in a branch no platform compiles.
        """
        body = """
        #if defined(_WIN32)
        bool within(const path& aPath, const path& aPrefix)
        {
            auto path   = aPath.begin();
            auto prefix = aPrefix.begin();
            for (; prefix != aPrefix.end(); ++prefix, ++path) {
                if (path == aPath.end() || *path != *prefix) {
                    return false;
                }
            }
            return true;
        }
        #endif
        """
        self.assertEqual(self.findings(body, LINUX), [])

    def test_a_rename_inside_a_statement_block_is_still_caught(self) -> None:
        """The enclosing function still reads the block with its own parameters."""
        body = """
        #if defined(_WIN32)
        int total(int aCount, int aLimit)
        {
            int sum = 0;
            for (int index = 0; index < aCount; ++index) {
                if (index < aLimit) {
                    sum += limit;
                }
            }
            return sum;
        }
        #endif
        """
        caught = {entry[1] for entry in self.findings(body, LINUX)}
        self.assertIn("limit", caught)

    def test_an_undecided_platform_spelling_is_refused(self) -> None:
        """A platform macro the gate does not know would leave its branch unread.

        Undecided is the quiet direction, so the gate names the macro instead.
        A build switch, a Windows header switch and a known platform are not
        refused.
        """
        text = "\n".join(
            [
                "#if defined(Q_OS_WINDOWS)",
                "#endif",
                "#ifdef __MACH__",
                "#endif",
                "#if defined(BPR_BUILD_TESTING) || defined(_WIN64)",
                "#endif",
                "#if !defined(WIN32_LEAN_AND_MEAN)",
                "#endif",
                "#if defined(Q_OS_WIN) && defined(BPR_SQLITE_TESTING)",
                "#endif",
            ]
        )
        refused = [(line, name) for line, name, _ in unknown_platform_macros(text)]
        self.assertEqual(refused, [(1, "Q_OS_WINDOWS"), (3, "__MACH__"), (5, "_WIN64")])

    def test_every_host_names_its_qt_platform_and_only_decided_macros(
        self,
    ) -> None:
        """Each host defines Qt's name for itself, and nothing undecided.

        A host's macro outside `PLATFORM_MACROS` would read as undecided. A
        host missing Qt's name for itself polices its own `Q_OS_*` branches,
        which its compiler already reads, and skips their `#else`.
        """
        expected = {
            "Darwin": {"__APPLE__", "Q_OS_MACOS", "Q_OS_UNIX"},
            "Windows": {"_WIN32", "Q_OS_WIN"},
            "Linux": {"__linux__", "Q_OS_LINUX", "Q_OS_UNIX"},
        }
        for system, required in expected.items():
            with mock.patch("platform.system", return_value=system):
                macros = host_macros()
            self.assertLessEqual(required, macros, system)
            self.assertLessEqual(macros, PLATFORM_MACROS, system)


if __name__ == "__main__":
    unittest.main()
