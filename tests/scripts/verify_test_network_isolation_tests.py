#!/usr/bin/env python3
"""Positive controls for the CTest process-boundary verifier."""

from __future__ import annotations

import hashlib
import pathlib
import os
import shutil
import subprocess
import sys
import time
import unittest
import unittest.mock


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

from scripts.verify_test_network_isolation import (  # noqa: E402
    AUTOMATED_TEST_BOUNDARY_ERASURE,
    AUTOMATED_TEST_BOUNDARY_MODIFICATION,
    AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST,
    AUTOMATED_TEST_BOUNDARY_VARIABLE,
    CHILD_EXIT_EXPRESSION,
    EXIT_VERDICT_TESTS,
    EXPECTS_FAILED_RUN_EXPRESSIONS,
    EXPECTS_FAILED_RUN_LABEL,
    EXPECTS_FAILED_RUN_TESTS,
    EXPECTS_NONZERO_EXIT_EXPRESSIONS,
    EXPECTS_NONZERO_EXIT_LABEL,
    EXPECTS_NONZERO_EXIT_TESTS,
    EXPECTS_WORKSPACE_RECOVERY_REFUSAL_LABEL,
    EXPECTS_WORKSPACE_RECOVERY_REFUSAL_TESTS,
    FORBIDDEN_EXIT_VERDICT_PROPERTIES,
    INHERITED_FAILURE_EXPRESSIONS,
    LIVE_CREDENTIAL_VARIABLES,
    LIVE_TEST_LABEL,
    LIVE_TEST_NAME,
    LiveImageIdentity,
    MACOS_NM,
    MACOS_UNSANDBOXED_LABEL,
    MACOS_UNSANDBOXED_TESTS,
    PRE_BOUNDARY_INJECTION_VARIABLES,
    RAW_REGISTRATION_OWNER,
    RUN_VERDICT_EXPRESSION,
    STAGE_ZERO_PRELOADER,
    STAGE_ZERO_PRELOADER_SOURCE,
    UNMEASURED_PIN_PREFIX,
    WINDOWS_UNCONTAINED_LABEL,
    WINDOWS_UNCONTAINED_REGISTRATION_LABEL,
    pin_registration_failures,
    WINDOWS_UNCONTAINED_TESTS,
    WORKSPACE_REFUSAL_EXPRESSION,
    boundary_marker_failures,
    composition_symbols,
    configured_failures,
    environment_injection_failures,
    exit_verdict_failures,
    inherited_verdict_failures,
    live_image_failures,
    live_stage_zero_command,
    manifest_command,
    raw_registration_failures,
    registration_inventory,
    uncontained_registration_count,
)
from scripts.run_network_isolated_test import write_profile  # noqa: E402
from skip_report import SkipReportingTestRunner  # noqa: E402


