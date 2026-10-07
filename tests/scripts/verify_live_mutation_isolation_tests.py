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
import subprocess
import unittest
from unittest import mock
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

from scripts.verify_live_mutation_isolation import (  # noqa: E402
    FORBIDDEN_SYMBOL,
    POSITIVE_CONTROL_SYMBOL,
    REQUIRED_PORT_IMPLEMENTATION,
    _check_binaries,
    _check_sources,
    COMPILED_RESOURCE_HEADER,
    construction_site_counts,
    construction_sites,
    forbidden_symbols_in,
    is_compiled_resource,
    object_manifest_entries,
    port_implementations,
    symbol_command,
    symbols,
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


class NativeSymbolTests(unittest.TestCase):
    @mock.patch("scripts.verify_live_mutation_isolation.subprocess.run")
    @mock.patch("scripts.verify_live_mutation_isolation.symbol_command")
    def test_decorated_native_table_keeps_forbidden_and_control_identifiers(
        self, command: mock.Mock, run: mock.Mock
    ) -> None:
        command.return_value = ["nm", "test-binary"]
        table = (
            "0000000100003a10 T _ZN3bpr11application26review_mutation_coordinator7publishEv\n"
            "0000000100003b20 T _ZN3bpr8adapters6github29live_review_mutation_provider6submitEv\n"
        )
        run.return_value = subprocess.CompletedProcess(command.return_value, 0, table, "")
        observed = symbols("test-binary")
        self.assertIsNotNone(observed)
        self.assertIn(POSITIVE_CONTROL_SYMBOL, observed)
        self.assertEqual(len(forbidden_symbols_in(observed)), 1)
        run.assert_called_once_with(
            command.return_value, capture_output=True, text=True, check=False
        )

    @mock.patch("scripts.verify_live_mutation_isolation.subprocess.run")
    @mock.patch("scripts.verify_live_mutation_isolation.symbol_command")
    def test_an_unreadable_native_table_still_refuses(
        self, command: mock.Mock, run: mock.Mock
    ) -> None:
        command.return_value = ["nm", "test-binary"]
        run.return_value = subprocess.CompletedProcess(command.return_value, 1, "", "unreadable")
        self.assertIsNone(symbols("test-binary"))


class PortInventoryTests(unittest.TestCase):
    def test_finds_the_fake_and_nothing_else(self) -> None:
        with contained_temporary_directory() as directory:
            root = Path(directory)
            tree_with_only_the_fake(root)
            self.assertEqual(
                port_implementations(root),
                {REQUIRED_PORT_IMPLEMENTATION},
            )

    def test_finds_a_rogue_implementation(self) -> None:
        with contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
            root = Path(directory)
            write(root, "src/core/src/unrelated.cpp", "int main() { return 0; }\n")
            self.assertNotIn(
                REQUIRED_PORT_IMPLEMENTATION,
                port_implementations(root),
            )

    def test_a_mere_mention_is_not_an_implementation(self) -> None:
        with contained_temporary_directory() as directory:
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


class ObjectInspectionTests(unittest.TestCase):
    """The Windows half reads objects, because a linked image has no table.

    `dumpbin /symbols` against an MSVC-linked executable prints a section
    summary and no symbols at all. A guard that ran it would report a clean
    tree while reading nothing, which is the exact failure the positive control
    exists to catch, so the objects that composed each binary are read instead.
    """

    def test_manifest_maps_each_binary_to_its_objects(self) -> None:
        with contained_temporary_directory() as directory:
            manifest = Path(directory) / "objects.txt"
            manifest.write_text(
                "C:/b/one.exe|C:/b/a.obj|C:/b/b.obj\n"
                "C:/b/two.exe|C:/b/c.obj\n",
                encoding="utf-8",
            )
            self.assertEqual(
                object_manifest_entries(manifest),
                {
                    "C:/b/one.exe": ["C:/b/a.obj", "C:/b/b.obj"],
                    "C:/b/two.exe": ["C:/b/c.obj"],
                },
            )

    def test_only_a_real_compiled_resource_is_passed_over(self) -> None:
        # BPR-0974. A helper's version resource links as a .res, which dumpbin
        # cannot read and which holds no code. Recognised by content: a COFF
        # object or a /bigobj object renamed .res is still read.
        with contained_temporary_directory() as directory:
            root = Path(directory)
            resource = root / "version.rc.res"
            resource.write_bytes(COMPILED_RESOURCE_HEADER + bytes(16) + b"VS_VERSION_INFO")
            renamed_object = root / "renamed.res"
            renamed_object.write_bytes(b"\x64\x86" + bytes(30))
            renamed_bigobj = root / "bigobj.res"
            renamed_bigobj.write_bytes(b"\x00\x00\xff\xff" + bytes(28))
            named_object = root / "version.obj"
            named_object.write_bytes(resource.read_bytes())
            self.assertTrue(is_compiled_resource(str(resource)))
            for path in (renamed_object, renamed_bigobj, named_object, root / "absent.res"):
                self.assertFalse(is_compiled_resource(str(path)), path.name)

    def test_a_binary_with_no_objects_recorded_is_not_silently_clean(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            manifest = Path(directory) / "objects.txt"
            manifest.write_text("C:/b/other.exe|C:/b/a.obj\n", encoding="utf-8")
            self.assertEqual(object_manifest_entries(manifest).get("C:/b/one.exe"), None)

    @mock.patch("scripts.verify_live_mutation_isolation.inspects_objects")
    def test_missing_object_manifest_fails_closed(
        self, inspects: mock.Mock
    ) -> None:
        inspects.return_value = True
        with contained_temporary_directory() as directory:
            binaries = Path(directory) / "binaries.txt"
            binaries.write_text("C:/b/one.exe\n", encoding="utf-8")
            failures, inspected, complete = _check_binaries(binaries, None)
        self.assertEqual(inspected, 0)
        self.assertFalse(complete)
        self.assertTrue(
            any("objects must be inspected instead" in item for item in failures),
            failures,
        )

    @mock.patch("scripts.verify_live_mutation_isolation.inspects_objects")
    def test_a_binary_absent_from_the_object_manifest_is_a_failure(
        self, inspects: mock.Mock
    ) -> None:
        inspects.return_value = True
        with contained_temporary_directory() as directory:
            root = Path(directory)
            binaries = root / "binaries.txt"
            binaries.write_text("C:/b/one.exe\n", encoding="utf-8")
            objects = root / "objects.txt"
            objects.write_text("C:/b/other.exe|C:/b/a.obj\n", encoding="utf-8")
            failures, inspected, _ = _check_binaries(binaries, objects)
        self.assertEqual(inspected, 0)
        self.assertTrue(
            any("records no objects for" in item for item in failures), failures
        )

    @mock.patch("scripts.verify_live_mutation_isolation.object_symbols")
    @mock.patch("scripts.verify_live_mutation_isolation.inspects_objects")
    def test_an_unreadable_object_set_is_not_silently_clean(
        self, inspects: mock.Mock, read: mock.Mock
    ) -> None:
        inspects.return_value = True
        read.return_value = None
        with contained_temporary_directory() as directory:
            root = Path(directory)
            binaries = root / "binaries.txt"
            binaries.write_text("C:/b/review_mutation_tests.exe\n", encoding="utf-8")
            objects = root / "objects.txt"
            objects.write_text(
                "C:/b/review_mutation_tests.exe|C:/b/a.obj\n", encoding="utf-8"
            )
            failures, inspected, _ = _check_binaries(binaries, objects)
        self.assertEqual(inspected, 0)
        self.assertTrue(
            any("no inspector could read" in item for item in failures), failures
        )

    @mock.patch("scripts.verify_live_mutation_isolation.object_symbols")
    @mock.patch("scripts.verify_live_mutation_isolation.inspects_objects")
    def test_dumpbin_header_naming_the_provider_is_not_a_symbol(
        self, inspects: mock.Mock, read: mock.Mock
    ) -> None:
        # dumpbin prefaces each object's table with `Dump of file <path>`, and
        # the provider's own test object is named for it. The first Windows Qt
        # graph run flagged that path as a symbol (BPR-0431).
        inspects.return_value = True
        read.return_value = (
            f"Dump of file C:\\b\\{FORBIDDEN_SYMBOL}_tests.obj\n\n"
            "File Type: COFF OBJECT\n\n"
            "COFF SYMBOL TABLE\n"
            "A26 00000000 SECT1 notype () External | "
            f"??0{POSITIVE_CONTROL_SYMBOL}@application@bpr@@QEAA@XZ\n"
        )
        with contained_temporary_directory() as directory:
            root = Path(directory)
            binaries = root / "binaries.txt"
            binaries.write_text("C:/b/review_mutation_tests.exe\n", encoding="utf-8")
            objects = root / "objects.txt"
            objects.write_text(
                f"C:/b/review_mutation_tests.exe|C:/b/{FORBIDDEN_SYMBOL}_tests.obj\n",
                encoding="utf-8",
            )
            failures, inspected, _ = _check_binaries(binaries, objects)
        self.assertEqual(inspected, 1)
        self.assertEqual(failures, [])

    @mock.patch("scripts.verify_live_mutation_isolation.object_symbols")
    @mock.patch("scripts.verify_live_mutation_isolation.inspects_objects")
    def test_a_forbidden_symbol_in_an_object_is_caught(
        self, inspects: mock.Mock, read: mock.Mock
    ) -> None:
        inspects.return_value = True
        # An MSVC decorated name, which is what dumpbin actually prints. The
        # scan matches on the undecorated identifier inside it, so no demangler
        # is required — and none ships with Visual Studio.
        read.return_value = (
            "A26 00000000 UNDEF notype () External | "
            f"??0{POSITIVE_CONTROL_SYMBOL}@application@bpr@@QEAA@XZ\n"
            "A27 00000000 SECT1 notype () External | "
            f"?make@{FORBIDDEN_SYMBOL}@application@bpr@@QEAA@XZ\n"
        )
        with contained_temporary_directory() as directory:
            root = Path(directory)
            binaries = root / "binaries.txt"
            binaries.write_text("C:/b/review_mutation_tests.exe\n", encoding="utf-8")
            objects = root / "objects.txt"
            objects.write_text(
                "C:/b/review_mutation_tests.exe|C:/b/a.obj\n", encoding="utf-8"
            )
            failures, inspected, _ = _check_binaries(binaries, objects)
        self.assertEqual(inspected, 1)
        self.assertTrue(
            any(FORBIDDEN_SYMBOL in item for item in failures), failures
        )


class ConstructionSiteTests(unittest.TestCase):
    def test_fallback_only_live_path_is_rejected(self) -> None:
        # Choosing the compatibility representation cannot bypass inventory
        # or construction checks: both representations use the same port.
        with contained_temporary_directory() as directory:
            root = Path(directory)
            tree_with_only_the_fake(root)
            rogue = "tests/application/rogue_fallback.cpp"
            write(
                root, rogue,
                "class fallback_provider : public application::review_mutation_port {};\n"
                "auto provider = make_live_review_mutation_provider(effects);\n"
                "request.Representation = core::review_publication_representation::"
                "file_comment_with_permalink;\n"
                "provider->submit_and_reconcile(request);\n",
            )
            failures = _check_sources(root)
        self.assertTrue(any(rogue in item and "implements" in item for item in failures))
        self.assertTrue(any(rogue in item and "calls" in item for item in failures))

    @mock.patch("scripts.verify_live_mutation_isolation.symbols")
    @mock.patch("scripts.verify_live_mutation_isolation.inspects_objects")
    def test_fallback_submission_symbol_is_rejected(
        self, inspects: mock.Mock, read: mock.Mock
    ) -> None:
        inspects.return_value = False
        read.return_value = CLEAN_TABLE + (
            "\n0000000100004c30 T bpr::adapters::github::"
            "live_review_mutation_provider::submit_and_reconcile("
            "bpr::application::review_mutation_request const&)\n"
        )
        with contained_temporary_directory() as directory:
            manifest = Path(directory) / "binaries.txt"
            manifest.write_text("/fixture/review_mutation_tests\n", encoding="utf-8")
            failures, inspected, _ = _check_binaries(manifest)
        self.assertEqual(inspected, 1)
        self.assertTrue(any("submit_and_reconcile" in item for item in failures))

    def test_no_sites_on_a_clean_tree(self) -> None:
        with contained_temporary_directory() as directory:
            root = Path(directory)
            tree_with_only_the_fake(root)
            self.assertEqual(construction_sites(root), set())

    def test_a_test_calling_the_factory_is_caught(self) -> None:
        with contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
            root = Path(directory)
            write(
                root,
                "src/adapters/github/src/live_review_mutation_provider.hpp",
                "// declared here: make_live_review_mutation_provider\n",
            )
            self.assertEqual(construction_sites(root), set())


if __name__ == "__main__":
    unittest.main()
