"""Compare the recorded native Settings inventories, not platform wrapper counts.

The e93d9215 macOS capture binds the Windows/Linux inventory baseline. Current
macOS dev/release captures also pin the later General and CLI-entry changes.
This is evidence comparison, not a substitute for running a native capture.
"""

from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2] / "docs" / "verification"
MAC = ROOT / "BPR-0593-macos-settings-inventory"
ROLE_NAMES = {
    "Button": "button", "push button": "button", "AXButton": "button",
    "CheckBox": "check", "check box": "check", "AXCheckBox": "check",
    "ComboBox": "combo", "combo box": "combo", "AXComboBox": "combo", "AXMenuButton": "combo",
    "Spinner": "spin", "spin box": "spin", "AXIncrementor": "spin",
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

    def test_current_mac_dev_and_release_are_identical(self) -> None:
        self.assertEqual(self.dev["pages"], self.current["pages"])
        self.assertEqual(self.dev["native_checks"], self.current["native_checks"])
        self.assertEqual(len(self.current["native_checks"]), 11)

    def test_categories_match_on_every_capture(self) -> None:
        self.assertEqual(len(self.current["categories"]), 7)
        for capture in (self.dev, self.baseline, self.windows, self.linux):
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


if __name__ == "__main__":
    unittest.main()