def write(root: pathlib.Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def scrub_property() -> dict[str, object]:
    # The exact ENVIRONMENT_MODIFICATION the registration seam writes: the
    # pre-boundary scrubs, and the BPR-0797 marker that tells the composed
    # process it is inside the automated test boundary.
    return {
        "name": "ENVIRONMENT_MODIFICATION",
        "value": [
            *(f"{name}=unset:" for name in PRE_BOUNDARY_INJECTION_VARIABLES),
            AUTOMATED_TEST_BOUNDARY_MODIFICATION,
        ],
    }


def live_scrub_property() -> dict[str, object]:
    return {
        "name": "ENVIRONMENT_MODIFICATION",
        "value": [
            f"{name}=unset:"
            for name in (
                *PRE_BOUNDARY_INJECTION_VARIABLES,
                *LIVE_CREDENTIAL_VARIABLES,
            )
        ],
    }


def live_command() -> list[str]:
    return live_stage_zero_command(
        pathlib.Path("/source"),
        pathlib.Path("/build/capsule.dmg"),
        "a" * 64,
        pathlib.Path("/private/mounts"),
        pathlib.Path("/private/approval"),
        LiveImageIdentity("b" * 64, "c" * 64),
    )


def protected_commands() -> dict[str, list[str]]:
    return {
        "architecture_boundaries": ["/sandbox", "/cmake", "-P", "/verify.cmake"],
        "architecture_boundary_contract": ["/sandbox", "/python", "/contract.py"],
    }


def protected_test(name: str, command: list[str]) -> dict[str, object]:
    return {
        "name": name,
        "command": command,
        "properties": [
            {"name": "LABELS", "value": ["bpr_network_isolated"]},
            scrub_property(),
        ],
    }


class RegistrationInventoryTests(unittest.TestCase):
    def test_exact_owner_and_isolated_registration_are_visible(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(
                root,
                "tests/CMakeLists.txt",
                "bpr_add_network_isolated_test(NAME safe COMMAND x)\n"
                "bpr_add_designated_live_mutation_test()\n",
            )
            self.assertEqual(
                registration_inventory(root),
                ([RAW_REGISTRATION_OWNER], 1, 1, 1),
            )

    def test_nested_repositories_do_not_contribute_registrations(self) -> None:
        for marker in ("directory", "file"):
            with self.subTest(git_marker=marker), contained_temporary_directory() as directory:
                root = pathlib.Path(directory)
                nested = root / ".claude" / "worktrees" / "other-checkout"
                nested.mkdir(parents=True)
                for checkout in (root, nested):
                    if marker == "directory":
                        (checkout / ".git").mkdir()
                    else:
                        (checkout / ".git").write_text(
                            "gitdir: /unused/worktree-metadata\n", encoding="utf-8"
                        )
                    write(checkout, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
                    write(
                        checkout,
                        "tests/CMakeLists.txt",
                        "bpr_add_network_isolated_test(NAME safe WINDOWS_UNCONTAINED COMMAND x)\n"
                        "bpr_add_designated_live_mutation_test()\n",
                    )
                self.assertEqual(
                    registration_inventory(root),
                    ([RAW_REGISTRATION_OWNER], 1, 1, 1),
                )
                self.assertEqual(uncontained_registration_count(root), 1)

    def test_hidden_directories_without_a_repository_marker_are_checked(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            rogue = ".claude/worktrees/unmarked/CMakeLists.txt"
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(root, rogue, "add_test(NAME bypass COMMAND x)\n")
            raw, _, _, _ = registration_inventory(root)
            self.assertIn(rogue, raw)
            self.assertTrue(raw_registration_failures(root, raw))

    def test_build_exclusions_are_relative_to_the_scan_root(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory) / "build" / "checkout"
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            for excluded in (".git", "build", "_build", "tests/build", ".mask/sessions"):
                write(root, f"{excluded}/CMakeLists.txt", "add_test(NAME generated COMMAND x)\n")
            self.assertEqual(
                registration_inventory(root), ([RAW_REGISTRATION_OWNER], 0, 0, 0)
            )

    def test_only_the_root_runtime_session_directory_is_excluded(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            for rogue in (".mask/skill-config/CMakeLists.txt", "tests/.mask/sessions/CMakeLists.txt"):
                write(root, rogue, "add_test(NAME bypass COMMAND x)\n")
            raw, _, _, _ = registration_inventory(root)
            self.assertIn(".mask/skill-config/CMakeLists.txt", raw)
            self.assertIn("tests/.mask/sessions/CMakeLists.txt", raw)

    def test_rogue_raw_registration_is_visible(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(root, "tests/CMakeLists.txt", "ADD_TEST(NAME bypass COMMAND x)\n")
            raw, _, _, _ = registration_inventory(root)
            self.assertIn("tests/CMakeLists.txt", raw)

    def test_second_or_argument_selected_live_registration_is_visible(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(
                root,
                "tests/CMakeLists.txt",
                "bpr_add_designated_live_mutation_test()\n"
                "bpr_add_designated_live_mutation_test(NAME arbitrary)\n",
            )
            _, _, live, exact_live = registration_inventory(root)
            self.assertEqual((live, exact_live), (2, 1))


# BPR-0802. The one macOS row that cannot run under the sandbox runner.
UNSANDBOXED_NAME = "gh_cli_git_smart_transport"
UNSANDBOXED = MACOS_UNSANDBOXED_TESTS[UNSANDBOXED_NAME]
UNSANDBOXED_RAW = (
    f"add_test(NAME {UNSANDBOXED_NAME} COMMAND {UNSANDBOXED.target})\n"
)


def unsandboxed_row(
    name: str = UNSANDBOXED_NAME,
    command: list[str] | None = None,
    labels: list[str] | None = None,
    modifications: dict[str, object] | None = None,
) -> dict[str, object]:
    return {
        "name": name,
        "command": (
            [f"/build/tests/github_contract/{UNSANDBOXED.executable}"]
            if command is None
            else command
        ),
        "properties": [
            {
                "name": "LABELS",
                "value": [MACOS_UNSANDBOXED_LABEL] if labels is None else labels,
            },
            scrub_property() if modifications is None else modifications,
        ],
    }


def no_composition_symbols(_: pathlib.Path) -> list[str]:
    return []


def composition_linked(_: pathlib.Path) -> list[str]:
    return ["0000000100001234 T bpr::app::github_application::start()"]


def uninspectable(_: pathlib.Path) -> None:
    return None


def unsandboxed_failures(
    tests: list[dict[str, object]],
    probe=no_composition_symbols,
    exceptions=MACOS_UNSANDBOXED_TESTS,
) -> list[str]:
    return configured_failures(
        {"tests": tests},
        ["/python", "-I", "-S", "/runner.py"],
        macos_runner=True,
        macos_unsandboxed=exceptions,
        symbol_probe=probe,
    )


class MacosUnsandboxedExceptionTests(unittest.TestCase):
    def test_enumerated_row_passes_with_marker_scrubs_and_no_composition(self) -> None:
        self.assertEqual(UNSANDBOXED.cause, "nested-sandbox")
        self.assertEqual(unsandboxed_failures([unsandboxed_row()]), [])

    # The property the exception rests on. A row that links the composition
    # can build `github_application`, and outside the sandbox that is a row
    # allowed to open a browser or reach `gh`.
    def test_enumerated_row_that_links_the_composition_is_reported(self) -> None:
        failures = unsandboxed_failures([unsandboxed_row()], probe=composition_linked)
        self.assertEqual(len(failures), 1)
        self.assertIn("links 1 bpr::app:: symbols", failures[0])

    def test_enumerated_row_that_cannot_be_inspected_is_reported(self) -> None:
        for probe in (uninspectable, None):
            with self.subTest(probe=probe):
                failures = unsandboxed_failures([unsandboxed_row()], probe=probe)
                self.assertEqual(len(failures), 1)
                self.assertIn("could not be inspected", failures[0])

    def test_unlisted_row_outside_the_runner_is_still_reported(self) -> None:
        failures = unsandboxed_failures(
            [unsandboxed_row(name="gh_cli_git_smart_transport_copy")]
        )
        self.assertTrue(any("does not start with" in value for value in failures))
        self.assertTrue(
            any("lacks the bpr_network_isolated label" in value for value in failures)
        )

    def test_exception_exists_on_no_other_backend(self) -> None:
        failures = configured_failures(
            {"tests": [unsandboxed_row()]},
            ["/launcher"],
            symbol_probe=no_composition_symbols,
        )
        self.assertTrue(any("does not start with" in value for value in failures))

    def test_wrapper_argument_or_other_executable_is_reported(self) -> None:
        executable = f"/build/tests/github_contract/{UNSANDBOXED.executable}"
        for command in (
            [executable, "--extra"],
            ["/usr/bin/env", executable],
            ["/build/src/app/app_better_pull_review"],
            None,
        ):
            with self.subTest(command=command):
                row = unsandboxed_row(command=command)
                if command is None:
                    row["command"] = None
                failures = unsandboxed_failures([row])
                self.assertEqual(len(failures), 1)
                self.assertIn("must run only its own executable", failures[0])

    def test_isolation_label_marker_and_scrubs_are_still_required(self) -> None:
        failures = unsandboxed_failures(
            [
                unsandboxed_row(
                    labels=["bpr_network_isolated"],
                    modifications={"name": "ENVIRONMENT_MODIFICATION", "value": []},
                )
            ]
        )
        self.assertTrue(any("expected only bpr_local_only_fixture" in value for value in failures))
        self.assertTrue(any("lacks the automated-test boundary marker" in value for value in failures))
        self.assertTrue(any("lacks pre-boundary environment scrubs" in value for value in failures))

    def test_enumerated_row_found_under_the_runner_is_a_stale_pin(self) -> None:
        failures = unsandboxed_failures(
            [
                unsandboxed_row(
                    command=[
                        "/python", "-I", "-S", "/runner.py",
                        "--loopback-port-count", "0", "--",
                        f"/build/{UNSANDBOXED.executable}",
                    ],
                )
            ]
        )
        self.assertTrue(any("retire it from the pinned set" in value for value in failures))

    def test_raw_registration_is_exact_to_the_enumerated_row(self) -> None:
        cases = {
            "enumerated": (UNSANDBOXED_RAW, True),
            "reflowed": (
                f"add_test(\n    NAME {UNSANDBOXED_NAME}\n    COMMAND {UNSANDBOXED.target}\n)\n",
                True,
            ),
            "second-row": (UNSANDBOXED_RAW + "add_test(NAME rogue COMMAND x)\n", False),
            "argument": (
                f"add_test(NAME {UNSANDBOXED_NAME} COMMAND {UNSANDBOXED.target} --x)\n",
                False,
            ),
            "other-target": (
                f"add_test(NAME {UNSANDBOXED_NAME} COMMAND app_better_pull_review)\n",
                False,
            ),
            # A call whose arguments hold parentheses is not parsed as a
            # registration, and must still be counted as a call.
            "unparsed-call": (
                UNSANDBOXED_RAW + 'add_test(NAME rogue COMMAND x "(y)")\n',
                False,
            ),
        }
        for name, (source, passes) in cases.items():
            with self.subTest(case=name):
                with contained_temporary_directory() as directory:
                    root = pathlib.Path(directory)
                    write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
                    write(root, UNSANDBOXED.source, source)
                    raw, _, _, _ = registration_inventory(root)
                    self.assertEqual(
                        raw_registration_failures(root, raw) == [], passes
                    )

    def test_raw_registration_in_an_unlisted_file_is_reported(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(root, UNSANDBOXED.source, UNSANDBOXED_RAW)
            write(root, "tests/end_to_end/CMakeLists.txt", UNSANDBOXED_RAW)
            raw, _, _, _ = registration_inventory(root)
            failures = raw_registration_failures(root, raw)
            self.assertEqual(len(failures), 1)
            self.assertIn("raw add_test ownership", failures[0])

    # The tree itself: the registration the exception pins is the one written.
    def test_repository_raw_registrations_match_the_exception(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[2]
        raw, _, _, _ = registration_inventory(root)
        self.assertIn(UNSANDBOXED.source, raw)
        self.assertEqual(raw_registration_failures(root, raw), [])

    # The real probe fails closed on a binary that holds no `bpr::` symbol at
    # all, so a stripped or substituted executable cannot pass as "links no
    # composition". The interpreter running this test is such a binary.
    @unittest.skipUnless(
        sys.platform == "darwin" and MACOS_NM.is_file(),
        "the composition symbol probe reads Mach-O tables with /usr/bin/nm",
    )
    def test_symbol_probe_fails_closed_without_any_bpr_symbol(self) -> None:
        self.assertIsNone(
            composition_symbols(pathlib.Path(os.path.realpath(sys.executable)))
        )
        self.assertIsNone(composition_symbols(pathlib.Path("/nonexistent/executable")))


class ConfiguredManifestTests(unittest.TestCase):
    def test_live_profile_is_part_of_exact_launch_command(self) -> None:
        args = (pathlib.Path("/source"), pathlib.Path("/build/capsule.dmg"), "a" * 64, pathlib.Path("/private/mounts"), pathlib.Path("/private/approval"), LiveImageIdentity("b" * 64, "c" * 64))
        selected = live_stage_zero_command(*args, profile="2")
        self.assertEqual(selected, [*live_command(), "--bpr-profile", "2"])
        row = {"name": LIVE_TEST_NAME, "command": selected, "properties": [{"name": "LABELS", "value": [LIVE_TEST_LABEL]}, {"name": "TIMEOUT", "value": 1800.0}, live_scrub_property()]}
        self.assertEqual(configured_failures({"tests": [row]}, ["/sandbox"], live_command=selected), [])
        changed = [*selected[:-1], "3"]
        row["command"] = changed
        self.assertTrue(any("exact runner/approval command" in value for value in configured_failures({"tests": [row]}, ["/sandbox"], live_command=selected)))
        for value in ("", "0", "02", "-1", "2 ", "18446744073709551616"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                live_stage_zero_command(*args, profile=value)

    def test_required_architecture_name_command_pairs_are_exact(self) -> None:
        commands = protected_commands()
        manifest = {
            "tests": [
                protected_test(name, command)
                for name, command in commands.items()
            ]
        }
        self.assertEqual(
            configured_failures(
                manifest,
                ["/sandbox"],
                required_commands=commands,
            ),
            [],
        )

    def test_renamed_or_absent_architecture_commands_are_rejected(self) -> None:
        commands = protected_commands()
        cases = (
            [
                protected_test(
                    "renamed_architecture_boundaries",
                    commands["architecture_boundaries"],
                ),
                protected_test(
                    "architecture_boundary_contract",
                    commands["architecture_boundary_contract"],
                ),
            ],
            [
                protected_test(
                    "architecture_boundary_contract",
                    commands["architecture_boundary_contract"],
                ),
            ],
            [
                protected_test(
                    "architecture_boundaries",
                    ["/sandbox", "/other"],
                ),
                protected_test(
                    "architecture_boundary_contract",
                    commands["architecture_boundary_contract"],
                ),
            ],
        )
        for tests in cases:
            with self.subTest(names=[test["name"] for test in tests]):
                failures = configured_failures(
                    {"tests": tests},
                    ["/sandbox"],
                    required_commands=commands,
                )
                self.assertTrue(
                    any("protected test architecture_boundaries" in value for value in failures)
                )

    def test_missing_prefix_and_label_are_both_failures(self) -> None:
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": "bypass",
                        "command": ["/usr/bin/python3", "test.py"],
                        "properties": [scrub_property()],
                    }
                ]
            },
            ["/usr/bin/sandbox-exec", "-f", "/profile"],
        )
        self.assertEqual(len(failures), 2)

    # The Windows exemption is exact in both directions and exists on no other
    # backend. Each case below would pass if the verifier merely tolerated a
    # missing prefix, which is the failure these tests exist to refuse.
    # [[BPR-1343]]. A WINDOWS_UNCONTAINED registration needs its pin on every
    # backend; an unmeasured pin satisfies every backend but Windows.
    def test_uncontained_registration_requires_a_pin_on_every_backend(self) -> None:
        def manifest(name: str) -> dict[str, Any]:
            return {
                "tests": [
                    {
                        "name": name,
                        "properties": [
                            {"name": "LABELS", "value": ["bpr_network_isolated", WINDOWS_UNCONTAINED_REGISTRATION_LABEL]},
                        ],
                    }
                ]
            }

        for windows in (False, True):
            failures = pin_registration_failures(manifest("application_unpinned_row"), windows=windows)
            self.assertEqual(len(failures), 1, failures)
            self.assertIn("without an entry in WINDOWS_UNCONTAINED_TESTS", failures[0])
        measured = next(name for name, pin in WINDOWS_UNCONTAINED_TESTS.items() if not pin.startswith("unmeasured"))
        for windows in (False, True):
            self.assertEqual(pin_registration_failures(manifest(measured), windows=windows), [])
        unlabelled = {"tests": [{"name": "application_unpinned_row", "properties": [{"name": "LABELS", "value": ["bpr_network_isolated"]}]}]}
        self.assertEqual(pin_registration_failures(unlabelled, windows=True), [])

    def test_unmeasured_pin_is_refused_only_on_windows(self) -> None:
        name = "application_unmeasured_row"
        manifest = {
            "tests": [
                {
                    "name": name,
                    "properties": [{"name": "LABELS", "value": ["bpr_network_isolated", WINDOWS_UNCONTAINED_REGISTRATION_LABEL]}],
                }
            ]
        }
        with unittest.mock.patch.dict(WINDOWS_UNCONTAINED_TESTS, {name: "unmeasured (registered on macOS)"}):
            self.assertEqual(pin_registration_failures(manifest, windows=False), [])
            failures = pin_registration_failures(manifest, windows=True)
            self.assertEqual(len(failures), 1, failures)
            self.assertIn("measure it contained on Windows", failures[0])

    def test_pinned_windows_test_passes_uncontained_only_with_its_label(self) -> None:
        name = sorted(WINDOWS_UNCONTAINED_TESTS)[0]
        self.assertEqual(
            configured_failures(
                {
                    "tests": [
                        {
                            "name": name,
                            "command": ["/python", "/test.py"],
                            "properties": [
                                {"name": "LABELS", "value": [WINDOWS_UNCONTAINED_LABEL]},
                                scrub_property(),
                            ],
                        }
                    ]
                },
                ["/launcher"],
                windows_uncontained=frozenset(WINDOWS_UNCONTAINED_TESTS),
            ),
            [],
        )
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": name,
                        "command": ["/python", "/test.py"],
                        "properties": [
                            {"name": "LABELS", "value": ["bpr_network_isolated"]},
                            scrub_property(),
                        ],
                    }
                ]
            },
            ["/launcher"],
            windows_uncontained=frozenset(WINDOWS_UNCONTAINED_TESTS),
        )
        self.assertTrue(any(f"lacks the {WINDOWS_UNCONTAINED_LABEL}" in value for value in failures))
        self.assertTrue(any("while running uncontained" in value for value in failures))

    def test_pinned_windows_test_found_contained_is_a_stale_pin(self) -> None:
        name = sorted(WINDOWS_UNCONTAINED_TESTS)[0]
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": name,
                        "command": ["/launcher", "/python", "/test.py"],
                        "properties": [
                            {"name": "LABELS", "value": [WINDOWS_UNCONTAINED_LABEL]},
                            scrub_property(),
                        ],
                    }
                ]
            },
            ["/launcher"],
            windows_uncontained=frozenset(WINDOWS_UNCONTAINED_TESTS),
        )
        self.assertTrue(any("retire it from the pinned set" in value for value in failures))

    def test_unpinned_test_cannot_claim_the_windows_exemption(self) -> None:
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": "arbitrary_uncontained",
                        "command": ["/python", "/test.py"],
                        "properties": [
                            {"name": "LABELS", "value": [WINDOWS_UNCONTAINED_LABEL]},
                            scrub_property(),
                        ],
                    }
                ]
            },
            ["/launcher"],
            windows_uncontained=frozenset(WINDOWS_UNCONTAINED_TESTS),
        )
        self.assertTrue(any("not in the pinned Windows set" in value for value in failures))
        self.assertTrue(any("does not start with" in value for value in failures))

    def test_windows_exemption_does_not_exist_on_other_backends(self) -> None:
        name = sorted(WINDOWS_UNCONTAINED_TESTS)[0]
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": name,
                        "command": ["/python", "/test.py"],
                        "properties": [
                            {"name": "LABELS", "value": [WINDOWS_UNCONTAINED_LABEL]},
                            scrub_property(),
                        ],
                    }
                ]
            },
            ["/sandbox"],
        )
        self.assertTrue(any("not in the pinned Windows set" in value for value in failures))
        self.assertTrue(any("does not start with" in value for value in failures))
        self.assertTrue(any("lacks the bpr_network_isolated label" in value for value in failures))

    def test_uncontained_keyword_count_is_pinned_to_the_set(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(
                root,
                "tests/CMakeLists.txt",
                "bpr_add_network_isolated_test(NAME a WINDOWS_UNCONTAINED COMMAND x)\n"
                "bpr_add_network_isolated_test(\n    NAME c\n    WINDOWS_UNCONTAINED\n    COMMAND x\n)\n"
                "bpr_add_network_isolated_test(NAME b COMMAND x)\n"
                "# WINDOWS_UNCONTAINED in a comment is not a registration\n"
                'cmake_parse_arguments(PARSE_ARGV 0 t "WINDOWS_UNCONTAINED" "" "")\n',
            )
            self.assertEqual(uncontained_registration_count(root), 2)
            self.assertLessEqual(uncontained_registration_count(root), len(WINDOWS_UNCONTAINED_TESTS))

    def test_pending_windows_inventory_grants_no_configured_exception(self) -> None:
        # [[BPR-1237]]. The inventory is empty now that every row is measured, so
        # the course plants one pending registration of its own, a name no pin has.
        source, name = "tests/end_to_end/CMakeLists.txt", "application_pending_probe"
        self.assertNotIn(name, WINDOWS_UNCONTAINED_TESTS)
        with unittest.mock.patch("scripts.verify_test_network_isolation.WINDOWS_PENDING_REGISTRATIONS", frozenset({(source, name)})), contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            write(root, source, f"bpr_add_network_isolated_test(NAME {name} WINDOWS_UNCONTAINED COMMAND x)\n")
            self.assertEqual(uncontained_registration_count(root), 1)
            self.assertEqual(uncontained_registration_count(root, exclude_pending=True), 0)
            write(root, "tests/unlisted/CMakeLists.txt", f"bpr_add_network_isolated_test(NAME {name} WINDOWS_UNCONTAINED COMMAND x)\n")
            self.assertEqual(uncontained_registration_count(root, exclude_pending=True), 1)
        failures = configured_failures(
            {"tests": [{"name": name, "command": ["/test"], "properties": [
                {"name": "LABELS", "value": [WINDOWS_UNCONTAINED_LABEL]}, scrub_property()
            ]}]},
            ["/launcher"], windows_uncontained=frozenset(WINDOWS_UNCONTAINED_TESTS),
        )
        self.assertTrue(any("not in the pinned" in value for value in failures))

    def test_manifest_command_names_the_configuration_when_given(self) -> None:
        # A multi-configuration generator's `ctest --show-only` lists nothing
        # until told which configuration; the first Windows Qt-graph gate saw
        # an empty manifest for exactly that reason (BPR-0432).
        ctest = pathlib.Path("/ctest")
        build = pathlib.Path("/build")
        self.assertEqual(
            manifest_command(ctest, build, "Debug"),
            [str(ctest), "--test-dir", str(build), "--build-config", "Debug", "--show-only=json-v1"],
        )
        self.assertEqual(
            manifest_command(ctest, build, None),
            [str(ctest), "--test-dir", str(build), "--show-only=json-v1"],
        )

    def test_every_pinned_exemption_names_a_measured_cause(self) -> None:
        # [[BPR-1093]]. An unmeasured pin ([[BPR-1343]]) names no cause yet; it
        # names the platform that registered the row, and nothing else.
        unmeasured = {value for value in WINDOWS_UNCONTAINED_TESTS.values() if value.startswith(UNMEASURED_PIN_PREFIX)}
        self.assertLessEqual(unmeasured, {UNMEASURED_PIN_PREFIX + platform + ")" for platform in ("Linux", "macOS")})
        self.assertEqual(
            set(WINDOWS_UNCONTAINED_TESTS.values()) - unmeasured,
            {
                "dos-volume-name",
                "batch-stub",
                "ninja-named-pipe",
                "qprocess-named-pipe",
                "credential-manager",
                # [[BPR-0872]], [[BPR-0873]]. Measured on the Windows station:
                # CreateProcess refused outright for a tool outside the granted
                # scopes, and content the repository points at but does not
                # contain.
                "child-process-denied",
                "repository-content-outside-checkout",
                # [[BPR-1072]]. Measured on the Windows station: a launcher
                # started inside the container is refused the Service Control
                # Manager before it reaches the profile.
                "service-manager-denied",
                # [[BPR-1525]]. Exact contained native storage, private-root
                # and isolated Git init refusals, distinct from DOS naming.
                "ancestor-control-denied",
                "owner-private-acl-denied",
                "git-reference-parent-denied",
                "msys-runtime-object",
                # [[BPR-1694]]. Exact R148/R150 Windows pairs pass
                # uncontained and report these three distinct contained
                # refusals. The pins name observations, not inferred syscalls.
                "agent-setup-default-read-refused",
                "private-test-root-refused",
                "persistent-recovery-open-unavailable",
                # RFC-BPR-0093 measured native loader refusals.
                "cmake-dll-initialization",
                "qt-dll-initialization",
            },
        )

    def test_disabled_graph_rejects_any_live_exception(self) -> None:
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": LIVE_TEST_NAME,
                        "command": live_command(),
                        "properties": [
                            {"name": "LABELS", "value": [LIVE_TEST_LABEL]},
                            {"name": "TIMEOUT", "value": 1800.0},
                            live_scrub_property(),
                        ],
                    }
                ]
            },
            ["/sandbox"],
        )
        self.assertTrue(
            any("configured while the live lane is disabled" in value for value in failures)
        )

    def test_exact_single_live_exception_passes(self) -> None:
        expected_live_command = live_command()
        self.assertEqual(
            configured_failures(
                {
                    "tests": [
                        {
                            "name": LIVE_TEST_NAME,
                            "command": expected_live_command,
                            "properties": [
                                {"name": "LABELS", "value": [LIVE_TEST_LABEL]},
                                {"name": "TIMEOUT", "value": 1800.0},
                                live_scrub_property(),
                            ],
                        },
                        {
                            "name": "safe",
                            "command": ["/sandbox", "/test"],
                            "properties": [
                                {"name": "LABELS", "value": ["bpr_network_isolated"]},
                                scrub_property(),
                            ],
                        },
                    ]
                },
                ["/sandbox"],
                live_command=expected_live_command,
            ),
            [],
        )

    def test_live_name_prefix_label_and_second_exception_are_exact(self) -> None:
        expected_live_command = live_command()
        hostile = (
            {
                "name": "arbitrary_live_name",
                "command": expected_live_command,
                "properties": [
                    {"name": "LABELS", "value": [LIVE_TEST_LABEL]},
                    {"name": "TIMEOUT", "value": 1800.0},
                    live_scrub_property(),
                ],
            },
            {
                "name": LIVE_TEST_NAME,
                "command": ["/fixture-supervisor", "--attended-launch", "--inventory", "/inventory", "--approval", "/approval"],
                "properties": [
                    {"name": "LABELS", "value": [LIVE_TEST_LABEL]},
                    live_scrub_property(),
                ],
            },
            {
                "name": LIVE_TEST_NAME,
                "command": expected_live_command,
                "properties": [
                    {"name": "LABELS", "value": ["arbitrary_live_label"]},
                    {"name": "TIMEOUT", "value": 1800.0},
                    live_scrub_property(),
                ],
            },
        )
        failures = configured_failures(
            {"tests": list(hostile)},
            ["/sandbox"],
            live_command=expected_live_command,
        )
        self.assertTrue(any("count is 2" in value for value in failures))
        self.assertTrue(any("only live_review_mutation_designated_target" in value for value in failures))
        self.assertTrue(any("exact runner/approval command" in value for value in failures))
        self.assertTrue(any("labels are" in value for value in failures))

    def test_live_timeout_and_process_exit_verdict_are_fixed(self) -> None:
        expected_live_command = live_command()
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": LIVE_TEST_NAME,
                        "command": expected_live_command,
                        "properties": [
                            {"name": "LABELS", "value": [LIVE_TEST_LABEL]},
                            {"name": "TIMEOUT", "value": 31.0},
                            {"name": "PASS_REGULAR_EXPRESSION", "value": ["OK"]},
                            live_scrub_property(),
                        ],
                    }
                ]
            },
            ["/sandbox"],
            live_command=expected_live_command,
        )
        self.assertTrue(any("1800-second timeout" in value for value in failures))
        self.assertTrue(any("process exit" in value for value in failures))

    def test_exact_inherited_failure_marker_is_ordinary_only(self) -> None:
        test = {
            "properties": [
                {
                    "name": "FAIL_REGULAR_EXPRESSION",
                    "value": list(INHERITED_FAILURE_EXPRESSIONS),
                }
            ]
        }
        for name in sorted(EXIT_VERDICT_TESTS):
            with self.subTest(name=name):
                self.assertEqual(
                    bool(exit_verdict_failures(test, name)), name == LIVE_TEST_NAME
                )

    def test_failure_marker_exception_rejects_altered_values(self) -> None:
        for value in (
            [], [".*"], ["PUBLIC_PATH_FAILED", "OK"],
            "PUBLIC_PATH_FAILED", ["PUBLIC_PATH_FAILED|OK"],
            # BPR-0890. The value before the child's exit status joined it: a
            # row still guarded against the printed verdict and not against an
            # abort in the shutdown tail is not the inherited value.
            ["PUBLIC_PATH_FAILED"],
            ["TEST_RUNNER_CHILD_EXIT_FAILED"],
            list(reversed(INHERITED_FAILURE_EXPRESSIONS)),
            # BPR-1050. The value before the workspace refusal joined it.
            ["PUBLIC_PATH_FAILED", "TEST_RUNNER_CHILD_EXIT_FAILED"],
        ):
            with self.subTest(value=value):
                self.assertTrue(exit_verdict_failures(
                    {"properties": [
                        {"name": "FAIL_REGULAR_EXPRESSION", "value": value}
                    ]},
                    "test_network_isolation_manifest",
                ))

    def test_all_verdict_override_properties_are_rejected(self) -> None:
        expected_live_command = live_command()
        for safety_name in sorted(EXIT_VERDICT_TESTS):
            for property_name in sorted(FORBIDDEN_EXIT_VERDICT_PROPERTIES):
                is_live = safety_name == LIVE_TEST_NAME
                command = expected_live_command if is_live else ["/sandbox", "/test"]
                properties = [
                    {
                        "name": "LABELS",
                        "value": [
                            LIVE_TEST_LABEL if is_live else "bpr_network_isolated"
                        ],
                    },
                    {"name": property_name, "value": ["OK"]},
                    live_scrub_property() if is_live else scrub_property(),
                ]
                if is_live:
                    properties.append({"name": "TIMEOUT", "value": 1800.0})
                with self.subTest(
                    safety_name=safety_name,
                    property_name=property_name,
                ):
                    failures = configured_failures(
                        {
                            "tests": [
                                {
                                    "name": safety_name,
                                    "command": command,
                                    "properties": properties,
                                }
                            ]
                        },
                        ["/sandbox"],
                        live_command=expected_live_command if is_live else None,
                    )
                    self.assertTrue(
                        any("process exit" in value for value in failures)
                    )

    def test_failure_marker_cannot_false_green_a_safety_test(self) -> None:
        safety_name = "test_network_isolation_manifest"
        self.assertIn(safety_name, EXIT_VERDICT_TESTS)
        failing_output = (
            "TEST_NETWORK_ISOLATION_FAILED:expected TEST_NETWORK_ISOLATION_OK marker"
        )
        self.assertIn("TEST_NETWORK_ISOLATION_OK", failing_output)
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": safety_name,
                        "command": ["/sandbox", "/verifier"],
                        "properties": [
                            {"name": "LABELS", "value": ["bpr_network_isolated"]},
                            {"name": "PASS_REGULAR_EXPRESSION", "value": ["TEST_NETWORK_ISOLATION_OK"]},
                            scrub_property(),
                        ],
                    }
                ]
            },
            ["/sandbox"],
        )
        self.assertTrue(any("process exit" in value for value in failures))

    def test_inherited_verdict_is_required_on_every_ordinary_row(self) -> None:
        good = {
            "name": "ordinary",
            "properties": [
                {"name": "LABELS", "value": ["bpr_network_isolated"]},
                {
                    "name": "FAIL_REGULAR_EXPRESSION",
                    "value": list(INHERITED_FAILURE_EXPRESSIONS),
                },
            ],
        }
        self.assertEqual(inherited_verdict_failures({"tests": [good]}), [])
        missing = {
            "name": "ordinary",
            "properties": [{"name": "LABELS", "value": ["bpr_network_isolated"]}],
        }
        self.assertEqual(
            inherited_verdict_failures({"tests": [missing]}),
            [
                "ordinary must carry the inherited fail-closed verdict "
                f"{INHERITED_FAILURE_EXPRESSIONS!r}; found None"
            ],
        )

    def test_row_level_replacement_of_the_inherited_verdict_is_rejected(self) -> None:
        # BPR-0890. `set_tests_properties(<row> PROPERTIES FAIL_REGULAR_EXPRESSION
        # ...)` runs after the registration seam and replaces the property
        # outright. This is the shape that passed the gate before the check
        # existed.
        for replacement in (
            ["x"],
            ["PUBLIC_PATH_FAILED"],
            [],
            ["PUBLIC_PATH_FAILED", "x"],
            [RUN_VERDICT_EXPRESSION, CHILD_EXIT_EXPRESSION],
        ):
            with self.subTest(replacement=replacement):
                failures = inherited_verdict_failures(
                    {
                        "tests": [
                            {
                                "name": "replaced",
                                "properties": [
                                    {
                                        "name": "LABELS",
                                        "value": ["bpr_network_isolated"],
                                    },
                                    {
                                        "name": "FAIL_REGULAR_EXPRESSION",
                                        "value": replacement,
                                    },
                                ],
                            }
                        ]
                    }
                )
                self.assertTrue(
                    any("inherited fail-closed verdict" in value for value in failures)
                )

    def test_expected_failure_rows_are_exempt_only_when_recorded(self) -> None:
        recorded = sorted(EXPECTS_FAILED_RUN_TESTS)[0]
        exempt = {
            "name": recorded,
            "properties": [
                {
                    "name": "LABELS",
                    "value": ["bpr_network_isolated", EXPECTS_FAILED_RUN_LABEL],
                },
                {
                    "name": "FAIL_REGULAR_EXPRESSION",
                    "value": list(EXPECTS_FAILED_RUN_EXPRESSIONS),
                },
            ],
        }
        self.assertEqual(inherited_verdict_failures({"tests": [exempt]}), [])
        # BPR-1050. The failed-run exemption covers the verdict entries only;
        # a failed-run row that also dropped the workspace refusal is wider
        # than the exemption it took.
        self.assertTrue(
            any(
                "must carry no verdict expression" in value
                for value in inherited_verdict_failures(
                    {
                        "tests": [
                            {
                                "name": recorded,
                                "properties": exempt["properties"][:1],
                            }
                        ]
                    }
                )
            )
        )
        unrecorded = {
            "name": "unrecorded_expected_failure",
            "properties": [
                {
                    "name": "LABELS",
                    "value": ["bpr_network_isolated", EXPECTS_FAILED_RUN_LABEL],
                }
            ],
        }
        failures = inherited_verdict_failures({"tests": [unrecorded]})
        self.assertTrue(
            any("without being recorded" in value for value in failures)
        )
        contradictory = {
            "name": recorded,
            "properties": [
                {
                    "name": "LABELS",
                    "value": ["bpr_network_isolated", EXPECTS_FAILED_RUN_LABEL],
                },
                {
                    "name": "FAIL_REGULAR_EXPRESSION",
                    "value": list(INHERITED_FAILURE_EXPRESSIONS),
                },
            ],
        }
        self.assertTrue(
            any(
                "must carry no verdict expression" in value
                for value in inherited_verdict_failures({"tests": [contradictory]})
            )
        )

    def test_rows_that_assert_their_own_non_zero_exit_keep_the_run_verdict(
        self,
    ) -> None:
        recorded = sorted(EXPECTS_NONZERO_EXIT_TESTS)[0]

        def row(name: str, value: object) -> dict[str, object]:
            properties: list[dict[str, object]] = [
                {
                    "name": "LABELS",
                    "value": ["bpr_network_isolated", EXPECTS_NONZERO_EXIT_LABEL],
                }
            ]
            if value is not None:
                properties.append(
                    {"name": "FAIL_REGULAR_EXPRESSION", "value": value}
                )
            return {"name": name, "properties": properties}

        self.assertEqual(
            inherited_verdict_failures(
                {"tests": [row(recorded, list(EXPECTS_NONZERO_EXIT_EXPRESSIONS))]}
            ),
            [],
        )
        # Dropping the run verdict as well is a wider exemption than the row
        # asked for, and dropping nothing means the row was never exempt.
        for value in (None, list(INHERITED_FAILURE_EXPRESSIONS)):
            with self.subTest(value=value):
                self.assertTrue(
                    any(
                        "must still carry" in failure
                        for failure in inherited_verdict_failures(
                            {"tests": [row(recorded, value)]}
                        )
                    )
                )
        self.assertTrue(
            any(
                "without being recorded" in failure
                for failure in inherited_verdict_failures(
                    {
                        "tests": [
                            row("unrecorded", list(EXPECTS_NONZERO_EXIT_EXPRESSIONS))
                        ]
                    }
                )
            )
        )
        # A recorded row that is configured without its label has lost the
        # keyword, which is the drift this pin exists to catch.
        self.assertTrue(
            any(
                "carries neither" in failure
                for failure in inherited_verdict_failures(
                    {
                        "tests": [
                            {
                                "name": recorded,
                                "properties": [
                                    {
                                        "name": "LABELS",
                                        "value": ["bpr_network_isolated"],
                                    },
                                    {
                                        "name": "FAIL_REGULAR_EXPRESSION",
                                        "value": list(INHERITED_FAILURE_EXPRESSIONS),
                                    },
                                ],
                            }
                        ]
                    }
                )
            )
        )

    def test_workspace_refusal_rows_drop_only_the_refusal_entry(self) -> None:
        # BPR-1050. A row whose subject is the refusal drops that one entry and
        # keeps the verdict it inherits; nothing else about it is exempt.
        recorded = sorted(EXPECTS_WORKSPACE_RECOVERY_REFUSAL_TESTS)[0]
        verdict = [RUN_VERDICT_EXPRESSION, CHILD_EXIT_EXPRESSION]

        def row(name: str, labels: list[str], value: object) -> dict[str, object]:
            properties: list[dict[str, object]] = [
                {"name": "LABELS", "value": ["bpr_network_isolated", *labels]}
            ]
            if value is not None:
                properties.append({"name": "FAIL_REGULAR_EXPRESSION", "value": value})
            return {"name": name, "properties": properties}

        refusal = [EXPECTS_WORKSPACE_RECOVERY_REFUSAL_LABEL]
        self.assertEqual(
            inherited_verdict_failures({"tests": [row(recorded, refusal, verdict)]}),
            [],
        )
        for value in (None, [], [RUN_VERDICT_EXPRESSION], list(INHERITED_FAILURE_EXPRESSIONS)):
            with self.subTest(value=value):
                self.assertTrue(
                    any(
                        "inherited fail-closed verdict" in failure
                        for failure in inherited_verdict_failures(
                            {"tests": [row(recorded, refusal, value)]}
                        )
                    )
                )
        self.assertTrue(
            any(
                "without being recorded" in failure
                for failure in inherited_verdict_failures(
                    {"tests": [row("unrecorded", refusal, verdict)]}
                )
            )
        )
        # Exact in one direction only: a recorded row configured without the
        # label, as on Windows where its section is compiled out, is held to
        # the ordinary rule and passes it.
        self.assertEqual(
            inherited_verdict_failures(
                {"tests": [row(recorded, [], list(INHERITED_FAILURE_EXPRESSIONS))]}
            ),
            [],
        )
        # Combined with the failed-run exemption, nothing is left to carry.
        failed_run = sorted(EXPECTS_FAILED_RUN_TESTS)[0]
        with unittest.mock.patch(
            "scripts.verify_test_network_isolation.EXPECTS_WORKSPACE_RECOVERY_REFUSAL_TESTS",
            {failed_run},
        ):
            self.assertEqual(
                inherited_verdict_failures(
                    {"tests": [row(failed_run, [EXPECTS_FAILED_RUN_LABEL, *refusal], None)]}
                ),
                [],
            )
        self.assertIn(WORKSPACE_REFUSAL_EXPRESSION, INHERITED_FAILURE_EXPRESSIONS)
        self.assertIn(WORKSPACE_REFUSAL_EXPRESSION, EXPECTS_NONZERO_EXIT_EXPRESSIONS)
        self.assertEqual(EXPECTS_FAILED_RUN_EXPRESSIONS, [WORKSPACE_REFUSAL_EXPRESSION])

    def test_live_lane_and_unsandboxed_exception_keep_process_exit(self) -> None:
        live = {"name": LIVE_TEST_NAME, "properties": []}
        self.assertEqual(inherited_verdict_failures({"tests": [live]}), [])
        unsandboxed_name = sorted(MACOS_UNSANDBOXED_TESTS)[0]
        bare = {"name": unsandboxed_name, "properties": []}
        self.assertEqual(
            inherited_verdict_failures(
                {"tests": [bare]}, macos_unsandboxed=MACOS_UNSANDBOXED_TESTS
            ),
            [],
        )
        pinned = {
            "name": unsandboxed_name,
            "properties": [
                {"name": "PASS_REGULAR_EXPRESSION", "value": ["OK"]},
            ],
        }
        self.assertTrue(
            any(
                "process exit as its only verdict" in value
                for value in inherited_verdict_failures(
                    {"tests": [pinned]}, macos_unsandboxed=MACOS_UNSANDBOXED_TESTS
                )
            )
        )

    def test_ctest_fails_a_row_that_prints_its_marker_and_then_exits_non_zero(
        self,
    ) -> None:
        # BPR-0890, the measurement the whole design rests on. CTest computes a
        # row's success as `!forceFail && (retVal == 0 ||
        # !RequiredRegularExpressions.empty())`, so a row with a
        # `PASS_REGULAR_EXPRESSION` ignores the exit status outright - which is
        # why an abort after `PUBLIC_PATH_OK` was printed kept the row green.
        # An error expression is evaluated independently of the pass list, so
        # this drives the real CTest on both property sets and shows that only
        # the inherited one refuses the run.
        ctest = shutil.which("ctest")
        if ctest is None:
            self.skipTest("ctest is not on PATH")
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            child = root / "abort_after_marker.py"
            child.write_text(
                "import os, sys\n"
                'sys.stdout.write("PUBLIC_PATH_OK\\n")\n'
                "sys.stdout.flush()\n"
                'sys.stdout.write("TEST_RUNNER_CHILD_EXIT:-6\\n")\n'
                'sys.stdout.write("TEST_RUNNER_CHILD_EXIT_FAILED:-6\\n")\n'
                "sys.stdout.flush()\n"
                "os._exit(70)\n",
                encoding="utf-8",
            )
            # [[BPR-0890]]. CMake reads a backslash in a quoted argument as an
            # escape, so a Windows interpreter path written raw produces a
            # CTestTestfile that lists no tests at all. The generated script
            # path beside it already uses forward slashes for the same reason.
            interpreter = pathlib.Path(sys.executable).as_posix()
            rows = [
                f'add_test(before "{interpreter}" "-I" "-S" "{child.as_posix()}")',
                'set_tests_properties(before PROPERTIES '
                'PASS_REGULAR_EXPRESSION "PUBLIC_PATH_OK" '
                'FAIL_REGULAR_EXPRESSION "PUBLIC_PATH_FAILED")',
                f'add_test(after "{interpreter}" "-I" "-S" "{child.as_posix()}")',
                'set_tests_properties(after PROPERTIES '
                'PASS_REGULAR_EXPRESSION "PUBLIC_PATH_OK" '
                'FAIL_REGULAR_EXPRESSION "'
                + ";".join(INHERITED_FAILURE_EXPRESSIONS)
                + '")',
            ]
            (root / "CTestTestfile.cmake").write_text(
                "\n".join(rows) + "\n", encoding="utf-8"
            )
            result = subprocess.run(
                [ctest, "--test-dir", str(root), "-N"],
                capture_output=True,
                text=True,
                errors="replace",
                check=False,
                timeout=120,
            )
            self.assertIn("Total Tests: 2", result.stdout)
            outcomes = {}
            for name in ("before", "after"):
                run = subprocess.run(
                    [ctest, "--test-dir", str(root), "-R", f"^{name}$"],
                    capture_output=True,
                    text=True,
                    errors="replace",
                    check=False,
                    timeout=120,
                )
                outcomes[name] = run.returncode
            self.assertEqual(
                outcomes["before"],
                0,
                "a pass expression alone still ignores a non-zero exit; if this "
                "ever fails, CTest changed and the error expression below may "
                "no longer be needed",
            )
            self.assertNotEqual(
                outcomes["after"],
                0,
                "the inherited error expression must refuse a run that printed "
                "its marker and then exited non-zero",
            )

    def test_pre_boundary_python_injection_is_rejected(self) -> None:
        test = {
            "properties": [
                {"name": "ENVIRONMENT", "value": ["PYTHONPATH=/tmp/inject"]},
                {
                    "name": "ENVIRONMENT_MODIFICATION",
                    "value": [
                        f"{name}=unset:"
                        for name in PRE_BOUNDARY_INJECTION_VARIABLES
                    ],
                },
            ]
        }
        self.assertEqual(
            environment_injection_failures(test, "injected"),
            ["injected sets pre-boundary variable PYTHONPATH"],
        )

    def test_versioned_framework_injection_is_rejected(self) -> None:
        test = {
            "properties": [
                {
                    "name": "ENVIRONMENT",
                    "value": ["DYLD_VERSIONED_FRAMEWORK_PATH=/tmp/inject"],
                },
                scrub_property(),
            ]
        }
        self.assertEqual(
            environment_injection_failures(test, "injected"),
            [
                "injected sets pre-boundary variable "
                "DYLD_VERSIONED_FRAMEWORK_PATH"
            ],
        )

    def test_missing_or_overridden_scrub_is_rejected(self) -> None:
        test = {
            "properties": [
                {
                    "name": "ENVIRONMENT_MODIFICATION",
                    "value": ["PYTHONPATH=set:/tmp/inject"],
                }
            ]
        }
        failures = environment_injection_failures(test, "injected")
        self.assertEqual(len(failures), 2)

    # BPR-0797. The marker is how a composed process learns it is inside the
    # automated boundary and may not bind the real system browser opener or
    # the real GitHub CLI transport. A row that reaches CTest without it has
    # to fail here, because the next place it shows up is a browser window on
    # whoever is running the suite.
    def test_seam_registered_row_carries_the_boundary_marker(self) -> None:
        self.assertIn(
            AUTOMATED_TEST_BOUNDARY_MODIFICATION,
            scrub_property()["value"],
        )
        self.assertEqual(
            configured_failures(
                {
                    "tests": [
                        {
                            "name": "safe",
                            "command": ["/sandbox", "/test"],
                            "properties": [
                                {
                                    "name": "LABELS",
                                    "value": ["bpr_network_isolated"],
                                },
                                scrub_property(),
                            ],
                        }
                    ]
                },
                ["/sandbox"],
            ),
            [],
        )

    def test_row_without_the_boundary_marker_is_rejected(self) -> None:
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": "unmarked",
                        "command": ["/sandbox", "/test"],
                        "properties": [
                            {"name": "LABELS", "value": ["bpr_network_isolated"]},
                            {
                                "name": "ENVIRONMENT_MODIFICATION",
                                "value": [
                                    f"{name}=unset:"
                                    for name in PRE_BOUNDARY_INJECTION_VARIABLES
                                ],
                            },
                        ],
                    }
                ]
            },
            ["/sandbox"],
        )
        self.assertEqual(
            failures,
            [
                "unmarked lacks the automated-test boundary marker "
                f"{AUTOMATED_TEST_BOUNDARY_MODIFICATION}"
            ],
        )

    # The property the marker lives in is the check. Measured on CMake 4.4.3:
    # a marker appended to ENVIRONMENT is erased outright by the next
    # `set_tests_properties(<row> PROPERTIES ENVIRONMENT ...)`, and over a
    # hundred rows write one. A marker there is not a marker.
    def test_boundary_marker_in_environment_is_not_accepted(self) -> None:
        failures = boundary_marker_failures(
            {
                "properties": [
                    {
                        "name": "ENVIRONMENT",
                        "value": [f"{AUTOMATED_TEST_BOUNDARY_VARIABLE}=1"],
                    },
                    {
                        "name": "ENVIRONMENT_MODIFICATION",
                        "value": [
                            f"{name}=unset:"
                            for name in PRE_BOUNDARY_INJECTION_VARIABLES
                        ],
                    },
                ]
            },
            "erasable",
        )
        self.assertEqual(
            failures,
            [
                "erasable carries the boundary marker in ENVIRONMENT, where a "
                "row-level write erases it",
                "erasable lacks the automated-test boundary marker "
                f"{AUTOMATED_TEST_BOUNDARY_MODIFICATION}",
            ],
        )

    def test_altered_boundary_marker_value_is_rejected(self) -> None:
        for entry in (
            f"{AUTOMATED_TEST_BOUNDARY_VARIABLE}=unset:",
            f"{AUTOMATED_TEST_BOUNDARY_VARIABLE}=set:0",
            f"{AUTOMATED_TEST_BOUNDARY_VARIABLE}=string_append:1",
        ):
            with self.subTest(entry=entry):
                failures = boundary_marker_failures(
                    {
                        "properties": [
                            {
                                "name": "ENVIRONMENT_MODIFICATION",
                                "value": [
                                    *(
                                        f"{name}=unset:"
                                        for name in PRE_BOUNDARY_INJECTION_VARIABLES
                                    ),
                                    entry,
                                ],
                            }
                        ]
                    },
                    "altered",
                )
                self.assertEqual(
                    failures,
                    [
                        "altered lacks the automated-test boundary marker "
                        f"{AUTOMATED_TEST_BOUNDARY_MODIFICATION}",
                        f"altered modifies the boundary marker: {entry}",
                    ],
                )

    # The product-run row is the one row allowed to stand outside the boundary,
    # and it does so by erasing the seam's marker after the seam wrote it. It
    # is named, so a second row that copied the erasure would still be
    # rejected, and ordered, so an erasure the seam's own `set:1` then undid
    # would be rejected too.
    def test_product_run_row_erases_the_marker_after_the_seam(self) -> None:
        def row(entries: list[str]) -> dict[str, object]:
            return {
                "properties": [
                    {"name": "ENVIRONMENT_MODIFICATION", "value": entries},
                ]
            }

        scrubs = [f"{name}=unset:" for name in PRE_BOUNDARY_INJECTION_VARIABLES]
        self.assertEqual(
            boundary_marker_failures(
                row(
                    [
                        *scrubs,
                        AUTOMATED_TEST_BOUNDARY_MODIFICATION,
                        AUTOMATED_TEST_BOUNDARY_ERASURE,
                    ]
                ),
                AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST,
            ),
            [],
        )
        self.assertEqual(
            boundary_marker_failures(
                row([*scrubs, AUTOMATED_TEST_BOUNDARY_MODIFICATION]),
                AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST,
            ),
            [
                f"{AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST} is the product-run "
                "row and must erase the boundary marker with "
                f"{AUTOMATED_TEST_BOUNDARY_ERASURE}"
            ],
        )
        self.assertEqual(
            boundary_marker_failures(
                row(
                    [
                        *scrubs,
                        AUTOMATED_TEST_BOUNDARY_ERASURE,
                        AUTOMATED_TEST_BOUNDARY_MODIFICATION,
                    ]
                ),
                AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST,
            ),
            [
                f"{AUTOMATED_TEST_BOUNDARY_PRODUCT_RUN_TEST} erases the boundary "
                "marker before the seam sets it, so the row would run inside "
                "the boundary after all"
            ],
        )
        self.assertEqual(
            boundary_marker_failures(
                row(
                    [
                        *scrubs,
                        AUTOMATED_TEST_BOUNDARY_MODIFICATION,
                        AUTOMATED_TEST_BOUNDARY_ERASURE,
                    ]
                ),
                "copied",
            ),
            [
                "copied modifies the boundary marker: "
                f"{AUTOMATED_TEST_BOUNDARY_ERASURE}"
            ],
        )

    # The live lane is the one row that must not carry the marker. It drives
    # the shipped application against real GitHub on purpose, from an image
    # this gate already requires to be built with BUILD_TESTING=OFF, so a
    # marker there would hand it the recording stand-ins it exists to avoid.
    def test_live_lane_is_not_asked_for_the_boundary_marker(self) -> None:
        expected_live_command = live_command()
        self.assertNotIn(
            AUTOMATED_TEST_BOUNDARY_MODIFICATION,
            live_scrub_property()["value"],
        )
        self.assertEqual(
            configured_failures(
                {
                    "tests": [
                        {
                            "name": LIVE_TEST_NAME,
                            "command": expected_live_command,
                            "properties": [
                                {"name": "LABELS", "value": [LIVE_TEST_LABEL]},
                                {"name": "TIMEOUT", "value": 1800.0},
                                live_scrub_property(),
                            ],
                        }
                    ]
                },
                ["/sandbox"],
                live_command=expected_live_command,
            ),
            [],
        )

    # Verifier and seam have to name the same entry, in the same property, or
    # the gate checks a string nothing writes. One literal, one place, both
    # files. Whitespace is folded so reflowing the registration cannot break
    # this, though folding the marker into the pre-boundary scrub list would:
    # it is registered and commented on its own because it is not a scrub.
    def test_registration_seam_writes_the_exact_demanded_marker(self) -> None:
        module = (
            pathlib.Path(__file__).resolve().parents[2]
            / "cmake"
            / "BprTestIsolation.cmake"
        )
        source = " ".join(module.read_text(encoding="utf-8").split())
        self.assertEqual(
            source.count(
                "PROPERTY ENVIRONMENT_MODIFICATION "
                f'"{AUTOMATED_TEST_BOUNDARY_MODIFICATION}"'
            ),
            1,
        )


