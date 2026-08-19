#!/usr/bin/env python3
"""Hostile filesystem and runtime-contract tests for the macOS live capsule."""

from __future__ import annotations

import argparse
from pathlib import Path
import importlib.util
import os
import pathlib
import stat
import subprocess
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

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
        with contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            (root / capsule.APP_RELATIVE).write_bytes(b"substitute")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "differs from filesystem"):
                capsule.verify_inventory(root, inventory)

    def test_missing_and_extra_entries_are_rejected(self) -> None:
        for mutation in ("missing", "extra"):
            with self.subTest(mutation=mutation), contained_temporary_directory() as directory:
                root, inventory = make_capsule(pathlib.Path(directory))
                if mutation == "missing":
                    (root / capsule.QT_CONF_RELATIVE).unlink()
                else:
                    write(root / "BetterPullReview.app/Contents/Resources/extra", b"extra")
                with self.assertRaisesRegex(capsule.CapsuleFailure, "differs from filesystem"):
                    capsule.verify_inventory(root, inventory)

    def test_symlink_substitution_is_rejected(self) -> None:
        with contained_temporary_directory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            link = root / "BetterPullReview.app/Contents/Frameworks/QtCore.framework/Versions/Current"
            link.unlink()
            link.symlink_to("../Versions/A")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "differs from filesystem"):
                capsule.verify_inventory(root, inventory)

    def test_escaping_symlink_is_rejected_before_inventory_comparison(self) -> None:
        with contained_temporary_directory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            link = root / "BetterPullReview.app/Contents/Resources/escape"
            link.symlink_to("../../../../../../etc/passwd")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "symlink escapes"):
                capsule.verify_inventory(root, inventory)

    def test_writable_directory_and_non_private_root_are_rejected(self) -> None:
        for mutation in ("root", "directory"):
            with self.subTest(mutation=mutation), contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            (root / capsule.QT_CONF_RELATIVE).write_bytes(b"[Paths]\nPlugins=/tmp\n")
            inventory.write_text(
                capsule.render_inventory(root, SOURCE_DIGEST), encoding="utf-8", newline="\n"
            )
            with self.assertRaisesRegex(capsule.CapsuleFailure, "Qt lookup policy"):
                capsule.verify_inventory(root, inventory)

    def test_inventory_must_be_canonical_and_sorted(self) -> None:
        with contained_temporary_directory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            lines = inventory.read_text(encoding="utf-8").splitlines()
            lines[-1], lines[-2] = lines[-2], lines[-1]
            inventory.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "canonical and sorted"):
                capsule.verify_inventory(root, inventory)


class CapsuleRuntimeTests(unittest.TestCase):
    def test_homebrew_dependency_is_rejected(self) -> None:
        with contained_temporary_directory() as directory:
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
                    output = f"{application}:\n"
                return subprocess.CompletedProcess(arguments, 0, output, "")

            with mock.patch.object(capsule, "command", side_effect=result):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "external runtime dependency"):
                    capsule.verify_dependencies(root, pathlib.Path("/usr/bin/otool"))

    def test_unresolved_rpath_dependency_is_rejected(self) -> None:
        with contained_temporary_directory() as directory:
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
                    output = f"{application}:\n"
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
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            policy = root / capsule.QT_CONF_RELATIVE
            write(policy, b"[Paths]\nPrefix = /opt/homebrew\n")
            capsule.write_exact_qt_conf(root)
            self.assertEqual(policy.read_bytes(), capsule.QT_CONF)
            self.assertEqual(stat.S_IMODE(policy.stat().st_mode), 0o644)
            self.assertFalse((policy.parent / ".qt.conf.bpr-new").exists())

    def test_exact_loader_path_rpath_resolves_authentic_brotli_edge(self) -> None:
        with contained_temporary_directory() as directory:
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
                        f"{path}:\n"
                        "Load command 1\n"
                        "          cmd LC_RPATH\n"
                        "      cmdsize 32\n"
                        "         path @loader_path (offset 12)\n"
                    )
                return subprocess.CompletedProcess(arguments, 0, output, "")

            with mock.patch.object(capsule, "command", side_effect=result):
                capsule.verify_dependencies(root, pathlib.Path("/usr/bin/otool"))

    def test_digest_prefixed_copy_is_collision_safe(self) -> None:
        with contained_temporary_directory() as directory:
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
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            frameworks = root / "BetterPullReview.app/Contents/Frameworks"
            write(frameworks / "one/libsame.dylib", b"\xcf\xfa\xed\xfe")
            write(frameworks / "two/libsame.dylib", b"\xcf\xfa\xed\xfe")
            candidates = capsule.capsule_candidates(root, "@rpath/libsame.dylib")
            self.assertEqual(len(candidates), 2)

    def test_plugin_inventory_is_reduced_to_exact_runtime_set(self) -> None:
        with contained_temporary_directory() as directory:
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


