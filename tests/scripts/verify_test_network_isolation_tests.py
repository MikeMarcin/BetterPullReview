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


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

from scripts.verify_test_network_isolation import (  # noqa: E402
    EXIT_VERDICT_TESTS,
    FORBIDDEN_EXIT_VERDICT_PROPERTIES,
    LIVE_CREDENTIAL_VARIABLES,
    LIVE_TEST_LABEL,
    LIVE_TEST_NAME,
    LiveImageIdentity,
    PRE_BOUNDARY_INJECTION_VARIABLES,
    RAW_REGISTRATION_OWNER,
    STAGE_ZERO_PRELOADER,
    STAGE_ZERO_PRELOADER_SOURCE,
    WINDOWS_UNCONTAINED_LABEL,
    WINDOWS_UNCONTAINED_TESTS,
    configured_failures,
    environment_injection_failures,
    live_image_failures,
    live_stage_zero_command,
    manifest_command,
    registration_inventory,
    uncontained_registration_count,
)
from scripts.run_network_isolated_test import write_profile  # noqa: E402


def write(root: pathlib.Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def scrub_property() -> dict[str, object]:
    return {
        "name": "ENVIRONMENT_MODIFICATION",
        "value": [
            f"{name}=unset:" for name in PRE_BOUNDARY_INJECTION_VARIABLES
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


class ConfiguredManifestTests(unittest.TestCase):
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
        self.assertEqual(
            set(WINDOWS_UNCONTAINED_TESTS.values()),
            {
                "dos-volume-name",
                "batch-stub",
                "ninja-named-pipe",
                "qprocess-named-pipe",
                "credential-manager",
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
    unittest.main()
