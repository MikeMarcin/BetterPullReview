#!/usr/bin/env python3
"""Hermetic tests for the RFC-BPR-0017 evidence-selector inventory.

The script under test asks CTest which tests a configured graph registers and
compares that list to a table of selectors, each anchored on the sibling
registration that proves the graph builds the surface it tests. These tests
feed it inventories directly, so no build tree, CTest, or network is involved,
and prove the refusal table: a missing selector, a stray one, an anchored
selector insisted on where its graph does not owe it, an unknown name, and a
selector whose command lost the argument that makes it the right suite.
"""
from __future__ import annotations

import ast
import contextlib
import importlib.util
import io
import pathlib
import sys
import unittest
from unittest import mock

MINIMUM_PYTHON = (3, 10)
if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "verify_rfc0017_test_inventory_tests.py requires Python {}.{} or "
        "newer; this is {}.{}.{} at {}.\n".format(
            MINIMUM_PYTHON[0], MINIMUM_PYTHON[1], sys.version_info[0],
            sys.version_info[1], sys.version_info[2], sys.executable))
    raise SystemExit(2)

REPOSITORY = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY / "scripts" / "verify_rfc0017_test_inventory.py"
sys.path.insert(0, str(REPOSITORY))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def load_module():
    specification = importlib.util.spec_from_file_location(
        "verify_rfc0017_test_inventory", SCRIPT)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


NATIVE = "rfc0017_credential_free_native_https"
CORE = "core_anonymous_public_read"
CONTRACT = "native_https_anonymous_read_contract"
FIXTURE = "native_https_anonymous_read_fixture"
PROVIDER = "github_anonymous_read_contract"
HOST = "application_anonymous_read_host"
ANCHOR = "native_https_transport_fixture"
QT_ANCHOR = "github_read_contract"
HOST_ANCHOR = "application_github_host"
GIT = "git_anonymous_public_read"
GIT_ANCHOR = "git_cache_contract"
SQLITE = "sqlite_anonymous_epoch_authority"
SQLITE_ANCHOR = "sqlite_epoch_catalog_storage_contract"
DIFF = "application_anonymous_diff_authority"
QT_SCALES = tuple(f"qt_anonymous_public_preview_scale_{scale}" for scale in (100, 125, 200))
QT_SCALE_ANCHORS = tuple(f"qt_composed_github_scale_{scale}" for scale in (100, 125, 200))
PREVIEW_ROWS = (
    ("application_anonymous_public_preview_unavailable",
     "application_device_authorization_unavailable", ("--public-preview-probe",)),
    ("application_anonymous_public_preview_release_contract",
     "application_device_authorization_release_contract", ("--public-preview-probe",)),
    ("application_anonymous_public_preview_ghes_refusal",
     "application_github_public_path_ghes_3_18", ("--public-preview-ghes-course",)),
    ("application_anonymous_public_preview_public_path",
     "application_github_public_path_github_com", ("--public-preview-enabled-course",)),
    ("application_anonymous_public_preview_enabled_probe",
     "application_device_authorization_unavailable", ("--public-preview-selection-enabled-probe",)),
    ("application_anonymous_public_preview_live_candidate_wiring",
     "application_device_authorization_unavailable", ("--public-preview-live-candidate-dry-run",)),
)
BUILD = pathlib.Path("/nonexistent/build")

# The wire suites: registered only inside the APPLE/Linux wire block.
WIRE_SUITES = (NATIVE, FIXTURE)

DEV_GRAPH = [
    (ANCHOR, ("fixture",)),
    (QT_ANCHOR, ("read_contract",)),
    (NATIVE, ("runner", "--suite", "rfc0017")),
    (FIXTURE, ("runner", "--suite", "anonymous_read")),
    (CONTRACT, ("anonymous_read_tests", "--contract")),
    (PROVIDER, ("provider_tests",)),
    (HOST_ANCHOR, ("host_tests",)),
    (HOST, ("anonymous_host_tests",)),
    (DIFF, ("anonymous_diff_tests",)),
    *((anchor, ("composed_tests",)) for anchor in QT_SCALE_ANCHORS),
    *((selector, ("preview_tests",)) for selector in QT_SCALES),
    *((anchor, ("app", "--self-test")) for _, anchor, _ in PREVIEW_ROWS),
    *((selector, ("app", "--self-test", *arguments)) for selector, _, arguments in PREVIEW_ROWS),
    (GIT_ANCHOR, ("git_cache_tests",)),
    (GIT, ("anonymous_git_tests",)),
    (SQLITE_ANCHOR, ("catalog_storage_tests",)),
    (SQLITE, ("anonymous_catalog_tests",)),
    (CORE, ("core_tests",)),
    ("unrelated", ("other",)),
]
CORE_ONLY_GRAPH = [
    (CORE, ("core_tests",)),
    ("unrelated", ("other",)),
]