@unittest.skipUnless(
    hasattr(os, "geteuid"),
    "live-image identity is defined in POSIX owner terms and the designated "
    "live lane runs only where os.geteuid exists",
)
class LiveImageIdentityTests(unittest.TestCase):
    def make_fixture(
        self, root: pathlib.Path
    ) -> tuple[pathlib.Path, str, pathlib.Path, pathlib.Path, pathlib.Path]:
        sources = {
            "src/app/CMakeLists.txt": "set(app safe)\n",
            "cmake/BprDesignatedLiveStageZero.cmake": "set(loader safe)\n",
            "cmake/BprTestIsolation.cmake": "set(isolation safe)\n",
            "scripts/designated_live_macos_capsule.py": "print('safe stage zero')\n",
        }
        manifest = ""
        for relative, contents in sources.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            manifest += f"{relative}={digest}\n"
        source_digest = hashlib.sha256(manifest.encode("utf-8")).hexdigest()
        build = root / "build" / "release"
        image_parent = build / "designated-live" / source_digest
        image_parent.mkdir(parents=True)
        image = image_parent / "capsule.dmg"
        image.write_bytes(b"read-only-image")
        image.chmod(0o600)
        image_sha256 = hashlib.sha256(image.read_bytes()).hexdigest()
        source_manifest = image_parent / "source-manifest.txt"
        source_manifest.write_text(manifest, encoding="utf-8", newline="\n")
        source_manifest.chmod(0o600)
        mount_parent = root / "private-mounts"
        mount_parent.mkdir(mode=0o700)
        approval_parent = root / "private-approval"
        approval_parent.mkdir(mode=0o700)
        approval = approval_parent / "approval"
        approval.write_text("not a real approval\n", encoding="utf-8")
        approval.chmod(0o600)
        (build / "CMakeCache.txt").write_text(
            "BUILD_TESTING:BOOL=OFF\n"
            "BPR_BUILD_QT:BOOL=ON\n"
            "BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE:BOOL=ON\n"
            "CMAKE_BUILD_TYPE:STRING=Release\n"
            f"CMAKE_HOME_DIRECTORY:INTERNAL={root.resolve()}\n",
            encoding="utf-8",
        )
        source_time = max((root / relative).stat().st_mtime_ns for relative in sources)
        for artifact in (image, source_manifest):
            os.utime(artifact, ns=(source_time + 1_000_000, source_time + 1_000_000))
        return (
            image.resolve(),
            image_sha256,
            mount_parent.resolve(),
            build.resolve(),
            approval.resolve(),
        )

    def test_exact_fresh_tests_disabled_image_passes(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            image, digest, mounts, build, approval = self.make_fixture(root)
            self.assertEqual(
                live_image_failures(
                    image, digest, mounts, build, root, approval
                ),
                [],
            )

    def test_digest_stale_and_tests_enabled_images_fail(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            image, digest, mounts, build, approval = self.make_fixture(root)
            cache = build / "CMakeCache.txt"
            cache.write_text(
                cache.read_text(encoding="utf-8").replace(
                    "BUILD_TESTING:BOOL=OFF", "BUILD_TESTING:BOOL=ON"
                ),
                encoding="utf-8",
            )
            os.utime(image, ns=(1, 1))
            failures = live_image_failures(
                image, "0" * 64, mounts, build, root, approval
            )
            self.assertTrue(any("image digest" in value for value in failures))
            self.assertTrue(any("BUILD_TESTING:BOOL" in value for value in failures))
            self.assertTrue(any("predates" in value for value in failures))

            alternate = root / "fixture-image.dmg"
            alternate.write_bytes(image.read_bytes())
            alternate.chmod(0o600)
            failures = live_image_failures(
                alternate.resolve(), digest, mounts, build, root, approval
            )
            self.assertTrue(any("outside" in value for value in failures))

    def test_source_manifest_and_launcher_drift_fail(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            image, digest, mounts, build, approval = self.make_fixture(root)
            launcher = root / "scripts/designated_live_macos_capsule.py"
            launcher.write_text("print('substitute')\n", encoding="utf-8")
            failures = live_image_failures(
                image, digest, mounts, build, root, approval
            )
            self.assertTrue(any("current source differs" in value for value in failures))

    def test_registration_rejects_plain_supervisor_selection_directly(
        self,
    ) -> None:
        cmake = shutil.which("cmake") or "cmake"
        module = (
            pathlib.Path(__file__).resolve().parents[2]
            / "cmake"
            / "BprTestIsolation.cmake"
        )

        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory).resolve()
            preloader = pathlib.Path(__file__).resolve().parents[2] / "cmake" / "BprDesignatedLiveStageZero.cmake"
            copied = root / "cmake" / preloader.name
            copied.parent.mkdir(parents=True)
            shutil.copy2(preloader, copied)
            result = subprocess.run(
                [
                    cmake,
                    "-DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON",
                    "-DBPR_BUILD_QT=ON",
                    "-DBPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR=/plain/supervisor",
                    "-DBPR_DESIGNATED_LIVE_MUTATION_INVENTORY=/plain/inventory",
                    f"-DPROJECT_SOURCE_DIR={root}",
                    "-P",
                    str(module),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "plain supervisor/inventory selection is removed",
                result.stdout + result.stderr,
            )

    def test_manifest_swap_after_capture_cannot_authenticate_substitute(self) -> None:
        cmake = shutil.which("cmake") or "cmake"
        repository = pathlib.Path(__file__).resolve().parents[2]
        module_source = (repository / "cmake" / "BprTestIsolation.cmake").read_text(
            encoding="utf-8"
        )
        capture = """\
    file(
        READ
        "${bpr_designated_live_source_manifest}"
        bpr_designated_live_source_manifest_bytes
        LIMIT 1048577
    )
"""
        self.assertEqual(module_source.count(capture), 1)

        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory).resolve()
            ready = root / "barrier-ready"
            release = root / "barrier-release"
            barrier = root / "barrier.py"
            barrier.write_text(
                "import pathlib, sys, time\n"
                "pathlib.Path(sys.argv[1]).write_text('ready')\n"
                "while not pathlib.Path(sys.argv[2]).exists():\n"
                "    time.sleep(0.01)\n",
                encoding="utf-8",
            )
            barrier_call = (
                capture
                + "    execute_process(\n"
                + f'        COMMAND "{sys.executable}" "{barrier}" "{ready}" "{release}"\n'
                + "        COMMAND_ERROR_IS_FATAL ANY\n"
                + "    )\n"
            )
            guarded_module = module_source.replace(capture, barrier_call, 1)

            source_a = "print('captured A')\n"
            source_b = "print('substitute B')\n"
            sources = {
                "src/app/CMakeLists.txt": "set(app safe)\n",
                "cmake/BprDesignatedLiveStageZero.cmake": (
                    repository / "cmake" / "BprDesignatedLiveStageZero.cmake"
                ).read_text(encoding="utf-8"),
                "cmake/BprTestIsolation.cmake": guarded_module,
                "scripts/designated_live_macos_capsule.py": source_a,
            }
            manifest_a = ""
            for relative, contents in sources.items():
                write(root, relative, contents)
                digest = hashlib.sha256(contents.encode("utf-8")).hexdigest()
                manifest_a += f"{relative}={digest}\n"
            manifest_b = manifest_a.replace(
                hashlib.sha256(source_a.encode("utf-8")).hexdigest(),
                hashlib.sha256(source_b.encode("utf-8")).hexdigest(),
                1,
            )
            write(root, "CMakeLists.txt", "cmake_minimum_required(VERSION 3.25)\n")
            write(root, "CMakePresets.json", "{}\n")
            write(root, "cmake/sandbox/macos-network-deny.sb", "(version 1)\n")
            write(root, "scripts/run_network_isolated_test.py", "# fixture\n")

            source_identity = hashlib.sha256(manifest_a.encode("ascii")).hexdigest()
            build = root / "build" / "release"
            artifact_root = build / "designated-live" / source_identity
            artifact_root.mkdir(parents=True)
            image = artifact_root / "capsule.dmg"
            image.write_bytes(b"captured image")
            image.chmod(0o600)
            manifest = artifact_root / "source-manifest.txt"
            manifest.write_text(manifest_a, encoding="ascii", newline="\n")
            manifest.chmod(0o600)
            replacement_manifest = root / "replacement-manifest"
            replacement_manifest.write_text(manifest_b, encoding="ascii", newline="\n")
            replacement_source = root / "replacement-stage-zero"
            replacement_source.write_text(source_b, encoding="utf-8")
            mount_parent = root / "mounts"
            mount_parent.mkdir(mode=0o700)
            (build / "CMakeCache.txt").write_text(
                "BUILD_TESTING:BOOL=OFF\n"
                "BPR_BUILD_QT:BOOL=ON\n"
                "BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE:BOOL=ON\n"
                "CMAKE_BUILD_TYPE:STRING=Release\n"
                f"CMAKE_HOME_DIRECTORY:INTERNAL={root}\n",
                encoding="utf-8",
            )
            newest_source = max(
                path.stat().st_mtime_ns
                for path in (root / "src", root / "cmake")
                for path in path.rglob("*")
                if path.is_file()
            )
            for artifact in (image, manifest):
                os.utime(
                    artifact,
                    ns=(newest_source + 1_000_000, newest_source + 1_000_000),
                )

            command = [
                cmake,
                "-DAPPLE=TRUE",
                "-DBPR_BUILD_QT=ON",
                "-DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON",
                f"-DBPR_DESIGNATED_LIVE_MUTATION_IMAGE={image}",
                "-DBPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256="
                + hashlib.sha256(image.read_bytes()).hexdigest(),
                f"-DBPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT={mount_parent}",
                f"-DBPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR={build}",
                "-DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/dev/null",
                f"-DPROJECT_SOURCE_DIR={root}",
                "-P",
                str(root / "cmake" / "BprTestIsolation.cmake"),
            ]
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for _ in range(300):
                if ready.exists() or process.poll() is not None:
                    break
                time.sleep(0.01)
            self.assertTrue(ready.exists())
            replacement_manifest.replace(manifest)
            replacement_source.replace(
                root / "scripts" / "designated_live_macos_capsule.py"
            )
            release.write_text("continue", encoding="utf-8")
            stdout, stderr = process.communicate(timeout=30)
            self.assertNotEqual(process.returncode, 0)
            output = stdout + stderr
            self.assertIn("current source differs from producer entry", output)
            self.assertIn("scripts/designated_live_macos_capsule.py", output)


@unittest.skipUnless(
    pathlib.Path("/usr/bin/python3").is_file(),
    "the protected stage-zero loader has a fixed macOS interpreter",
)
class ProtectedStageZeroLoaderTests(unittest.TestCase):
    def protected_command(
        self,
        launcher: pathlib.Path,
        expected: str,
        *arguments: pathlib.Path,
    ) -> list[str]:
        return [
            "/usr/bin/python3",
            "-I",
            "-S",
            "-c",
            STAGE_ZERO_PRELOADER,
            str(launcher),
            expected,
            *(str(argument) for argument in arguments),
        ]

    def test_verifier_and_cmake_use_identical_literal_preloader(self) -> None:
        module = (
            pathlib.Path(__file__).resolve().parents[2]
            / "cmake"
            / "BprDesignatedLiveStageZero.cmake"
        )
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            output = root / "literal"
            harness = root / "preloader.cmake"
            harness.write_text(
                f'include("{module}")\n'
                'file(WRITE "${OUTPUT}" '
                '"${BPR_DESIGNATED_LIVE_STAGE_ZERO_PRELOADER}")\n',
                encoding="utf-8",
            )
            result = subprocess.run(
                [
                    shutil.which("cmake") or "cmake",
                    f"-DOUTPUT={output}",
                    "-P",
                    str(harness),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"), STAGE_ZERO_PRELOADER)

    def test_launcher_replacement_is_refused_before_any_effect(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            launcher = root / "stage-zero.py"
            marker = root / "effect"
            source_a = b"import pathlib, sys\npathlib.Path(sys.argv[1]).write_text('A')\n"
            source_b = b"import pathlib, sys\npathlib.Path(sys.argv[1]).write_text('B')\n"
            launcher.write_bytes(source_a)
            expected = hashlib.sha256(source_a).hexdigest()
            replacement = root / "replacement.py"
            replacement.write_bytes(source_b)
            replacement.replace(launcher)

            result = subprocess.run(
                self.protected_command(launcher, expected, marker),
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("launcher digest mismatch", result.stderr)
            self.assertFalse(marker.exists())

    def test_symlink_substitution_is_refused_before_any_effect(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            target = root / "target.py"
            marker = root / "effect"
            source = b"import pathlib, sys\npathlib.Path(sys.argv[1]).write_text('ran')\n"
            target.write_bytes(source)
            launcher = root / "stage-zero.py"
            launcher.symlink_to(target)

            result = subprocess.run(
                self.protected_command(
                    launcher,
                    hashlib.sha256(source).hexdigest(),
                    marker,
                ),
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(marker.exists())

    def test_same_inode_mutation_after_capture_executes_captured_bytes(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            launcher = root / "stage-zero.py"
            ready = root / "ready"
            release = root / "release"
            outcome = root / "outcome"
            source_a = (
                b"import pathlib, sys\n"
                b"pathlib.Path(sys.argv[3]).write_text('A')\n"
            )
            source_b = (
                b"import pathlib, sys\n"
                b"pathlib.Path(sys.argv[3]).write_text('B')\n"
            )
            launcher.write_bytes(source_a)
            exec_anchor = "sys.argv = [path, *sys.argv[3:]]"
            self.assertEqual(STAGE_ZERO_PRELOADER_SOURCE.count(exec_anchor), 1)
            synchronized_source = STAGE_ZERO_PRELOADER_SOURCE.replace(
                exec_anchor,
                "import time\n"
                "with open(sys.argv[3], 'w', encoding='utf-8') as signal:\n"
                "    signal.write('ready')\n"
                "while not os.path.exists(sys.argv[4]):\n"
                "    time.sleep(0.01)\n"
                + exec_anchor,
                1,
            )
            synchronized_preloader = f"exec({synchronized_source!r})"
            command = self.protected_command(
                launcher,
                hashlib.sha256(source_a).hexdigest(),
                ready,
                release,
                outcome,
            )
            command[4] = synchronized_preloader
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for _ in range(100):
                if ready.exists():
                    break
                if process.poll() is not None:
                    break
                time.sleep(0.01)
            self.assertTrue(ready.exists())
            # The derived exact preloader is paused after its identity/hash
            # checks and before argv setup/compile-exec. write_bytes truncates
            # and rewrites the same inode; release then proves it executes the
            # bytes already captured instead of reopening the path.
            launcher.write_bytes(source_b)
            release.write_text("continue", encoding="utf-8")
            stdout, stderr = process.communicate(timeout=30)
            self.assertEqual(process.returncode, 0, stdout + stderr)
            self.assertEqual(outcome.read_text(encoding="utf-8"), "A")


class ConfiguredManifestAndProfileTests(unittest.TestCase):
    def test_derived_profile_contains_only_exact_runner_authority(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            template = root / "template.sb"
            output = root / "derived.sb"
            # The profile writer is macOS-only, and the fixture root it is
            # handed there is always a POSIX path. Deriving one from the host's
            # own temporary directory made this test hand it a Windows path on
            # Windows, which seatbelt_string refuses by design: the refusal was
            # right and the fixture was wrong. Only the string form of this
            # argument reaches the profile, so a pure POSIX path states the
            # production shape on every host.
            fixture = pathlib.PurePosixPath("/private/tmp/bpr-fixture")
            template.write_text(
                "(version 1)\n(allow default)\n"
                "(deny network-outbound network-inbound network-bind)\n",
                encoding="utf-8",
            )
            write_profile(template, output, fixture, [43123, 43124])
            text = output.read_text(encoding="utf-8")
            self.assertIn(f'(subpath "{fixture}")', text)
            self.assertIn('(remote ip "localhost:43123")', text)
            self.assertIn('(local ip "localhost:43124")', text)
            self.assertNotIn("localhost:*", text)
            self.assertNotIn('(subpath "/")', text)

    # A fixture root that could close the quoted seatbelt string, or that is
    # not anchored at all, must be refused rather than escaped into the
    # profile. Both shapes are unsafe on every host, so unlike the case above
    # this control asserts the refusal everywhere.
    def test_derived_profile_refuses_an_unsafe_fixture_root(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            template = root / "template.sb"
            template.write_text(
                "(version 1)\n(allow default)\n"
                "(deny network-outbound network-inbound network-bind)\n",
                encoding="utf-8",
            )
            for unsafe in (
                pathlib.PurePosixPath('/private/tmp/bpr"escape'),
                pathlib.PurePosixPath("relative/fixture"),
            ):
                with self.assertRaisesRegex(SystemExit, "125"):
                    write_profile(template, root / "derived.sb", unsafe, [])

    def test_profile_template_cannot_pregrant_network_authority(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            template = root / "template.sb"
            template.write_text(
                '(version 1)\n(allow network-outbound (remote ip "localhost:*"))\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(SystemExit, "125"):
                write_profile(template, root / "derived.sb", root, [])

    def test_exact_prefix_and_label_pass(self) -> None:
        self.assertEqual(
            configured_failures(
                {
                    "tests": [
                        {
                            "name": "safe",
                            "command": [
                                "/usr/bin/python3",
                                "-I",
                                "-S",
                                "/runner.py",
                                "--loopback-port-count",
                                "0",
                                "--",
                                "/usr/bin/python3",
                            ],
                            "properties": [
                                {
                                    "name": "LABELS",
                                    "value": ["bpr_network_isolated"],
                                },
                                scrub_property(),
                            ],
                        }
                    ]
                },
                ["/usr/bin/python3", "-I", "-S", "/runner.py"],
                macos_runner=True,
            ),
            [],
        )

    def test_designated_public_observer_has_one_loopback_authority(self) -> None:
        self.assertEqual(
            configured_failures(
                {
                    "tests": [
                        {
                            "name": "designated_live_public_observer_fixture",
                            "command": [
                                "/usr/bin/python3",
                                "-I",
                                "-S",
                                "/runner.py",
                                "--loopback-port-count",
                                "1",
                                "--",
                                "/usr/bin/python3",
                            ],
                            "properties": [
                                {
                                    "name": "LABELS",
                                    "value": [
                                        "bpr_loopback_fixture",
                                        "bpr_network_isolated",
                                    ],
                                },
                                scrub_property(),
                            ],
                        }
                    ]
                },
                ["/usr/bin/python3", "-I", "-S", "/runner.py"],
                macos_runner=True,
            ),
            [],
        )

    def test_runner_authority_requires_exact_count_delimiter_and_label(self) -> None:
        failures = configured_failures(
            {
                "tests": [
                    {
                        "name": "native_git_smart_transport_fixture",
                        "command": [
                            "/usr/bin/python3",
                            "-I",
                            "-S",
                            "/runner.py",
                            "--loopback-port-count",
                            "1",
                            "/bin/true",
                        ],
                        "properties": [
                            {
                                "name": "LABELS",
                                "value": [
                                    "bpr_network_isolated",
                                    "bpr_loopback_fixture",
                                ],
                            },
                            scrub_property(),
                        ],
                    }
                ]
            },
            ["/usr/bin/python3", "-I", "-S", "/runner.py"],
            macos_runner=True,
        )
        self.assertEqual(len(failures), 1)


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
