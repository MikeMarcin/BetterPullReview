#!/usr/bin/env python3
"""Positive controls for the CTest process-boundary verifier."""

from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from scripts.verify_test_network_isolation import (  # noqa: E402
    PRE_BOUNDARY_INJECTION_VARIABLES,
    RAW_REGISTRATION_OWNER,
    configured_failures,
    environment_injection_failures,
    registration_inventory,
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


class RegistrationInventoryTests(unittest.TestCase):
    def test_exact_owner_and_isolated_registration_are_visible(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(
                root,
                "tests/CMakeLists.txt",
                "bpr_add_network_isolated_test(NAME safe COMMAND x)\n",
            )
            self.assertEqual(registration_inventory(root), ([RAW_REGISTRATION_OWNER], 1))

    def test_rogue_raw_registration_is_visible(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            write(root, RAW_REGISTRATION_OWNER, "add_test(NAME x COMMAND y)\n")
            write(root, "tests/CMakeLists.txt", "ADD_TEST(NAME bypass COMMAND x)\n")
            raw, _ = registration_inventory(root)
            self.assertIn("tests/CMakeLists.txt", raw)


class ConfiguredManifestTests(unittest.TestCase):
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


class ConfiguredManifestAndProfileTests(unittest.TestCase):
    def test_derived_profile_contains_only_exact_runner_authority(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            template = root / "template.sb"
            output = root / "derived.sb"
            fixture = root / "fixture"
            fixture.mkdir()
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

    def test_profile_template_cannot_pregrant_network_authority(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
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
