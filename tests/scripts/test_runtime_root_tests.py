#!/usr/bin/env python3
"""[[BPR-1942]]. Refuse unsafe designated runtime roots without touching them."""

import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from validate_test_runtime_root import validate_runtime_root


class DefaultRuntimeRootTests(unittest.TestCase):
    def test_empty_default_never_inspects_or_changes_paths(self) -> None:
        with mock.patch.object(Path, "resolve", side_effect=AssertionError("default inspected disk")):
            self.assertIsNone(validate_runtime_root("", Path("source"), Path("build")))

    def test_non_posix_override_refuses_before_accessing_disk(self) -> None:
        with mock.patch.object(os, "name", "nt"):
            with self.assertRaisesRegex(ValueError, "only on POSIX"):
                validate_runtime_root("unused", None, None)


@unittest.skipUnless(os.name == "posix", "the optional root is POSIX-only")
class PrivateRuntimeRootTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="bpr-runtime-root-")
        self.addCleanup(self.temporary.cleanup)
        self.home = Path(self.temporary.name).resolve()
        self.source = self.home / "source"
        self.build = self.source / "build"
        self.runtime = self.home / "native runtime"
        for path in (self.source, self.build, self.runtime):
            path.mkdir(mode=0o700)

    def check(self, value: str) -> Path | None:
        return validate_runtime_root(value, self.source, self.build, self.home)

    def test_dedicated_private_child_of_home_with_spaces_is_accepted(self) -> None:
        self.assertEqual(self.check(str(self.runtime)), self.runtime)

    def test_relative_list_and_control_inputs_refuse(self) -> None:
        for value in ("relative", str(self.runtime) + ";other", str(self.runtime) + "\n", str(self.runtime) + "\t"):
            with self.subTest(value=repr(value)), self.assertRaises(ValueError):
                self.check(value)

    def test_missing_file_symlink_and_symlinked_ancestor_refuse(self) -> None:
        file = self.home / "file"
        file.write_text("fixture stays intact")
        link = self.home / "link"
        link.symlink_to(self.runtime, target_is_directory=True)
        loop = self.home / "loop"
        loop.symlink_to(loop, target_is_directory=True)
        child = self.runtime / "child"
        child.mkdir(mode=0o700)
        for path in (self.home / "missing", file, link, link / "child", loop):
            with self.subTest(path=path.name), self.assertRaises(ValueError):
                self.check(str(path))
        self.assertEqual(file.read_text(), "fixture stays intact")
        self.assertTrue(link.is_symlink())

    def test_shared_and_foreign_owned_directory_refuse_without_narrowing(self) -> None:
        self.runtime.chmod(0o755)
        with self.assertRaisesRegex(ValueError, "mode 0700"):
            self.check(str(self.runtime))
        self.assertEqual(self.runtime.stat().st_mode & 0o777, 0o755)
        self.runtime.chmod(0o700)
        with mock.patch.object(os, "geteuid", return_value=self.runtime.stat().st_uid + 1):
            with self.assertRaisesRegex(ValueError, "current user"):
                self.check(str(self.runtime))

    def test_source_build_and_home_collisions_refuse(self) -> None:
        source_child = self.source / "runtime"
        build_child = self.build / "runtime"
        for path in (source_child, build_child):
            path.mkdir(mode=0o700)
        for path in (self.home, self.source, self.build, source_child, build_child):
            with self.subTest(path=path.name), self.assertRaises(ValueError):
                self.check(str(path))


if __name__ == "__main__":
    unittest.main()
