#!/usr/bin/env python3
"""Drive each clause of the live-mutation isolation guard to failure.

A guard written before the thing it guards exists can only be trusted if it has
been seen to fail. On the real tree every clause passes, and a clause that
passes because it reads nothing is indistinguishable from one that passes
because the tree is clean. These tests synthesise the trees where the answer is
known.
"""

from __future__ import annotations

import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.verify_live_mutation_isolation import (  # noqa: E402
    FORBIDDEN_SYMBOL,
    POSITIVE_CONTROL_SYMBOL,
    REQUIRED_PORT_IMPLEMENTATION,
    construction_site_counts,
    construction_sites,
    forbidden_symbols_in,
    port_implementations,
    symbol_command,
)


# A demangled table of the shape `nm | c++filt` produces.
CLEAN_TABLE = "\n".join(
    (
        "0000000100003a10 T bpr::application::review_mutation_coordinator::publish()",
        "0000000100003b20 T bpr::application::review_mutation_coordinator::designated()",
        "                 U operator new(unsigned long)",
    )
)

LIVE_PROVIDER_TABLE = CLEAN_TABLE + "\n" + "\n".join(
    (
        "0000000100004c30 T bpr::adapters::github::live_review_mutation_provider::submit("
        "bpr::application::review_mutation_request const&)",
    )
)


def write(root: Path, relative: str, body: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")


def tree_with_only_the_fake(root: Path) -> None:
    """The tree as it stands today: one implementation, and it is the fake."""
    write(
        root,
        REQUIRED_PORT_IMPLEMENTATION,
        "namespace {\n"
        "\tclass fake_provider final : public application::review_mutation_port\n"
        "\t{\n"
        "\t};\n"
        "}\n",
    )
    write(
        root,
        "src/application/src/review_mutation_coordinator.cpp",
        "// uses application::review_mutation_port& but implements nothing\n",
    )


class PortInventoryTests(unittest.TestCase):
    def test_finds_the_fake_and_nothing_else(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tree_with_only_the_fake(root)
            self.assertEqual(
                port_implementations(root),
                {REQUIRED_PORT_IMPLEMENTATION},
            )

    def test_finds_a_rogue_implementation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tree_with_only_the_fake(root)
            write(
                root,
                "src/application/src/sneaky_provider.cpp",
                "class sneaky final : public application::review_mutation_port\n"
                "{\n};\n",
            )
            found = port_implementations(root)
            self.assertIn("src/application/src/sneaky_provider.cpp", found)

    def test_a_tree_missing_the_fake_is_visible(self) -> None:
        # The instrument reporting an empty set must be distinguishable from a
        # clean tree, which is what the required-implementation check is for.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, "src/core/src/unrelated.cpp", "int main() { return 0; }\n")
            self.assertNotIn(
                REQUIRED_PORT_IMPLEMENTATION,
                port_implementations(root),
            )

    def test_a_mere_mention_is_not_an_implementation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(
                root,
                "src/application/src/uses_the_port.cpp",
                "void publish(application::review_mutation_port& aProvider);\n",
            )
            self.assertEqual(port_implementations(root), set())


class ForbiddenSymbolTests(unittest.TestCase):
    def test_a_clean_table_has_none(self) -> None:
        self.assertEqual(forbidden_symbols_in(CLEAN_TABLE), [])

    def test_the_live_provider_is_caught(self) -> None:
        found = forbidden_symbols_in(LIVE_PROVIDER_TABLE)
        self.assertEqual(len(found), 1)
        self.assertIn(FORBIDDEN_SYMBOL, found[0])

    def test_the_positive_control_symbol_is_present_in_a_real_table(self) -> None:
        # If this were absent the guard would be reading an empty or stripped
        # table and its silence would mean nothing.
        self.assertIn(POSITIVE_CONTROL_SYMBOL, CLEAN_TABLE)

    @mock.patch("scripts.verify_live_mutation_isolation.platform.system")
    @mock.patch("scripts.verify_live_mutation_isolation.shutil.which")
    def test_windows_selects_a_supported_inspector(
        self, which: mock.Mock, system: mock.Mock
    ) -> None:
        system.return_value = "Windows"
        which.side_effect = lambda name: (
            "C:/LLVM/bin/llvm-readobj.exe" if name == "llvm-readobj" else None
        )
        self.assertEqual(
            symbol_command("C:/build/test.exe"),
            [
                "C:/LLVM/bin/llvm-readobj.exe",
                "--symbols",
                "C:/build/test.exe",
            ],
        )

    @mock.patch("scripts.verify_live_mutation_isolation.platform.system")
    @mock.patch("scripts.verify_live_mutation_isolation.shutil.which")
    def test_missing_windows_inspector_fails_closed(
        self, which: mock.Mock, system: mock.Mock
    ) -> None:
        system.return_value = "Windows"
        which.return_value = None
        self.assertIsNone(symbol_command("C:/build/test.exe"))


class ConstructionSiteTests(unittest.TestCase):
    def test_no_sites_on_a_clean_tree(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tree_with_only_the_fake(root)
            self.assertEqual(construction_sites(root), set())

    def test_a_test_calling_the_factory_is_caught(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(
                root,
                "tests/application/rogue_tests.cpp",
                "auto provider = make_live_review_mutation_provider(effects);\n",
            )
            self.assertEqual(
                construction_sites(root),
                {"tests/application/rogue_tests.cpp"},
            )

    def test_the_allowed_sites_are_still_reported(self) -> None:
        # The function reports every site; the allowlist is applied by the
        # caller. Reporting only violations would make the allowlist untestable.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(
                root,
                "src/app/src/github_application.cpp",
                "auto provider = make_live_review_mutation_provider(effects);\n",
            )
            self.assertEqual(
                construction_sites(root),
                {"src/app/src/github_application.cpp"},
            )

    def test_a_second_call_inside_the_composition_root_is_counted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(
                root,
                "src/app/src/github_application.cpp",
                "auto first = make_live_review_mutation_provider(one);\n"
                "auto second = make_live_review_mutation_provider(two);\n",
            )
            self.assertEqual(
                construction_site_counts(root),
                {"src/app/src/github_application.cpp": 2},
            )

    def test_naming_without_calling_is_not_a_site(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(
                root,
                "src/adapters/github/src/live_review_mutation_provider.hpp",
                "// declared here: make_live_review_mutation_provider\n",
            )
            self.assertEqual(construction_sites(root), set())


if __name__ == "__main__":
    unittest.main()