class CapsuleImageTests(unittest.TestCase):
    """BPR-0353. The capsule now ships as a digest-bound read-only image.

    Half of these mock `command`, because what they assert is which flags the
    stage-zero launcher passes. The last one does not mock anything: it makes a
    real image, attaches it, tries to write to it, and detaches. A mocked
    read-only mount would prove only that the string "-readonly" was typed.
    """

    def image_arguments(self, **overrides: object) -> argparse.Namespace:
        defaults = {
            "hdiutil": pathlib.Path("/usr/bin/hdiutil"),
            "source_sha256": SOURCE_DIGEST,
        }
        defaults.update(overrides)
        return argparse.Namespace(**defaults)

    def test_attach_asks_for_a_read_only_unbrowsable_mount(self) -> None:
        recorded: list[list[str]] = []

        def record(arguments: list[str]) -> subprocess.CompletedProcess[str]:
            recorded.append(arguments)
            return subprocess.CompletedProcess(arguments, 0, "", "")

        with mock.patch.object(capsule, "command", record):
            capsule.attach_read_only(
                pathlib.Path("/image.dmg"),
                pathlib.Path("/mnt"),
                pathlib.Path("/usr/bin/hdiutil"),
            )
        self.assertEqual(len(recorded), 1)
        self.assertIn("-readonly", recorded[0])
        self.assertIn("-nobrowse", recorded[0])
        self.assertIn("-mountpoint", recorded[0])

    def test_detach_never_asks_for_force_and_refuses_when_it_would_need_it(
        self,
    ) -> None:
        recorded: list[list[str]] = []

        def refuse(arguments: list[str]) -> subprocess.CompletedProcess[str]:
            recorded.append(arguments)
            return subprocess.CompletedProcess(arguments, 16, "", "busy")

        with mock.patch.object(capsule, "command", refuse):
            with self.assertRaises(capsule.CapsuleFailure):
                capsule.detach_without_force(
                    pathlib.Path("/mnt"), pathlib.Path("/usr/bin/hdiutil")
                )
        self.assertEqual(len(recorded), 1)
        self.assertNotIn("-force", recorded[0])

    def test_a_writable_mount_is_refused_and_leaves_nothing_behind(self) -> None:
        with contained_temporary_directory() as directory:
            writable = pathlib.Path(directory)
            with self.assertRaises(capsule.CapsuleFailure):
                capsule.require_read_only_mount(writable)
            self.assertEqual(list(writable.iterdir()), [])

    def test_mount_root_is_fresh_and_owner_private(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory) / "mounts"
            first = capsule.owner_private_mount_root(parent)
            second = capsule.owner_private_mount_root(parent)
            self.assertNotEqual(first, second)
            for root in (first, second):
                self.assertTrue(root.is_dir())
                self.assertEqual(root.stat().st_mode & 0o777, 0o700)

    def test_a_drifted_image_digest_refuses_before_anything_is_attached(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"not the approved image")
            attempted: list[list[str]] = []

            def record(arguments: list[str]) -> subprocess.CompletedProcess[str]:
                attempted.append(arguments)
                return subprocess.CompletedProcess(arguments, 0, "", "")

            with mock.patch.object(capsule, "command", record):
                with self.assertRaises(capsule.CapsuleFailure):
                    capsule.stage_zero(
                        self.image_arguments(
                            image=image,
                            expected_image_sha256="0" * 64,
                            mount_parent=parent / "mounts",
                        )
                    )
            self.assertEqual(attempted, [])

    @unittest.skipUnless(
        pathlib.Path("/usr/bin/hdiutil").exists(), "hdiutil is macOS-only"
    )
    def test_real_image_round_trip_is_read_only_and_detaches_without_force(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            root, inventory = make_capsule(parent)
            image = parent / "capsule.dmg"
            capsule.build_image(
                self.image_arguments(
                    capsule=root,
                    inventory=inventory,
                    output=image,
                    image_digest=None,
                )
            )
            self.assertTrue(image.is_file())
            digest = capsule.image_sha256(image)

            observed: dict[str, object] = {}
            original = capsule.require_read_only_mount

            def observe(mount_root: pathlib.Path) -> None:
                # The mount is live exactly here, so this is the only place the
                # read-only claim can be tested against the real filesystem.
                observed["capsule"] = (
                    mount_root / capsule.IMAGE_CAPSULE_RELATIVE / capsule.APP_RELATIVE
                ).is_file()
                observed["inventory"] = (
                    mount_root / capsule.IMAGE_INVENTORY_RELATIVE
                ).is_file()
                probe = mount_root / "write-probe"
                try:
                    probe.write_bytes(b"x")
                    observed["writable"] = True
                except OSError:
                    observed["writable"] = False
                original(mount_root)

            with mock.patch.object(capsule, "require_read_only_mount", observe):
                capsule.stage_zero(
                    self.image_arguments(
                        image=image,
                        expected_image_sha256=digest,
                        mount_parent=parent / "mounts",
                    )
                )
            self.assertTrue(observed["capsule"], "the app is inside the image")
            self.assertTrue(observed["inventory"], "the inventory is inside the image")
            self.assertFalse(observed["writable"], "the attached image refuses writes")
            # stage_zero detaches on the way out, forcibly or not; nothing of
            # this run may still be attached.
            listing = subprocess.run(
                ["/usr/bin/hdiutil", "info"],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotIn(str(image), listing.stdout)


class MachFramingTests(unittest.TestCase):
    """The otool parsers accept thin and universal framing, nothing else.

    BPR-0367: the acquired Qt ships universal binaries, whose otool output
    repeats one stanza per architecture. The install-name parser used to fail
    closed on that framing, and the dependency parser used to merge slices
    and ingest the second architecture header as a dependency name.
    """

    PATH = pathlib.Path("/capsule/BetterPullReview.app/Contents/Frameworks/QtCore")

    def fat(self, first: str, second: str) -> str:
        return (
            f"{self.PATH} (architecture x86_64):\n{first}"
            f"{self.PATH} (architecture arm64):\n{second}"
        )

    def test_thin_install_id(self) -> None:
        stdout = f"{self.PATH}:\n@rpath/QtCore.framework/Versions/A/QtCore\n"
        self.assertEqual(
            capsule.parse_install_id(self.PATH, stdout),
            "@rpath/QtCore.framework/Versions/A/QtCore",
        )
        self.assertIsNone(capsule.parse_install_id(self.PATH, f"{self.PATH}:\n"))

    def test_fat_install_id_requires_agreement(self) -> None:
        agreeing = self.fat("@rpath/QtCore\n", "@rpath/QtCore\n")
        self.assertEqual(capsule.parse_install_id(self.PATH, agreeing), "@rpath/QtCore")
        disagreeing = self.fat("@rpath/QtCore\n", "@rpath/QtHostile\n")
        with self.assertRaises(capsule.CapsuleFailure):
            capsule.parse_install_id(self.PATH, disagreeing)

    def test_unrecognized_framing_fails_closed(self) -> None:
        for hostile in (
            "",
            "no header at all\n@rpath/QtCore\n",
            f"prefix\n{self.PATH}:\n@rpath/QtCore\n",
            f"{self.PATH}:\n@rpath/QtCore\nextra line\n",
        ):
            with self.assertRaises(capsule.CapsuleFailure):
                capsule.parse_install_id(self.PATH, hostile)

    def test_fat_dependencies_exclude_headers_and_require_agreement(self) -> None:
        body = (
            "\t@rpath/QtCore (compatibility version 6.0.0, current version 6.10.3)\n"
            "\t/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1800.0.0)\n"
        )
        agreeing = self.fat(body, body)
        self.assertEqual(
            capsule.parse_dependencies(self.PATH, agreeing, "@rpath/QtCore"),
            ["/usr/lib/libc++.1.dylib"],
        )
        hostile = self.fat(body, "\t@rpath/QtHostile (compatibility version 1.0.0, current version 1.0.0)\n")
        with self.assertRaises(capsule.CapsuleFailure):
            capsule.parse_dependencies(self.PATH, hostile, "@rpath/QtCore")

    def test_fat_rpaths_require_agreement(self) -> None:
        body = (
            "Load command 1\n"
            "          cmd LC_RPATH\n"
            "      cmdsize 40\n"
            "         path @executable_path/../Frameworks (offset 12)\n"
        )
        agreeing = self.fat(body, body)
        self.assertEqual(
            capsule.parse_rpaths(self.PATH, agreeing),
            ["@executable_path/../Frameworks"],
        )
        hostile = self.fat(
            body,
            body.replace("../Frameworks", "../Hostile"),
        )
        with self.assertRaises(capsule.CapsuleFailure):
            capsule.parse_rpaths(self.PATH, hostile)
        malformed = f"{self.PATH}:\n          cmd LC_RPATH\n      cmdsize 40\n         path missing-offset\n"
        with self.assertRaises(capsule.CapsuleFailure):
            capsule.parse_rpaths(self.PATH, malformed)


if __name__ == "__main__":
    unittest.main()
