#!/usr/bin/env python3
"""Hostile filesystem and runtime-contract tests for the macOS live capsule."""

from __future__ import annotations

import argparse
from pathlib import Path
import importlib.util
import os
import pathlib
import select
import signal
import socket
import stat
import subprocess
import sys
import time
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

    def test_anonymous_inventory_stream_preserves_exact_bytes(self) -> None:
        with contained_temporary_directory() as directory:
            root, inventory = make_capsule(pathlib.Path(directory))
            expected = inventory.read_text(encoding="utf-8")
            sender, receiver = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                sender.sendall(expected.encode("utf-8"))
                sender.shutdown(socket.SHUT_WR)
                observed = capsule.read_inventory_descriptor(receiver.fileno())
                self.assertEqual(observed, expected)
                self.assertEqual(
                    capsule.verify_inventory(
                        root,
                        None,
                        inventory_text=observed,
                        expected_source_sha256=SOURCE_DIGEST,
                    ),
                    SOURCE_DIGEST,
                )
            finally:
                sender.close()
                receiver.close()

    def test_inventory_descriptor_accepts_only_anonymous_stream(self) -> None:
        with contained_temporary_directory() as directory:
            _, inventory = make_capsule(pathlib.Path(directory))
            descriptor = os.open(inventory, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
            try:
                with self.assertRaisesRegex(capsule.CapsuleFailure, "not a socket"):
                    capsule.read_inventory_descriptor(descriptor)
            finally:
                os.close(descriptor)

    def test_inventory_descriptor_rejects_named_and_invalid_streams(self) -> None:
        with contained_temporary_directory() as directory:
            socket_path = pathlib.Path(directory) / "named.sock"
            listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            listener.bind(str(socket_path))
            listener.listen(1)
            client.connect(str(socket_path))
            accepted, _ = listener.accept()
            try:
                with self.assertRaisesRegex(capsule.CapsuleFailure, "is named"):
                    capsule.read_inventory_descriptor(accepted.fileno())
            finally:
                accepted.close()
                client.close()
                listener.close()

        for contents, expected in ((b"", "framing"), (b"123456789", "framing")):
            with self.subTest(contents=contents):
                sender, receiver = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
                try:
                    sender.sendall(contents)
                    sender.shutdown(socket.SHUT_WR)
                    with mock.patch.object(capsule, "MAXIMUM_INVENTORY_BYTES", 8):
                        with self.assertRaisesRegex(capsule.CapsuleFailure, expected):
                            capsule.read_inventory_descriptor(receiver.fileno())
                finally:
                    sender.close()
                    receiver.close()

    def test_descriptor_verification_requires_the_retained_cwd(self) -> None:
        arguments = argparse.Namespace(
            capsule=pathlib.Path("capsule"),
            inventory=None,
            inventory_fd=64,
        )
        with self.assertRaisesRegex(capsule.CapsuleFailure, "retained capsule cwd"):
            capsule.verify(arguments)

    def test_retained_lookup_rejects_absolute_and_escaping_paths(self) -> None:
        for path in (pathlib.Path("/tmp/outside"), pathlib.Path("../outside")):
            with self.subTest(path=path):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "retained capsule"):
                    capsule.retained_relative_path(path)


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
            "expected_image_sha256": None,
            "expected_image_sha256_file": None,
            "probe_launch": True,
            "attended_launch": False,
            "approval": None,
        }
        defaults.update(overrides)
        for name in (
            "image",
            "expected_image_sha256_file",
            "mount_parent",
            "approval",
        ):
            value = defaults.get(name)
            if isinstance(value, pathlib.Path):
                defaults[name] = value.resolve(strict=False)
        return argparse.Namespace(**defaults)

    def assert_process_gone(self, process_id: int) -> None:
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                os.kill(process_id, 0)
            except ProcessLookupError:
                return
            time.sleep(0.05)
        self.fail(f"process {process_id} remained alive")

    def blocking_helper(
        self,
        root: pathlib.Path,
        *,
        ignore_term: bool,
    ) -> tuple[pathlib.Path, pathlib.Path]:
        process_id = root / "process-id"
        helper = root / "helper.py"
        handler = (
            "signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
            if ignore_term
            else ""
        )
        helper.write_text(
            "#!/usr/bin/python3\n"
            "import os, pathlib, signal, time\n"
            + handler
            + f"pathlib.Path({str(process_id)!r}).write_text(str(os.getpid()))\n"
            + "while True:\n"
            + "    time.sleep(1)\n",
            encoding="utf-8",
        )
        helper.chmod(0o700)
        return helper, process_id

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

    def test_stage_one_timeout_terminates_cooperative_process_group(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            helper, process_id = self.blocking_helper(root, ignore_term=False)
            delivered: list[signal.Signals] = []
            original = capsule.signal_process_group

            def record(process_group: int, requested: signal.Signals) -> None:
                delivered.append(requested)
                original(process_group, requested)

            with (
                mock.patch.object(capsule, "STAGE_ONE_TIMEOUT_SECONDS", 1.0),
                mock.patch.object(capsule, "STAGE_ONE_TERM_GRACE_SECONDS", 1.0),
                mock.patch.object(capsule, "STAGE_ONE_KILL_GRACE_SECONDS", 1.0),
                mock.patch.object(capsule, "signal_process_group", side_effect=record),
            ):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "deadline"):
                    capsule.run_stage_one(helper, root / "inventory", None)
            self.assertEqual(delivered, [signal.SIGTERM])
            self.assert_process_gone(int(process_id.read_text(encoding="utf-8")))

    def test_stage_one_timeout_kills_term_ignoring_process_group(self) -> None:
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            helper, process_id = self.blocking_helper(root, ignore_term=True)
            delivered: list[signal.Signals] = []
            original = capsule.signal_process_group

            def record(process_group: int, requested: signal.Signals) -> None:
                delivered.append(requested)
                original(process_group, requested)

            with (
                mock.patch.object(capsule, "STAGE_ONE_TIMEOUT_SECONDS", 1.0),
                mock.patch.object(capsule, "STAGE_ONE_TERM_GRACE_SECONDS", 0.2),
                mock.patch.object(capsule, "STAGE_ONE_KILL_GRACE_SECONDS", 1.0),
                mock.patch.object(capsule, "signal_process_group", side_effect=record),
            ):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "deadline"):
                    capsule.run_stage_one(helper, root / "inventory", None)
            self.assertEqual(delivered, [signal.SIGTERM, signal.SIGKILL])
            self.assert_process_gone(int(process_id.read_text(encoding="utf-8")))

    def test_a_writable_mount_is_refused_and_leaves_nothing_behind(self) -> None:
        with contained_temporary_directory() as directory:
            writable = pathlib.Path(directory)
            with self.assertRaises(capsule.CapsuleFailure):
                capsule.require_read_only_mount(writable)
            self.assertEqual(list(writable.iterdir()), [])

    def test_mount_root_is_fresh_and_owner_private(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory) / "mounts"
            parent = parent.resolve(strict=False)
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

    def test_in_place_image_restore_cannot_hide_behind_restored_mtime(self) -> None:
        with contained_temporary_directory() as directory:
            image = pathlib.Path(directory) / "capsule.dmg"
            original = b"exact retained image bytes"
            image.write_bytes(original)
            image.chmod(0o600)
            retained = capsule.retain_image(image.resolve())
            try:
                retained = capsule.rebaseline_attached_image(retained)
                original_atime = image.stat().st_atime_ns
                time.sleep(0.01)
                with image.open("r+b") as stream:
                    stream.write(b"hostile transient bytes")
                    stream.seek(0)
                    stream.write(original)
                    stream.truncate()
                    stream.flush()
                    os.fsync(stream.fileno())
                os.utime(
                    image,
                    ns=(original_atime, retained.identity.modified_ns),
                )
                self.assertEqual(capsule.image_sha256(image), retained.digest)
                self.assertEqual(
                    image.stat().st_mtime_ns,
                    retained.identity.modified_ns,
                )
                self.assertNotEqual(
                    image.stat().st_ctime_ns,
                    retained.identity.changed_ns,
                )
                with self.assertRaisesRegex(
                    capsule.CapsuleFailure,
                    "retained image identity drifted",
                ):
                    capsule.revalidate_image(retained)
            finally:
                os.close(retained.descriptor)

    def test_stage_zero_orders_mounted_probe_revalidation_and_detach(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"exact image")
            image.chmod(0o600)
            events: list[str] = []
            mounted = capsule.RuntimeIdentity(
                mount=capsule.FilesystemIdentity(91, 92, stat.S_IFDIR | 0o555, os.geteuid(), os.getegid(), 0, 0, 0),
                capsule=capsule.FilesystemIdentity(91, 93, stat.S_IFDIR | 0o700, os.geteuid(), os.getegid(), 0, 0, 0),
                files=(),
            )

            def event(name: str, value: object = None) -> object:
                events.append(name)
                return value

            with (
                mock.patch.object(capsule, "attach_read_only", side_effect=lambda *args: event("attach")),
                mock.patch.object(capsule, "require_read_only_mount", side_effect=lambda *args: event("read-only")),
                mock.patch.object(capsule, "capture_runtime_identity", side_effect=lambda *args: event("capture-runtime", mounted)),
                mock.patch.object(capsule, "verify_inventory", side_effect=lambda *args, **kwargs: event("verify-static", SOURCE_DIGEST)),
                mock.patch.object(capsule, "approval_path", side_effect=lambda *args: event("approval", None)),
                mock.patch.object(capsule, "revalidate_image", side_effect=lambda *args: event("image")),
                mock.patch.object(capsule, "revalidate_runtime_identity", side_effect=lambda *args: event("runtime")),
                mock.patch.object(
                    capsule,
                    "run_stage_one",
                    side_effect=lambda *args: event(
                        "wait-child",
                        subprocess.CompletedProcess(["probe"], 0, b"", b""),
                    ),
                ),
                mock.patch.object(
                    capsule,
                    "close_runtime_identity",
                    side_effect=lambda *args: event("close-runtime"),
                ),
                mock.patch.object(capsule, "detach_without_force", side_effect=lambda *args: event("detach")),
                mock.patch.object(capsule, "require_underlying_mount_root", side_effect=lambda *args: event("underlying")),
            ):
                capsule.stage_zero(
                    self.image_arguments(
                        image=image,
                        expected_image_sha256=capsule.image_sha256(image),
                        mount_parent=parent / "mounts",
                    )
                )

            self.assertEqual(
                events,
                [
                    "attach",
                    "read-only",
                    "capture-runtime",
                    "verify-static",
                    "approval",
                    "image",
                    "runtime",
                    "wait-child",
                    "image",
                    "runtime",
                    "close-runtime",
                    "detach",
                    "underlying",
                ],
            )

    def test_sigterm_during_attach_unwinds_through_ordinary_detach(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"exact image")
            image.chmod(0o600)
            detached = mock.Mock()
            previous_handlers = tuple(
                signal.getsignal(signum) for signum in capsule.STAGE_ZERO_SIGNALS
            )

            def interrupt_attach(*_arguments: object) -> None:
                os.kill(os.getpid(), signal.SIGTERM)

            with (
                mock.patch.object(
                    capsule,
                    "attach_read_only",
                    side_effect=interrupt_attach,
                ),
                mock.patch.object(capsule, "detach_without_force", detached),
                mock.patch.object(capsule, "require_underlying_mount_root"),
            ):
                with self.assertRaises(capsule.StageZeroInterrupted):
                    capsule.stage_zero(
                        self.image_arguments(
                            image=image,
                            expected_image_sha256=capsule.image_sha256(image),
                            mount_parent=parent / "mounts",
                        )
                    )

            detached.assert_called_once()
            self.assertEqual(
                tuple(
                    signal.getsignal(signum)
                    for signum in capsule.STAGE_ZERO_SIGNALS
                ),
                previous_handlers,
            )

    def test_supervisor_failure_is_revalidated_before_cleanup(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"exact image")
            image.chmod(0o600)
            events: list[str] = []
            mounted = capsule.RuntimeIdentity(
                mount=capsule.FilesystemIdentity(81, 82, stat.S_IFDIR | 0o555, os.geteuid(), os.getegid(), 0, 0, 0),
                capsule=capsule.FilesystemIdentity(81, 83, stat.S_IFDIR | 0o700, os.geteuid(), os.getegid(), 0, 0, 0),
                files=(),
            )
            with (
                mock.patch.object(capsule, "attach_read_only"),
                mock.patch.object(capsule, "require_read_only_mount"),
                mock.patch.object(capsule, "capture_runtime_identity", return_value=mounted),
                mock.patch.object(capsule, "verify_inventory", return_value=SOURCE_DIGEST),
                mock.patch.object(capsule, "approval_path", return_value=None),
                mock.patch.object(capsule, "revalidate_image", side_effect=lambda *args: events.append("image")),
                mock.patch.object(capsule, "revalidate_runtime_identity", side_effect=lambda *args: events.append("runtime")),
                mock.patch.object(
                    capsule,
                    "run_stage_one",
                    side_effect=lambda *args: (
                        events.append("wait-child")
                        or subprocess.CompletedProcess(["probe"], 17, b"", b"")
                    ),
                ),
                mock.patch.object(
                    capsule,
                    "close_runtime_identity",
                    side_effect=lambda *args: events.append("close-runtime"),
                ),
                mock.patch.object(capsule, "detach_without_force", side_effect=lambda *args: events.append("detach")),
                mock.patch.object(capsule, "require_underlying_mount_root"),
            ):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "status 17"):
                    capsule.stage_zero(
                        self.image_arguments(
                            image=image,
                            expected_image_sha256=capsule.image_sha256(image),
                            mount_parent=parent / "mounts",
                        )
                    )
            self.assertEqual(
                events,
                [
                    "image",
                    "runtime",
                    "wait-child",
                    "image",
                    "runtime",
                    "close-runtime",
                    "detach",
                ],
            )

    def test_stage_one_exception_is_revalidated_before_cleanup(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"exact image")
            image.chmod(0o600)
            events: list[str] = []
            mounted = capsule.RuntimeIdentity(
                mount=capsule.FilesystemIdentity(51, 52, stat.S_IFDIR | 0o555, os.geteuid(), os.getegid(), 0, 0, 0),
                capsule=capsule.FilesystemIdentity(51, 53, stat.S_IFDIR | 0o700, os.geteuid(), os.getegid(), 0, 0, 0),
                files=(),
            )

            def interrupted(*_arguments: object) -> None:
                events.append("wait-child")
                raise capsule.CapsuleFailure("stage one interrupted")

            with (
                mock.patch.object(capsule, "attach_read_only"),
                mock.patch.object(capsule, "require_read_only_mount"),
                mock.patch.object(capsule, "capture_runtime_identity", return_value=mounted),
                mock.patch.object(capsule, "verify_inventory", return_value=SOURCE_DIGEST),
                mock.patch.object(capsule, "approval_path", return_value=None),
                mock.patch.object(capsule, "revalidate_image", side_effect=lambda *args: events.append("image")),
                mock.patch.object(capsule, "revalidate_runtime_identity", side_effect=lambda *args: events.append("runtime")),
                mock.patch.object(capsule, "run_stage_one", side_effect=interrupted),
                mock.patch.object(capsule, "close_runtime_identity", side_effect=lambda *args: events.append("close-runtime")),
                mock.patch.object(capsule, "detach_without_force", side_effect=lambda *args: events.append("detach")),
                mock.patch.object(capsule, "require_underlying_mount_root"),
            ):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "interrupted"):
                    capsule.stage_zero(
                        self.image_arguments(
                            image=image,
                            expected_image_sha256=capsule.image_sha256(image),
                            mount_parent=parent / "mounts",
                        )
                    )
            self.assertEqual(
                events,
                [
                    "image",
                    "runtime",
                    "wait-child",
                    "image",
                    "runtime",
                    "close-runtime",
                    "detach",
                ],
            )

    def test_prelaunch_image_or_runtime_drift_performs_zero_launches(self) -> None:
        for drift in ("image", "runtime"):
            with self.subTest(drift=drift), contained_temporary_directory() as directory:
                parent = pathlib.Path(directory)
                image = parent / "capsule.dmg"
                image.write_bytes(b"exact image")
                image.chmod(0o600)
                mounted = capsule.RuntimeIdentity(
                    mount=capsule.FilesystemIdentity(71, 72, stat.S_IFDIR | 0o555, os.geteuid(), os.getegid(), 0, 0, 0),
                    capsule=capsule.FilesystemIdentity(71, 73, stat.S_IFDIR | 0o700, os.geteuid(), os.getegid(), 0, 0, 0),
                    files=(),
                )
                launched = mock.Mock()
                image_check = mock.Mock(
                    side_effect=(capsule.CapsuleFailure("image drift") if drift == "image" else None)
                )
                runtime_check = mock.Mock(
                    side_effect=(capsule.CapsuleFailure("runtime drift") if drift == "runtime" else None)
                )
                detached = mock.Mock()
                with (
                    mock.patch.object(capsule, "attach_read_only"),
                    mock.patch.object(capsule, "require_read_only_mount"),
                    mock.patch.object(capsule, "capture_runtime_identity", return_value=mounted),
                    mock.patch.object(capsule, "verify_inventory", return_value=SOURCE_DIGEST),
                    mock.patch.object(capsule, "approval_path", return_value=None),
                    mock.patch.object(capsule, "revalidate_image", image_check),
                    mock.patch.object(capsule, "revalidate_runtime_identity", runtime_check),
                    mock.patch.object(capsule, "run_stage_one", launched),
                    mock.patch.object(capsule, "detach_without_force", detached),
                    mock.patch.object(capsule, "require_underlying_mount_root"),
                ):
                    with self.assertRaises(capsule.CapsuleFailure):
                        capsule.stage_zero(
                            self.image_arguments(
                                image=image,
                                expected_image_sha256=capsule.image_sha256(image),
                                mount_parent=parent / "mounts",
                            )
                        )
                launched.assert_not_called()
                detached.assert_called_once()

    def test_detach_during_child_is_detected_after_wait(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"exact image")
            image.chmod(0o600)
            mounted = capsule.RuntimeIdentity(
                mount=capsule.FilesystemIdentity(61, 62, stat.S_IFDIR | 0o555, os.geteuid(), os.getegid(), 0, 0, 0),
                capsule=capsule.FilesystemIdentity(61, 63, stat.S_IFDIR | 0o700, os.geteuid(), os.getegid(), 0, 0, 0),
                files=(),
            )
            runtime_checks = 0

            def revalidate(*args: object) -> None:
                nonlocal runtime_checks
                runtime_checks += 1
                if runtime_checks == 2:
                    raise capsule.CapsuleFailure("mounted image identity drifted")

            detached = mock.Mock()
            with (
                mock.patch.object(capsule, "attach_read_only"),
                mock.patch.object(capsule, "require_read_only_mount"),
                mock.patch.object(capsule, "capture_runtime_identity", return_value=mounted),
                mock.patch.object(capsule, "verify_inventory", return_value=SOURCE_DIGEST),
                mock.patch.object(capsule, "approval_path", return_value=None),
                mock.patch.object(capsule, "revalidate_image"),
                mock.patch.object(capsule, "revalidate_runtime_identity", side_effect=revalidate),
                mock.patch.object(
                    capsule,
                    "run_stage_one",
                    return_value=subprocess.CompletedProcess(["probe"], 0, b"", b""),
                ),
                mock.patch.object(capsule, "detach_without_force", detached),
                mock.patch.object(capsule, "require_underlying_mount_root"),
            ):
                with self.assertRaisesRegex(capsule.CapsuleFailure, "mounted image identity drifted"):
                    capsule.stage_zero(
                        self.image_arguments(
                            image=image,
                            expected_image_sha256=capsule.image_sha256(image),
                            mount_parent=parent / "mounts",
                        )
                    )
            self.assertEqual(runtime_checks, 2)
            detached.assert_called_once()

    def test_digest_file_is_reserved_for_the_build_probe(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            image.write_bytes(b"exact image")
            image.chmod(0o600)
            digest_file = parent / "capsule.dmg.sha256"
            digest_file.write_text(capsule.image_sha256(image) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(capsule.CapsuleFailure, "only the build probe"):
                capsule.stage_zero(
                    self.image_arguments(
                        image=image,
                        expected_image_sha256_file=digest_file,
                        probe_launch=False,
                        attended_launch=True,
                        approval=parent / "approval",
                        mount_parent=parent / "mounts",
                    )
                )

    def test_stage_zero_source_contains_no_forced_detach_argument(self) -> None:
        self.assertNotIn('"-force"', SCRIPT.read_text(encoding="utf-8"))

    @unittest.skipUnless(
        pathlib.Path("/usr/bin/hdiutil").exists(), "hdiutil is macOS-only"
    )
    def test_real_image_round_trip_is_read_only_and_detaches_without_force(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            root, inventory = make_capsule(parent)
            approval = parent / "external-approval.txt"
            approval.write_text("must stay outside the image\n", encoding="utf-8")
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
                observed["approval"] = any(
                    path.name == approval.name for path in mount_root.rglob("*")
                )
                probe = mount_root / "write-probe"
                try:
                    probe.write_bytes(b"x")
                    observed["writable"] = True
                except OSError:
                    observed["writable"] = False
                original(mount_root)

            launched: list[tuple[pathlib.Path, pathlib.Path, pathlib.Path | None]] = []

            def probe(
                helper: pathlib.Path,
                mounted_inventory: pathlib.Path,
                approval: pathlib.Path | None,
                _signals: capsule.StageZeroSignalScope,
            ) -> subprocess.CompletedProcess[bytes]:
                launched.append((helper, mounted_inventory, approval))
                early_detach = subprocess.run(
                    [
                        "/usr/bin/hdiutil",
                        "detach",
                        str(mounted_inventory.parent),
                        "-quiet",
                    ],
                    check=False,
                    capture_output=True,
                )
                observed["early_detach"] = early_detach.returncode
                return subprocess.CompletedProcess([str(helper)], 0, b"", b"")

            with (
                mock.patch.object(capsule, "require_read_only_mount", observe),
                mock.patch.object(capsule, "run_stage_one", probe),
            ):
                capsule.stage_zero(
                    self.image_arguments(
                        image=image,
                        expected_image_sha256=digest,
                        mount_parent=parent / "mounts",
                    )
                )
            self.assertTrue(observed["capsule"], "the app is inside the image")
            self.assertTrue(observed["inventory"], "the inventory is inside the image")
            self.assertFalse(observed["approval"], "approval never enters the image")
            self.assertFalse(observed["writable"], "the attached image refuses writes")
            self.assertEqual(len(launched), 1)
            self.assertIn("mount-", str(launched[0][0]))
            self.assertIsNone(launched[0][2])
            self.assertNotEqual(
                observed["early_detach"],
                0,
                "retained mount descriptors must refuse ordinary early detach",
            )
            # stage_zero ordinary-detaches on the way out; nothing of this run
            # may still be attached.
            listing = subprocess.run(
                ["/usr/bin/hdiutil", "info"],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotIn(str(image), listing.stdout)

    @unittest.skipUnless(
        pathlib.Path("/usr/bin/hdiutil").exists(), "hdiutil is macOS-only"
    )
    def test_sigterm_reaps_blocked_child_group_and_detaches_real_image(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            root, inventory = make_capsule(parent)
            helper_ready = parent / "helper-ready"
            helper_pid = parent / "helper-pid"
            descendant_ready = parent / "descendant-ready"
            descendant_pid = parent / "descendant-pid"
            descendant_source = (
                "import os, pathlib, signal, time\n"
                "signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
                f"pathlib.Path({str(descendant_pid)!r}).write_text(str(os.getpid()))\n"
                f"pathlib.Path({str(descendant_ready)!r}).write_text('ready')\n"
                "while True:\n"
                "    time.sleep(1)\n"
            )
            helper_source = (
                "#!/usr/bin/python3\n"
                "import os, pathlib, subprocess, time\n"
                f"subprocess.Popen(['/usr/bin/python3', '-c', {descendant_source!r}])\n"
                f"descendant_ready = pathlib.Path({str(descendant_ready)!r})\n"
                "while not descendant_ready.exists():\n"
                "    time.sleep(0.01)\n"
                f"pathlib.Path({str(helper_pid)!r}).write_text(str(os.getpid()))\n"
                f"pathlib.Path({str(helper_ready)!r}).write_text('ready')\n"
                "while True:\n"
                "    time.sleep(1)\n"
            )
            helper = root / capsule.HELPER_RELATIVE
            helper.write_text(helper_source, encoding="utf-8")
            helper.chmod(0o755)
            inventory.write_text(
                capsule.render_inventory(root, SOURCE_DIGEST),
                encoding="utf-8",
                newline="\n",
            )
            image = parent / "capsule.dmg"
            capsule.build_image(
                self.image_arguments(
                    capsule=root,
                    inventory=inventory,
                    output=image,
                    image_digest=None,
                )
            )
            mount_parent = (parent / "mounts").resolve(strict=False)
            process = subprocess.Popen(
                [
                    sys.executable,
                    str(SCRIPT),
                    "stage-zero",
                    "--image",
                    str(image.resolve()),
                    "--expected-image-sha256",
                    capsule.image_sha256(image),
                    "--mount-parent",
                    str(mount_parent),
                    "--source-sha256",
                    SOURCE_DIGEST,
                    "--probe-launch",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertIsNotNone(process.stdout)
            mounted_marker = ""
            deadline = time.monotonic() + 20.0
            while time.monotonic() < deadline and process.poll() is None:
                readable, _, _ = select.select([process.stdout], [], [], 0.1)
                if not readable:
                    continue
                line = process.stdout.readline()
                if "designated-live-macos-capsule:mounted:" in line:
                    mounted_marker = line
                    break
            self.assertIn("designated-live-macos-capsule:mounted:", mounted_marker)
            while time.monotonic() < deadline and not helper_ready.exists():
                if process.poll() is not None:
                    break
                time.sleep(0.01)
            self.assertTrue(helper_ready.exists())
            child_processes = (
                int(helper_pid.read_text(encoding="utf-8")),
                int(descendant_pid.read_text(encoding="utf-8")),
            )
            process.send_signal(signal.SIGTERM)
            stdout, stderr = process.communicate(timeout=30)
            self.assertNotEqual(process.returncode, 0, stdout + stderr)
            self.assertIn("stage zero interrupted by signal", stderr)
            for process_id in child_processes:
                self.assert_process_gone(process_id)
            listing = subprocess.run(
                ["/usr/bin/hdiutil", "info"],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotIn(str(image.resolve()), listing.stdout)


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
