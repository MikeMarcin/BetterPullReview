"""Compare the recorded native Settings inventories, not platform wrapper counts.

The e93d9215 macOS capture binds the Windows/Linux inventory baseline. Current
macOS dev/release captures also pin the later General and CLI-entry changes.
The 2d3bdbdf continuation compares the refreshed Mac and Linux inventories,
without upgrading Linux's inventory-only course to native lifetime acceptance.
The b651554f BPR-0975 captures add the Linux focus controls, the failed full
course and its labelled refused-activation diagnosis, whose pages are compared
again here. This is evidence comparison, not a substitute for running a native
capture.
"""

from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2] / "docs" / "verification"
MAC = ROOT / "BPR-0593-macos-settings-inventory"
CONTINUATION = ROOT / "BPR-0593-native-settings-continuation"
LINUX_FOCUS = ROOT / "BPR-0975-linux-native-settings"
ROLE_NAMES = {
    "Button": "button", "push button": "button", "button": "button", "AXButton": "button",
    "CheckBox": "check", "check box": "check", "AXCheckBox": "check",
    "ComboBox": "combo", "combo box": "combo", "AXComboBox": "combo", "AXMenuButton": "combo",
    "Spinner": "spin", "spin box": "spin", "spin button": "spin", "AXIncrementor": "spin",
    "Edit": "edit", "text": "edit", "AXTextField": "edit", "AXTextArea": "edit",
}
# Qt's native AX/AT-SPI bridges use current choices for some combo names.
# These are explicit observed aliases; unrecognized names remain different.
COMBO_NAMES = {
    "Role: any": "Queue role filter",
    "Observer: any reason": "Queue observer reason filter",
    "Bucket: any": "Queue attention bucket filter",
    "Repository: any": "Queue repository filter",
    "State: any": "Queue quiet or current filter",
    "Sort: core priority": "Queue sort order",
    "repository": "Observer rule kind",
    ".AppleSystemUIFont": "Interface font family",
    "Sans Serif": "Interface font family",
    "Menlo": "Code font family",
    "monospace": "Code font family",
    "System": "Dif motion",
    "No release access account selected": "Release access account",
}
TOGGLE_STATES = {
    "": "", "On": "On", "Off": "Off",
    "toggle=On;": "On", "toggle=Off;": "Off",
    "checked": "On", "unchecked": "Off",
}


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def controls(page: dict) -> list[tuple]:
    result = []
    for node in page["nodes"]:
        role = ROLE_NAMES.get(node.get("type", node.get("role")))
        if role is None:
            continue  # Layout, headings, scroll bars and read-only labels.
        name = node["name"]
        if role == "combo":
            name = COMBO_NAMES.get(name, name)
        result.append((role, name, node["enabled"], TOGGLE_STATES[node["state"]]))
    return result


