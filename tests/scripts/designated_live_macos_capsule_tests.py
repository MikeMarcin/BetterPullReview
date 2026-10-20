#!/usr/bin/env python3
"""Hostile filesystem and runtime-contract tests for the macOS live capsule."""

from __future__ import annotations

import argparse
from collections.abc import Callable, Iterator
import contextlib
from pathlib import Path
import importlib.util
import io
import os
import pathlib
import plistlib
import re
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
from skip_report import SkipReportingTestRunner  # noqa: E402

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

    def assert_process_gone(
        self, process_id: int, deadline: float | None = None
    ) -> None:
        if deadline is None:
            deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                os.kill(process_id, 0)
            except ProcessLookupError:
                return
            time.sleep(0.05)
        self.fail(f"process {process_id} remained alive")

    def assert_process_group_gone(self, process_group: int) -> None:
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if not capsule.process_group_exists(process_group):
                return
            time.sleep(0.05)
        self.fail(f"process group {process_group} remained alive")

    def reap_process_group(self, process: subprocess.Popen[bytes]) -> None:
        if capsule.process_group_exists(process.pid):
            capsule.signal_process_group(process.pid, signal.SIGKILL)
        try:
            process.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            pass

    def blocking_helper(
        self,
        root: pathlib.Path,
        *,
        ignore_term: bool,
    ) -> tuple[pathlib.Path, pathlib.Path]:
        """Writes a helper that blocks forever and says when it is ready.

        The readiness marker is created last, after the optional SIGTERM
        handler is installed, so its existence proves the child finished the
        setup a caller is about to depend on. Nothing here may infer that from
        elapsed time. Interpreter startup under a loaded parallel matrix is
        unbounded, and BPR-0454 records both cases this file lost to assuming
        otherwise.
        """
        ready = root / "helper-ready"
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
            + f"pathlib.Path({str(ready)!r}).write_text(str(os.getpid()))\n"
            # Bounded rather than endless. Every case here kills this child
            # within seconds, so the ceiling is invisible to them -- but a
            # case that fails to, or a mutation run that removes the cleanup
            # under test, would otherwise leave a blocked interpreter running
            # until the machine is rebooted. One was observed during BPR-0454.
            + "for _ in range(300):\n"
            + "    time.sleep(1)\n",
            encoding="utf-8",
        )
        helper.chmod(0o700)
        return helper, ready

    def await_helper_ready(
        self,
        ready: pathlib.Path,
        process: subprocess.Popen[bytes],
    ) -> None:
        deadline = time.monotonic() + 60.0
        while time.monotonic() < deadline:
            if ready.exists():
                return
            self.assertIsNone(
                process.poll(), "the helper exited before it became ready"
            )
            time.sleep(0.01)
        self.fail(f"the helper never became ready: {ready}")

    @staticmethod
    def attached_whole_disks(image: pathlib.Path) -> list[str]:
        """The whole-disk devices of every attachment of exactly `image`.

        Matching on this case's own image path is the whole safety argument
        for detaching what comes back: an attachment of any other image,
        however alike, is never returned.
        """
        listing = subprocess.run(
            ["/usr/bin/hdiutil", "info", "-plist"],
            check=True,
            capture_output=True,
        )
        target = os.path.realpath(image)
        return [
            entity["dev-entry"]
            for attached in plistlib.loads(listing.stdout).get("images", [])
            if attached.get("image-path")
            and os.path.realpath(attached["image-path"]) == target
            for entity in attached.get("system-entities", [])
            if re.fullmatch(r"/dev/disk[0-9]+", entity.get("dev-entry", ""))
        ]

    def detach_image(self, image: pathlib.Path) -> None:
        """Ordinary-detaches this case's image until nothing of it is attached.

        Never forced: a mount that something still holds is a failure to
        report, not one to tear out from under its holder. Each attempt
        re-lists and tries the next whole disk, because detaching the physical
        store removes the APFS container synthesized on it but the listing
        does not say which disk is the store.
        """
        deadline = time.monotonic() + 60.0
        attempt = 0
        while disks := self.attached_whole_disks(image):
            if time.monotonic() >= deadline:
                self.fail(f"{image} is still attached as {disks}")
            subprocess.run(
                ["/usr/bin/hdiutil", "detach", disks[attempt % len(disks)], "-quiet"],
                check=False,
                capture_output=True,
            )
            attempt += 1
            time.sleep(0.1)

    @contextlib.contextmanager
    def real_image_directory(self) -> Iterator[tuple[pathlib.Path, pathlib.Path]]:
        """A contained directory whose `capsule.dmg` is never left attached.

        The detach runs before the directory is removed, on every exit path,
        so a failed case cannot leave a device behind (BPR-0814).
        """
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory)
            image = parent / "capsule.dmg"
            try:
                yield parent, image
            finally:
                self.detach_image(image)

    def build_real_image(
        self, root: pathlib.Path, inventory: pathlib.Path, image: pathlib.Path
    ) -> None:
        """Builds `image` and asserts that none of it is still attached.

        BPR-0814. `hdiutil create` writes this image through a write-once
        device. While other copies of this suite create images at the same
        time, it can exit 0 with that device still attached, and the device
        stays attached. The unified log for one such create shows its helper
        ejecting the APFS container synthesized on the device rather than the
        device itself. The image bytes were already final, and an ordinary
        detach of the device removed it. A stage-zero attach of that image
        failed.

        BPR-0836. The production image build now detaches what its own create
        leaves attached, so this fixture no longer repairs it: concurrent
        copies of this suite check the real device instead.
        `real_image_directory` still detaches on every exit path.
        """
        capsule.build_image(
            self.image_arguments(
                capsule=root,
                inventory=inventory,
                output=image,
                image_digest=None,
            )
        )
        self.assertEqual(
            self.attached_whole_disks(image),
            [],
            "the image build returned with its own image still attached",
        )

    SIMULATOR_IMAGE = (
        "/System/Library/AssetsV2/com_apple_MobileAsset_iOSSimulatorRuntime/"
        "example.asset/AssetData/Restore/example.dmg"
    )

    @staticmethod
    def attachment_listing(attached: dict[str, list[str]]) -> str:
        """`hdiutil info -plist` output for these image paths and devices."""
        return plistlib.dumps(
            {
                "images": [
                    {
                        "image-path": path,
                        "system-entities": [
                            {"dev-entry": device} for device in devices
                        ],
                    }
                    for path, devices in attached.items()
                ]
            }
        ).decode("utf-8")

    def leaking_image_tool(
        self,
        image: pathlib.Path,
        events: list[list[str]],
        *,
        store: str | None,
        create_returncode: int = 0,
        listing: subprocess.CompletedProcess[str] | None = None,
    ) -> Callable[[list[str]], subprocess.CompletedProcess[str]]:
        """A fake `hdiutil` whose create leaves `image` attached (BPR-0836).

        A real leak cannot be forced on demand: it needs other creates running
        and then happens only some of the time. This seam replays its shape.
        The leaked attachment lists its synthesized container before its
        physical store, and only an ordinary detach of `store` removes it
        (`None`: nothing does). Attachments of other images stay attached
        throughout, including a look-alike beside this image and one whose
        path merely extends it. `listing`, when given, replaces every listing.
        """
        own = os.path.realpath(image)
        attached = {
            self.SIMULATOR_IMAGE: ["/dev/disk4", "/dev/disk4s1", "/dev/disk5"],
            str(image.parent / "neighbor" / image.name): ["/dev/disk20", "/dev/disk21"],
            own + ".shadow": ["/dev/disk22"],
        }

        def tool(arguments: list[str]) -> subprocess.CompletedProcess[str]:
            events.append(list(arguments))
            if arguments[1] == "create":
                pathlib.Path(arguments[-1]).write_bytes(b"created image")
                attached[own] = ["/dev/disk11", "/dev/disk11s1", "/dev/disk10"]
                return subprocess.CompletedProcess(
                    arguments,
                    create_returncode,
                    "",
                    "" if create_returncode == 0 else "create refused",
                )
            if arguments[1:] == ["info", "-plist"]:
                if listing is not None:
                    return listing
                return subprocess.CompletedProcess(
                    arguments, 0, self.attachment_listing(attached), ""
                )
            if arguments[1] == "detach":
                if own in attached and arguments[2] == store:
                    del attached[own]
                    return subprocess.CompletedProcess(arguments, 0, "", "")
                return subprocess.CompletedProcess(
                    arguments, 16, "", "hdiutil: detach failed - Resource busy"
                )
            raise AssertionError(f"unexpected hdiutil call: {arguments}")

        return tool

    def test_image_build_detaches_its_own_leaked_device_before_reporting_a_digest(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory).resolve()
            root, inventory = make_capsule(parent)
            image = parent / "capsule.dmg"
            digest_file = parent / "capsule.dmg.sha256"
            events: list[list[str]] = []
            tool = self.leaking_image_tool(image, events, store="/dev/disk10")
            reported = io.StringIO()

            def observed(arguments: list[str]) -> subprocess.CompletedProcess[str]:
                # Nothing is reported while any hdiutil step is still running.
                self.assertEqual(reported.getvalue(), "")
                self.assertFalse(digest_file.exists())
                return tool(arguments)

            with (
                mock.patch.object(capsule, "command", observed),
                mock.patch.object(capsule.time, "sleep"),
                contextlib.redirect_stdout(reported),
            ):
                capsule.build_image(
                    self.image_arguments(
                        capsule=root,
                        inventory=inventory,
                        output=image,
                        image_digest=digest_file,
                    )
                )
            self.assertEqual(
                [event[1] for event in events],
                ["create", "info", "detach", "info", "detach", "info"],
            )
            self.assertEqual(
                [event for event in events if event[1] == "detach"],
                [
                    ["/usr/bin/hdiutil", "detach", "/dev/disk11", "-quiet"],
                    ["/usr/bin/hdiutil", "detach", "/dev/disk10", "-quiet"],
                ],
                "only whole disks of this image, one per re-listing, never forced",
            )
            digest = capsule.image_sha256(image)
            self.assertEqual(digest_file.read_text(encoding="utf-8"), f"{digest}\n")
            self.assertEqual(
                reported.getvalue(), f"designated-live-macos-capsule:image:{digest}\n"
            )

    def test_image_build_refuses_without_a_digest_while_its_image_stays_attached(
        self,
    ) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory).resolve()
            root, inventory = make_capsule(parent)
            image = parent / "capsule.dmg"
            digest_file = parent / "capsule.dmg.sha256"
            events: list[list[str]] = []
            clock = [0.0]

            def advance(seconds: float) -> None:
                clock[0] += seconds

            stdout, stderr = io.StringIO(), io.StringIO()
            argv = [
                str(SCRIPT),
                "image",
                "--capsule",
                str(root),
                "--inventory",
                str(inventory),
                "--output",
                str(image),
                "--image-digest",
                str(digest_file),
                "--source-sha256",
                SOURCE_DIGEST,
            ]
            with (
                mock.patch.object(
                    capsule, "command", self.leaking_image_tool(image, events, store=None)
                ),
                mock.patch.object(capsule, "IMAGE_DETACH_TIMEOUT_SECONDS", 0.35),
                mock.patch.object(capsule.time, "monotonic", lambda: clock[0]),
                mock.patch.object(capsule.time, "sleep", advance),
                mock.patch.object(sys, "argv", argv),
                contextlib.redirect_stdout(stdout),
                contextlib.redirect_stderr(stderr),
            ):
                self.assertEqual(capsule.main(), 1)
            self.assertEqual(
                stderr.getvalue(),
                "designated-live-macos-capsule:fail:image is still attached after "
                "creation as /dev/disk11, /dev/disk10: "
                "hdiutil: detach failed - Resource busy\n",
            )
            self.assertEqual(stdout.getvalue(), "", "no digest and no pass while attached")
            self.assertFalse(digest_file.exists())
            self.assertEqual(
                [event[1:] for event in events[1:]],
                [
                    ["info", "-plist"],
                    ["detach", "/dev/disk11", "-quiet"],
                    ["info", "-plist"],
                    ["detach", "/dev/disk10", "-quiet"],
                    ["info", "-plist"],
                    ["detach", "/dev/disk11", "-quiet"],
                    ["info", "-plist"],
                    ["detach", "/dev/disk10", "-quiet"],
                    ["info", "-plist"],
                ],
            )

    def test_image_build_refuses_a_listing_that_cannot_show_it_detached(self) -> None:
        cases = (
            ("listing failed", 1, "", "hdiutil: info failed"),
            ("listing is unreadable", 0, "not a property list", ""),
            ("listing is unreadable", 0, plistlib.dumps(["images"]).decode("utf-8"), ""),
            ("attached without a whole disk", 0, None, ""),
        )
        for index, (reason, returncode, stdout, stderr) in enumerate(cases):
            with self.subTest(case=index), contained_temporary_directory() as directory:
                parent = pathlib.Path(directory).resolve()
                root, inventory = make_capsule(parent)
                image = parent / "capsule.dmg"
                digest_file = parent / "capsule.dmg.sha256"
                if stdout is None:
                    stdout = self.attachment_listing(
                        {os.path.realpath(image): ["/dev/disk10s1"]}
                    )
                events: list[list[str]] = []
                tool = self.leaking_image_tool(
                    image,
                    events,
                    store="/dev/disk10",
                    listing=subprocess.CompletedProcess([], returncode, stdout, stderr),
                )
                reported = io.StringIO()
                with (
                    mock.patch.object(capsule, "command", tool),
                    contextlib.redirect_stdout(reported),
                    self.assertRaisesRegex(capsule.CapsuleFailure, reason),
                ):
                    capsule.build_image(
                        self.image_arguments(
                            capsule=root,
                            inventory=inventory,
                            output=image,
                            image_digest=digest_file,
                        )
                    )
                self.assertEqual([event[1] for event in events], ["create", "info"])
                self.assertEqual(reported.getvalue(), "")
                self.assertFalse(digest_file.exists())

    def test_failed_image_create_is_named_before_its_detach_refusal(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory).resolve()
            root, inventory = make_capsule(parent)
            image = parent / "capsule.dmg"
            events: list[list[str]] = []
            tool = self.leaking_image_tool(
                image,
                events,
                store="/dev/disk10",
                create_returncode=1,
                listing=subprocess.CompletedProcess([], 1, "", "hdiutil: info failed"),
            )
            with (
                mock.patch.object(capsule, "command", tool),
                self.assertRaises(capsule.CapsuleFailure) as refused,
            ):
                capsule.build_image(
                    self.image_arguments(
                        capsule=root,
                        inventory=inventory,
                        output=image,
                        image_digest=None,
                    )
                )
            self.assertEqual(
                str(refused.exception),
                "image creation failed: create refused; "
                "image attachment listing failed: hdiutil: info failed",
            )
            self.assertEqual([event[1] for event in events], ["create", "info"])

    def test_failed_image_create_still_detaches_its_own_device(self) -> None:
        with contained_temporary_directory() as directory:
            parent = pathlib.Path(directory).resolve()
            root, inventory = make_capsule(parent)
            image = parent / "capsule.dmg"
            events: list[list[str]] = []
            tool = self.leaking_image_tool(
                image, events, store="/dev/disk10", create_returncode=1
            )
            with (
                mock.patch.object(capsule, "command", tool),
                mock.patch.object(capsule.time, "sleep"),
                self.assertRaisesRegex(
                    capsule.CapsuleFailure, "image creation failed: create refused"
                ),
            ):
                capsule.build_image(
                    self.image_arguments(
                        capsule=root,
                        inventory=inventory,
                        output=image,
                        image_digest=None,
                    )
                )
            self.assertEqual(
                [event[1:3] for event in events[1:]],
                [
                    ["info", "-plist"],
                    ["detach", "/dev/disk11"],
                    ["info", "-plist"],
                    ["detach", "/dev/disk10"],
                    ["info", "-plist"],
                ],
            )

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

    def test_attended_profile_is_explicit_and_cannot_enter_probe(self) -> None:
        process = mock.Mock(pid=123, wait=mock.Mock(return_value=0))
        with mock.patch.object(capsule.subprocess, "Popen", return_value=process) as spawn, mock.patch.object(capsule, "process_group_exists", return_value=False):
            capsule.run_stage_one(pathlib.Path("/helper"), pathlib.Path("/inventory"), pathlib.Path("/approval"), profile="2")
            self.assertEqual(spawn.call_args.args[0], ["/helper", "--attended-launch", "--inventory", "/inventory", "--approval", "/approval", "--bpr-profile", "2"])
            spawn.reset_mock()
            with self.assertRaises(capsule.CapsuleFailure):
                capsule.run_stage_one(pathlib.Path("/helper"), pathlib.Path("/inventory"), None, profile="2")
            spawn.assert_not_called()
            for value in ("", "0", "02", "-2", "+2", "2 ", "٢", "18446744073709551616", "2 --other"):
                with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                    capsule.run_stage_one(pathlib.Path("/helper"), pathlib.Path("/inventory"), pathlib.Path("/approval"), profile=value)
            spawn.assert_not_called()
        self.assertEqual(capsule.profile_id("18446744073709551615"), "18446744073709551615")
        with self.assertRaises(capsule.CapsuleFailure):
            capsule.approval_path(self.image_arguments(bpr_profile="2"), pathlib.Path("/mount"))

    def test_stage_one_timeout_terminates_cooperative_process_group(self) -> None:
        """The deadline reaches cleanup, and one SIGTERM is enough.

        This case owns the wiring: an overrun stage one raises, and its
        process group is signalled and gone. It deliberately depends on
        nothing the child does after it is forked. The group identity comes
        from the recorded call rather than from a file the child has to live
        long enough to write, so a slow interpreter start cannot decide
        whether the cleanup claim is checked at all.
        """
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            helper, _ = self.blocking_helper(root, ignore_term=False)
            delivered: list[signal.Signals] = []
            groups: list[int] = []
            original = capsule.signal_process_group

            def record(process_group: int, requested: signal.Signals) -> None:
                delivered.append(requested)
                groups.append(process_group)
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
            self.assertEqual(len(set(groups)), 1)
            self.assert_process_group_gone(groups[0])

    def test_stage_one_cleanup_kills_a_term_ignoring_process_group(self) -> None:
        """A group that ignores SIGTERM is escalated to SIGKILL, and waited for.

        The escalation is the claim, so the child's SIGTERM handler has to be
        installed before the graded window opens. That is why this case owns
        the spawn and waits for readiness instead of driving
        ``run_stage_one``: the stage-one deadline starts at the fork, so
        nothing reached through it can wait for the child first, and a child
        signalled mid-startup dies on the default disposition and never
        escalates. The wiring that case is named for stays covered above.

        The term grace stays at 0.2 seconds. Widening it until the case stops
        failing would also stop it proving that the escalation is prompt, so
        the interval is asserted instead: half the grace is far more than the
        zero a lost wait would produce, and load can only lengthen it.
        """
        grace = 0.2
        with contained_temporary_directory() as directory:
            root = pathlib.Path(directory)
            helper, ready = self.blocking_helper(root, ignore_term=True)
            process = subprocess.Popen([str(helper)], start_new_session=True)
            self.addCleanup(self.reap_process_group, process)
            self.await_helper_ready(ready, process)

            delivered: list[tuple[signal.Signals, float]] = []
            original = capsule.signal_process_group

            def record(process_group: int, requested: signal.Signals) -> None:
                delivered.append((requested, time.monotonic()))
                original(process_group, requested)

            with (
                mock.patch.object(capsule, "STAGE_ONE_TERM_GRACE_SECONDS", grace),
                mock.patch.object(capsule, "STAGE_ONE_KILL_GRACE_SECONDS", 1.0),
                mock.patch.object(capsule, "signal_process_group", side_effect=record),
            ):
                capsule.terminate_process_group(process)

            self.assertEqual(
                [requested for requested, _ in delivered],
                [signal.SIGTERM, signal.SIGKILL],
            )
            self.assertGreaterEqual(
                delivered[1][1] - delivered[0][1],
                grace / 2.0,
                "SIGKILL followed SIGTERM without waiting out the term grace",
            )
            self.assert_process_group_gone(process.pid)

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
        with self.real_image_directory() as (parent, image):
            root, inventory = make_capsule(parent)
            approval = parent / "external-approval.txt"
            approval.write_text("must stay outside the image\n", encoding="utf-8")
            self.build_real_image(root, inventory, image)
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
        with self.real_image_directory() as (parent, image):
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
                # Bounded like blocking_helper's, so a failed reap cannot leave
                # a SIGTERM-ignoring interpreter running until reboot.
                "for _ in range(300):\n"
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
                "for _ in range(300):\n"
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
            self.build_real_image(root, inventory, image)
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
            try:
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
                # BPR-0692. The reap and the detach run on different clocks. Stage
                # zero bounds the reap by its own term and kill graces, so that is
                # asserted from the signal. The ordinary detach that follows waits
                # on DiskArbitration's unmount and eject, which stage zero does not
                # bound: one eject took 10.7 s under spinning-process load, and a
                # single 30-second window already spent 10 s of it on the term
                # grace this SIGTERM-ignoring descendant forces. So the detach gets
                # a hang bound, not a promptness claim.
                reap_deadline = (
                    time.monotonic()
                    + capsule.STAGE_ONE_TERM_GRACE_SECONDS
                    + capsule.STAGE_ONE_KILL_GRACE_SECONDS
                    + 10.0
                )
                for process_id in child_processes:
                    self.assert_process_gone(process_id, reap_deadline)
                stdout, stderr = process.communicate(timeout=180)
                self.assertNotEqual(process.returncode, 0, stdout + stderr)
                self.assertIn("stage zero interrupted by signal", stderr)
                listing = subprocess.run(
                    ["/usr/bin/hdiutil", "info"],
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertNotIn(str(image.resolve()), listing.stdout)
            finally:
                # BPR-0814. An assertion that fails above leaves stage zero
                # running over a mounted image that an ordinary detach cannot
                # remove while the helper holds it. Stage zero's own SIGTERM
                # path reaps the helper group and detaches, so use that first;
                # real_image_directory detaches whatever remains.
                if process.poll() is None:
                    process.send_signal(signal.SIGTERM)
                    try:
                        process.communicate(timeout=180)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.communicate()
                        # Stage zero starts the helper in a new session, so a
                        # killed stage zero leaves the helper's group behind.
                        if helper_pid.exists():
                            group = int(helper_pid.read_text(encoding="utf-8"))
                            if capsule.process_group_exists(group):
                                capsule.signal_process_group(group, signal.SIGKILL)


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
            "\t@rpath/QtCore (compatibility version 6.0.0, current version 6.12.0)\n"
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
    unittest.main(testRunner=SkipReportingTestRunner)
