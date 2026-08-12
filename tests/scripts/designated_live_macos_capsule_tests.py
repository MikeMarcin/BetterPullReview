#!/usr/bin/env python3
"""Hostile filesystem and runtime-contract tests for the macOS live capsule."""

from __future__ import annotations

import importlib.util
import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "scripts" / "designated_live_macos_capsule.py"
SPEC = importlib.util.spec_from_file_location("designated_live_macos_capsule", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("capsule verifier module could not be loaded")
capsule = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = capsule
SPEC.loader.exec_module(capsule)


SOURCE_DIGEST = "7" * 64


def write(path: pathlib.Path, contents: bytes, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(contents)
    path.chmod(mode)


def make_capsule(parent: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path]:
    root = parent / "capsule"
    root.mkdir(mode=0o700)
    write(root / capsule.APP_RELATIVE, b"application", 0o755)
    write(root / capsule.HELPER_RELATIVE, b"supervisor", 0o755)
    write(root / capsule.QT_CONF_RELATIVE, capsule.QT_CONF)
    write(root / capsule.VERIFIER_RELATIVE, b"verifier")
    write(
        root / "BetterPullReview.app/Contents/PlugIns/platforms/libqcocoa.dylib",
        b"platform plugin",
        0o755,
    )
    framework = root / "BetterPullReview.app/Contents/Frameworks/QtCore.framework"
    write(framework / "Versions/A/QtCore", b"framework", 0o755)
    (framework / "Versions/Current").symlink_to("A")
    inventory = parent / "capsule.inventory"
    inventory.write_text(
        capsule.render_inventory(root, SOURCE_DIGEST), encoding="utf-8", newline="\n"
    )
    return root, inventory


class CapsuleInventoryTests(unittest.TestCase):
    def test_exact_capsule_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            self.assertEqual(
                capsule.verify_inventory(
                    root,
                    inventory,
                    expected_source_sha256=SOURCE_DIGEST,
                    require_owner_private=True,
                ),
                SOURCE_DIGEST,
            )

    def test_substituted_file_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            (root / capsule.APP_RELATIVE).write_bytes(b"substitute")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "differs from filesystem"):
                capsule.verify_inventory(root, inventory)

    def test_missing_and_extra_entries_are_rejected(self) -> None:
        for mutation in ("missing", "extra"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root, inventory = make_capsule(pathlib.Path(directory))
                if mutation == "missing":
                    (root / capsule.QT_CONF_RELATIVE).unlink()
                else:
                    write(root / "BetterPullReview.app/Contents/Resources/extra", b"extra")
                with self.assertRaisesRegex(capsule.CapsuleFailure, "differs from filesystem"):
                    capsule.verify_inventory(root, inventory)

    def test_symlink_substitution_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            link = root / "BetterPullReview.app/Contents/Frameworks/QtCore.framework/Versions/Current"
            link.unlink()
            link.symlink_to("../Versions/A")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "differs from filesystem"):
                capsule.verify_inventory(root, inventory)

    def test_escaping_symlink_is_rejected_before_inventory_comparison(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            link = root / "BetterPullReview.app/Contents/Resources/escape"
            link.symlink_to("../../../../../../etc/passwd")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "symlink escapes"):
                capsule.verify_inventory(root, inventory)

    def test_writable_directory_and_non_private_root_are_rejected(self) -> None:
        for mutation in ("root", "directory"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root, inventory = make_capsule(pathlib.Path(directory))
                if mutation == "root":
                    root.chmod(0o755)
                    expected = "root is not owner-private"
                else:
                    target = root / "BetterPullReview.app/Contents/Resources"
                    target.chmod(stat.S_IMODE(target.stat().st_mode) | 0o020)
                    inventory.write_text(
                        capsule.render_inventory(root, SOURCE_DIGEST),
                        encoding="utf-8",
                        newline="\n",
                    )
                    expected = "group/world-writable entry"
                with self.assertRaisesRegex(capsule.CapsuleFailure, expected):
                    capsule.verify_inventory(root, inventory, require_owner_private=True)

    def test_qt_lookup_policy_is_exact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            (root / capsule.QT_CONF_RELATIVE).write_bytes(b"[Paths]\nPlugins=/tmp\n")
            inventory.write_text(
                capsule.render_inventory(root, SOURCE_DIGEST), encoding="utf-8", newline="\n"
            )
            with self.assertRaisesRegex(capsule.CapsuleFailure, "Qt lookup policy"):
                capsule.verify_inventory(root, inventory)

    def test_inventory_must_be_canonical_and_sorted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            lines = inventory.read_text(encoding="utf-8").splitlines()
            lines[-1], lines[-2] = lines[-2], lines[-1]
            inventory.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "canonical and sorted"):
                capsule.verify_inventory(root, inventory)


class CapsuleRuntimeTests(unittest.TestCase):
    def test_homebrew_dependency_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, _ = make_capsule(pathlib.Path(directory))
            application = root / capsule.APP_RELATIVE
            application.write_bytes(b"\xcf\xfa\xed\xfe" + b"x" * 8)

            def result(arguments: list[str]) -> subprocess.CompletedProcess[str]:
                if "-D" in arguments:
                    output = f"{application}:\n"
                elif "-L" in arguments:
                    output = (
                    f"{application}:\n"
                    "\t/opt/homebrew/opt/qt/lib/QtCore.framework/QtCore "
                    "(compatibility version 6.0.0, current version 6.0.0)\n"
                    )
                else:
                    output = ""
                return subprocess.CompletedProcess(arguments, 0, output, "")

            with mock.patch.object(capsule, "command", side_effect=result):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "external runtime dependency"):
                    capsule.verify_dependencies(root, pathlib.Path("/usr/bin/otool"))

    def test_unresolved_rpath_dependency_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, _ = make_capsule(pathlib.Path(directory))
            application = root / capsule.APP_RELATIVE
            application.write_bytes(b"\xcf\xfa\xed\xfe" + b"x" * 8)

            def result(arguments: list[str]) -> subprocess.CompletedProcess[str]:
                if "-D" in arguments:
                    output = f"{application}:\n"
                elif "-L" in arguments:
                    output = (
                    f"{application}:\n"
                    "\t@rpath/Absent.framework/Absent "
                    "(compatibility version 1.0.0, current version 1.0.0)\n"
                    )
                else:
                    output = ""
                return subprocess.CompletedProcess(arguments, 0, output, "")

            with mock.patch.object(capsule, "command", side_effect=result):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "cannot be resolved"):
                    capsule.verify_dependencies(root, pathlib.Path("/usr/bin/otool"))

    def test_signature_failure_is_rejected(self) -> None:
        failure = subprocess.CompletedProcess(["codesign"], 1, "", "invalid")
        with mock.patch.object(capsule, "command", return_value=failure):
            with self.assertRaisesRegex(capsule.CapsuleFailure, "signature is invalid"):
                capsule.verify_signature(pathlib.Path("bundle"), pathlib.Path("/usr/bin/codesign"))