class SettingsInventoryComparison(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.current = load(MAC / "macos-app-release.json")
        cls.dev = load(MAC / "macos-dev.json")
        cls.baseline = load(MAC / "macos-app-release-e93d9215.json")
        cls.windows = load(ROOT / "BPR-0730-windows-settings-inventory/windows-app-release.json")
        cls.linux = load(ROOT / "BPR-0731-linux-settings-inventory/linux-app-release.json")
        cls.continued_mac = load(CONTINUATION / "macos-app-release-2d3bdbdf.json")
        cls.continued_linux = load(CONTINUATION / "linux-native-inventory-2d3bdbdf.json")
        cls.focus_control = load(LINUX_FOCUS / "linux-focus-control-b651554f.json")
        cls.full_course = load(LINUX_FOCUS / "linux-full-course-b651554f.json")
        cls.bounded = load(LINUX_FOCUS / "linux-bounded-course-b651554f.json")

    def test_current_mac_dev_and_release_are_identical(self) -> None:
        self.assertEqual(self.dev["pages"], self.current["pages"])
        self.assertEqual(self.dev["native_checks"], self.current["native_checks"])
        self.assertEqual(len(self.current["native_checks"]), 11)

    def test_categories_match_on_every_capture(self) -> None:
        self.assertEqual(len(self.current["categories"]), 7)
        for capture in (self.dev, self.baseline, self.windows, self.linux, self.continued_mac, self.continued_linux,
                        self.bounded):
            self.assertEqual(self.current["categories"], capture["categories"])
            self.assertEqual(capture["categories"], [page["category"] for page in capture["pages"]])

    def test_matching_baseline_has_the_same_controls_on_all_platforms(self) -> None:
        for captured in (self.windows, self.linux):
            for expected, observed in zip(self.baseline["pages"], captured["pages"]):
                with self.subTest(platform=captured["build"], page=expected["category"]):
                    self.assertEqual(controls(expected), controls(observed))

    def test_five_unmodified_pages_still_match(self) -> None:
        for expected, observed in zip(self.baseline["pages"][2:], self.current["pages"][2:]):
            with self.subTest(page=expected["category"]):
                self.assertEqual(controls(expected), controls(observed))

    def test_later_owned_deltas_are_explicit(self) -> None:
        self.assertEqual(controls(self.baseline["pages"][0]), [])
        self.assertEqual(controls(self.current["pages"][0]), [
            ("check", "Automatically check for updates", False, "On"),
            ("combo", "Release access account", False, ""),
            *[("button", name, False, "") for name in (
                "Check for Updates…", "Download Update", "Later", "Skip This Version",
                "Cancel Download", "Install and Restart")],
            ("button", "View Release", True, ""),
        ])
        self.assertEqual(controls(self.current["pages"][1]), [
            ("button", "Sign in to github.com", True, ""),
            ("check", "Show details for github.com", True, "Off"),
            ("button", "Use GitHub CLI", True, ""),
            ("button", "Add enterprise server", True, ""),
        ])

    def test_missing_or_changed_controls_are_not_normalized_away(self) -> None:
        original = self.current["pages"][2]
        changed = copy.deepcopy(original)
        control = next(node for node in changed["nodes"] if node.get("type") == "AXButton")
        control["enabled"] = not control["enabled"]
        self.assertNotEqual(controls(original), controls(changed))
        changed["nodes"].remove(control)
        self.assertNotEqual(controls(original), controls(changed))

    def test_continued_inventories_differ_only_by_cli_capability(self) -> None:
        for expected, observed, count in zip(self.continued_mac["pages"], self.continued_linux["pages"],
                                             (9, 4, 35, 85, 4, 3, 11)):
            with self.subTest(page=expected["category"]):
                self.assertEqual(len(controls(expected)), count)
                expected_controls = controls(expected)
                if expected["category"] == "Accounts & hosts":
                    # Linux does not offer native CLI discovery. Keep this one
                    # observed capability explicit; the generic normalizer
                    # never deletes a control.
                    cli = ("button", "Use GitHub CLI", True, "")
                    self.assertEqual(expected_controls.count(cli), 1)
                    expected_controls.remove(cli)
                self.assertEqual(expected_controls, controls(observed))

    def test_six_pages_are_unchanged_since_previous_mac_capture(self) -> None:
        for expected, observed in zip(self.current["pages"][:6], self.continued_mac["pages"][:6]):
            with self.subTest(page=expected["category"]):
                self.assertEqual(controls(expected), controls(observed))

    def test_release_foundation_storage_additions_are_explicit(self) -> None:
        self.assertEqual(controls(self.continued_mac["pages"][6]), [
            *controls(self.current["pages"][6]),
            ("button", "Preview diagnostics", True, ""),
            ("button", "Export preview…", False, ""),
            ("button", "Prepare diagnostic cleanup…", True, ""),
            ("button", "Clear prepared diagnostics…", False, ""),
            ("button", "Prepare profile purge…", True, ""),
            ("button", "Purge prepared profile data…", False, ""),
            ("button", "Cancel maintenance", False, ""),
        ])

    def test_inventory_only_capture_claims_no_lifetime_pass(self) -> None:
        self.assertEqual(len(self.continued_mac["native_checks"]), 11)
        self.assertEqual(self.continued_linux["course"], "inventory-only")
        self.assertEqual(self.continued_linux["native_checks"], [
            "native-menu-opens-profile-labelled-settings",
            "seven-categories-with-current-page-heading",
            "native-window-close-process-terminated",
        ])

    def test_new_atspi_button_alias_does_not_hide_missing_controls(self) -> None:
        original = self.continued_linux["pages"][6]
        changed = copy.deepcopy(original)
        control = next(node for node in changed["nodes"] if node["name"] == "Preview diagnostics")
        self.assertEqual(control["role"], "button")
        control["enabled"] = False
        self.assertNotEqual(controls(original), controls(changed))
        changed["nodes"].remove(control)
        self.assertNotEqual(controls(original), controls(changed))


    def test_bpr0975_controls_attribute_the_x11_revert_and_focus_natively(self) -> None:
        self.assertEqual(self.focus_control["window_manager"]["name"], "Weston WM")
        self.assertEqual(self.focus_control["focus_surface"], "wslg-rail")
        revert = ["FocusOut(B)", "FocusIn(A)", "FocusOut(A)", "FocusIn(B)"]
        for name in ("qt", "xlib"):
            with self.subTest(control=name):
                control = self.focus_control["controls"][name]
                # X11 alone: focus reached A and was put back; EWMH activation was ignored.
                self.assertFalse(control["x11_only"]["passed"])
                self.assertEqual(control["x11_only"]["set_input_focus"], dict(events=revert, focus="B"))
                self.assertEqual(control["x11_only"]["net_active_window"], dict(events=[], focus="B"))
                self.assertTrue(control["two_window"]["passed"])
                self.assertTrue(control["post_close"]["passed"])
                self.assertEqual(control["exit"], 0)
                self.assertTrue(all(a["surface"] == "wslg-rail" for a in control["focus_activations"]))

    def test_bpr0975_captures_bind_one_clean_tests_disabled_candidate(self) -> None:
        for capture in (self.full_course, self.bounded):
            with self.subTest(course=capture["course"]):
                build = capture["context"]["build"]
                self.assertEqual(capture["revision"], build["revision"])
                self.assertEqual(capture["executable_sha256"], capture["context"]["executable"]["sha256"])
                self.assertEqual(capture["observed"]["image"], capture["context"]["executable"]["path"])
                self.assertTrue(build["clean"])
                self.assertEqual((build["cache"]["BUILD_TESTING"], build["cache"]["CMAKE_BUILD_TYPE"]), ("OFF", "Release"))
                self.assertEqual(build["first_party_error_demotions"], [])
                self.assertIn("-Werror", build["main_compile_command"].split())
                self.assertEqual(capture["observed"]["window_manager"], "Weston WM")
                self.assertFalse(capture["context"]["environment"]["home_overridden"])
                for root in capture["context"]["environment"]["owned_roots"].values():
                    self.assertEqual(root["mode"], "0o700")
        self.assertEqual(self.full_course["revision"], self.bounded["revision"])
        self.assertEqual(self.full_course["executable_sha256"], self.bounded["executable_sha256"])

    def test_bpr0975_full_course_fails_at_the_refused_ctrl_comma_activation(self) -> None:
        self.assertEqual((self.full_course["course"], self.full_course["status"]), ("full", "failed"))
        self.assertEqual(self.full_course["native_checks"], ["native-menu-opens-profile-labelled-settings"])
        self.assertIn("Ctrl-comma did not reuse/focus Settings", self.full_course["failure"])
        self.assertIn("'settings_reused': True", self.full_course["failure"])
        self.assertEqual(self.full_course["pages"], [])

    def test_bpr0975_refused_activation_diagnosis_is_not_a_lifetime_pass(self) -> None:
        self.assertEqual((self.bounded["course"], self.bounded["status"]), ("full-refused-activation-bounded", "passed"))
        self.assertEqual(self.bounded["native_checks"], [
            "native-menu-opens-profile-labelled-settings",
            "ctrl-comma-modeless-singleton-activation-refused-by-window-manager",
            "escape-and-shortcut-reopen",
            "same-profile-sibling-menu-shortcut-and-retirement",
            "seven-categories-with-current-page-heading",
            "native-quit-process-terminated",
        ])
        self.assertNotIn("ctrl-comma-modeless-singleton", self.bounded["native_checks"])
        refused = self.bounded["observed"]["refused_activations"]
        self.assertTrue(refused)
        self.assertTrue(all(event["requested"] != event["focus"] for event in refused))

    def test_bpr0975_inventory_is_unchanged_since_2d3bdbdf(self) -> None:
        # Relevant inputs changed, so the comparison is rerun on the new capture.
        for expected, observed in zip(self.continued_linux["pages"], self.bounded["pages"]):
            with self.subTest(page=expected["category"]):
                self.assertEqual(expected["nodes"], observed["nodes"])
        for expected, observed in zip(self.continued_mac["pages"], self.bounded["pages"]):
            with self.subTest(page=expected["category"]):
                expected_controls = controls(expected)
                if expected["category"] == "Accounts & hosts":
                    expected_controls.remove(("button", "Use GitHub CLI", True, ""))
                self.assertEqual(expected_controls, controls(observed))


if __name__ == "__main__":
    unittest.main()