class InventoryTable(unittest.TestCase):
    def setUp(self) -> None:
        self.module = load_module()

    def check(self, graph, platform="darwin", **keywords):
        with mock.patch.object(
            self.module, "registered_tests", return_value=list(graph)
        ), mock.patch.object(self.module.sys, "platform", platform):
            return self.module.check_inventory(BUILD, None, **keywords)

    def test_module_imports_no_network_client(self) -> None:
        tree = ast.parse(SCRIPT.read_text(encoding="utf-8"))
        names = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                names.update(alias.name for alias in node.names)
            elif isinstance(node, ast.ImportFrom) and node.module:
                names.add(node.module)
        for forbidden in ("socket", "http", "urllib", "ssl", "requests"):
            self.assertFalse(
                any(name == forbidden or name.startswith(forbidden + ".")
                    for name in names),
                f"{forbidden} imported")

    def test_every_selector_has_exactly_one_anchor_entry(self) -> None:
        self.assertEqual(
            set(self.module.SELECTORS), set(self.module.SELECTOR_ANCHORS))
        anchors = self.module.SELECTOR_ANCHORS
        self.assertEqual(anchors[CORE], (None, False))
        self.assertEqual(anchors[NATIVE], (ANCHOR, True))
        self.assertEqual(anchors[FIXTURE], (ANCHOR, True))
        self.assertEqual(anchors[CONTRACT], (QT_ANCHOR, False))
        self.assertEqual(anchors[PROVIDER], (QT_ANCHOR, False))
        self.assertEqual(anchors[HOST], (HOST_ANCHOR, False))
        self.assertEqual(anchors[GIT], (GIT_ANCHOR, False))
        self.assertEqual(anchors[DIFF], (HOST_ANCHOR, False))
        for selector, anchor in zip(QT_SCALES, QT_SCALE_ANCHORS):
            self.assertEqual(anchors[selector], (anchor, False))
        for selector, anchor, arguments in PREVIEW_ROWS:
            self.assertEqual(anchors[selector], (anchor, False))
            self.assertEqual(self.module.REQUIRED_ARGUMENTS[selector], arguments)
        self.assertEqual(anchors[SQLITE], (SQLITE_ANCHOR, False))
        for selector, (anchor, wire_block) in anchors.items():
            if wire_block:
                self.assertIn(selector, WIRE_SUITES)
                self.assertEqual(anchor, ANCHOR)

    def test_a_qt_graph_without_the_wire_block_owes_the_contract_suites(
            self) -> None:
        graph = [entry for entry in DEV_GRAPH
                 if entry[0] not in (ANCHOR, NATIVE, FIXTURE)]
        self.assertEqual(self.check(graph), [])
        without_provider = [entry for entry in graph if entry[0] != PROVIDER]
        failures = self.check(without_provider)
        self.assertEqual(len(failures), 1, failures)
        self.assertIn(f"{PROVIDER}: expected 1 registration(s), found 0",
                      failures[0])
        self.assertIn(f"graph builds {QT_ANCHOR}", failures[0])

    def test_the_forbidden_test_factory_is_named_in_both_scans(self) -> None:
        for factory in ("make_native_https_anonymous_read_transport_for_test",
                        "make_native_https_anonymous_smart_transport_for_test"):
            self.assertIn(factory, self.module.FORBIDDEN_SYMBOLS)
            self.assertIn(factory, self.module.FORBIDDEN_STRINGS)
        for shipped in ("make_native_https_anonymous_read_transport",
                        "make_native_https_anonymous_smart_transport"):
            self.assertIn(shipped, self.module.KNOWN_PRODUCT_CREDENTIAL_FREE)
        self.assertIn("--network-denied", self.module.FORBIDDEN_STRINGS)

    def test_a_stripped_image_still_refuses_a_tests_only_flag(self) -> None:
        with contained_temporary_directory(prefix="bpr-rfc0017-absence-") as root:
            binary = pathlib.Path(root) / "application.exe"
            with mock.patch.object(self.module, "symbol_table", return_value=""):
                binary.write_bytes(b"MZ synthetic fixture without a test seam")
                self.assertEqual(self.module.check_release_absence(binary), [])
                binary.write_bytes(b"MZ synthetic fixture\0--network-denied\0")
                self.assertEqual(self.module.check_release_absence(binary), [
                    "string present in release binary: --network-denied"])

    def test_the_tests_enabled_graph_passes(self) -> None:
        self.assertEqual(self.check(DEV_GRAPH), [])

    def test_the_core_only_graph_passes_with_only_the_core_selector(
            self) -> None:
        self.assertEqual(self.check(CORE_ONLY_GRAPH), [])

    def test_the_core_selector_is_owed_by_every_graph(self) -> None:
        for graph in (DEV_GRAPH, CORE_ONLY_GRAPH):
            without = [entry for entry in graph if entry[0] != CORE]
            failures = self.check(without)
            self.assertEqual(len(failures), 1, failures)
            self.assertIn(f"{CORE}: expected 1 registration(s), found 0",
                          failures[0])
            self.assertIn("required of every configured graph", failures[0])

    def test_a_stray_anchored_selector_fails_the_core_only_graph(
            self) -> None:
        stray = CORE_ONLY_GRAPH + [(NATIVE, ("runner", "--suite", "rfc0017"))]
        failures = self.check(stray)
        self.assertEqual(len(failures), 1, failures)
        self.assertIn(f"{NATIVE}: expected 0 registration(s), found 1",
                      failures[0])
        self.assertIn(f"graph does not build {ANCHOR}", failures[0])

    def test_a_duplicate_registration_is_a_defect(self) -> None:
        doubled = DEV_GRAPH + [(CORE, ("core_tests",))]
        failures = self.check(doubled)
        self.assertEqual(len(failures), 1, failures)
        self.assertIn(f"{CORE}: expected 1 registration(s), found 2",
                      failures[0])

    def test_a_command_that_lost_its_suite_argument_fails(self) -> None:
        repointed = [
            (name, ("runner", "--suite", "refresh") if name == NATIVE
             else arguments)
            for name, arguments in DEV_GRAPH
        ]
        failures = self.check(repointed)
        self.assertEqual(failures, [f"{NATIVE}: command omits rfc0017"])
        repointed = [
            (name, ("anonymous_read_tests",) if name == CONTRACT
             else arguments)
            for name, arguments in DEV_GRAPH
        ]
        self.assertEqual(
            self.check(repointed), [f"{CONTRACT}: command omits --contract"])

    def test_required_insists_on_an_anchored_selector(self) -> None:
        failures = self.check(CORE_ONLY_GRAPH, required=(NATIVE,))
        self.assertEqual(len(failures), 1, failures)
        self.assertIn(f"{NATIVE}: required but not owed by this graph",
                      failures[0])
        self.assertEqual(self.check(DEV_GRAPH, required=(NATIVE,)), [])
        self.assertEqual(self.check(CORE_ONLY_GRAPH, required=(CORE,)), [])

    def test_all_insists_on_every_selector(self) -> None:
        self.assertEqual(self.check(DEV_GRAPH, require_all=True), [])
        failures = self.check(CORE_ONLY_GRAPH, require_all=True)
        anchored = sorted(
            selector for selector, (anchor, _) in
            self.module.SELECTOR_ANCHORS.items() if anchor is not None)
        self.assertEqual(len(failures), len(anchored), failures)
        for selector, failure in zip(anchored, failures):
            self.assertIn(f"{selector}: required but not owed", failure)

    def test_an_unknown_required_selector_is_refused_by_name(self) -> None:
        with self.assertRaises(self.module.InventoryFailure) as caught:
            self.check(DEV_GRAPH, required=("no_such_selector",))
        self.assertEqual(
            str(caught.exception),
            "unknown-required-selector:no_such_selector")

    def test_windows_owes_every_selector_but_the_wire_block(self) -> None:
        self.assertEqual(
            self.check([entry for entry in DEV_GRAPH
                        if entry[0] not in WIRE_SUITES],
                       platform="win32"),
            [])
        failures = self.check(DEV_GRAPH, platform="win32")
        self.assertEqual(len(failures), 2, failures)
        for selector, failure in zip(sorted(WIRE_SUITES), failures):
            self.assertIn(f"{selector}: expected 0 registration(s), found 1",
                          failure)
            self.assertIn("outside the APPLE/Linux wire block", failure)

    def test_a_selector_without_an_anchor_entry_is_a_table_defect(
            self) -> None:
        with mock.patch.dict(self.module.SELECTORS, {"added_later": 1}):
            with self.assertRaises(self.module.InventoryFailure) as caught:
                self.check(DEV_GRAPH)
        self.assertEqual(str(caught.exception),
                         "selector-anchor-table-mismatch")

    def test_build_dir_is_the_ledger_spelling_of_build_root(self) -> None:
        with mock.patch.object(
            sys, "argv",
            ["prog", "--build-dir", "x", "--required", CORE, "--all"]):
            arguments = self.module.parse_arguments()
        self.assertEqual(arguments.build_root, pathlib.Path("x"))
        self.assertEqual(arguments.required, [CORE])
        self.assertTrue(arguments.all)

    def test_main_reports_each_failure_by_name(self) -> None:
        without = [entry for entry in DEV_GRAPH if entry[0] != CORE]
        error = io.StringIO()
        with mock.patch.object(
            self.module, "registered_tests", return_value=without
        ), mock.patch.object(self.module.sys, "platform", "darwin"), \
                mock.patch.object(
                    sys, "argv", ["prog", "--build-root", "x"]), \
                contextlib.redirect_stderr(error):
            status = self.module.main()
        self.assertEqual(status, 1)
        self.assertIn(f"rfc0017-test-inventory:fail:{CORE}:", error.getvalue())


if __name__ == "__main__":
    unittest.main()