class CapsuleClosureTests(unittest.TestCase):
    def test_macdeployqt_policy_overwrite_is_atomically_replaced(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            policy = root / capsule.QT_CONF_RELATIVE
            write(policy, b"[Paths]\nPrefix = /opt/homebrew\n")
            capsule.write_exact_qt_conf(root)
            self.assertEqual(policy.read_bytes(), capsule.QT_CONF)
            self.assertEqual(stat.S_IMODE(policy.stat().st_mode), 0o644)
            self.assertFalse((policy.parent / ".qt.conf.bpr-new").exists())

    def test_exact_loader_path_rpath_resolves_authentic_brotli_edge(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, _ = make_capsule(pathlib.Path(directory))
            application = root / capsule.APP_RELATIVE
            frameworks = root / "BetterPullReview.app/Contents/Frameworks"
            decoder = frameworks / "libbrotlidec.1.dylib"
            common = frameworks / "libbrotlicommon.1.dylib"
            for path in (application, decoder, common):
                path.write_bytes(b"\xcf\xfa\xed\xfe" + b"x" * 8)

            def result(arguments: list[str]) -> subprocess.CompletedProcess[str]:
                path = pathlib.Path(arguments[-1])
                if "-D" in arguments:
                    install_id = (
                        f"@rpath/{path.name}\n" if path in {decoder, common} else ""
                    )
                    output = f"{path}:\n{install_id}"
                elif "-L" in arguments:
                    dependencies = ""
                    if path == decoder:
                        dependencies = (
                            "\t@rpath/libbrotlidec.1.dylib "
                            "(compatibility version 1.0.0, current version 1.2.0)\n"
                            "\t@rpath/libbrotlicommon.1.dylib "
                            "(compatibility version 1.0.0, current version 1.2.0)\n"
                        )
                    elif path == common:
                        dependencies = (
                            "\t@rpath/libbrotlicommon.1.dylib "
                            "(compatibility version 1.0.0, current version 1.2.0)\n"
                        )
                    output = f"{path}:\n{dependencies}"
                else:
                    output = (
                        "Load command 1\n"
                        "          cmd LC_RPATH\n"
                        "      cmdsize 32\n"
                        "         path @loader_path (offset 12)\n"
                    )
                return subprocess.CompletedProcess(arguments, 0, output, "")

            with mock.patch.object(capsule, "command", side_effect=result):
                capsule.verify_dependencies(root, pathlib.Path("/usr/bin/otool"))

    def test_digest_prefixed_copy_is_collision_safe(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            frameworks = root / "Frameworks"
            frameworks.mkdir()
            first = root / "first/libsame.dylib"
            second = root / "second/libsame.dylib"
            write(first, b"first")
            write(second, b"second")
            copied: dict[pathlib.Path, pathlib.Path] = {}
            first_destination = capsule.copy_external_dependency(first, frameworks, copied)
            second_destination = capsule.copy_external_dependency(second, frameworks, copied)
            self.assertNotEqual(first_destination, second_destination)
            self.assertEqual(first_destination.read_bytes(), b"first")
            self.assertEqual(second_destination.read_bytes(), b"second")

    def test_ambiguous_capsule_dependency_is_visible(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            frameworks = root / "BetterPullReview.app/Contents/Frameworks"
            write(frameworks / "one/libsame.dylib", b"\xcf\xfa\xed\xfe")
            write(frameworks / "two/libsame.dylib", b"\xcf\xfa\xed\xfe")
            candidates = capsule.capsule_candidates(root, "@rpath/libsame.dylib")
            self.assertEqual(len(candidates), 2)

    def test_plugin_inventory_is_reduced_to_exact_runtime_set(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            plugins = root / "BetterPullReview.app/Contents/PlugIns"
            for relative in capsule.ALLOWED_PLUGIN_FILES:
                write(plugins / relative, b"allowed")
            unwanted = plugins / "tls/libqopensslbackend.dylib"
            write(unwanted, b"unwanted")
            capsule.prune_plugins(root)
            self.assertFalse(unwanted.exists())
            self.assertTrue(
                all((plugins / relative).is_file() for relative in capsule.ALLOWED_PLUGIN_FILES)
            )


if __name__ == "__main__":
    unittest.main()
