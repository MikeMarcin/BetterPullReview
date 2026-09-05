from __future__ import annotations

import copy
import importlib.util
import json
import unittest
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parents[1]
SCRIPT = SKILL_DIR / "scripts" / "project_config.py"
SPEC = importlib.util.spec_from_file_location("project_config_under_test", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class ProjectConfigTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = module.load()

    def test_checked_in_project_config_validates(self) -> None:
        self.assertEqual(self.config["schema"], module.SCHEMA)
        self.assertEqual(module.repo_root(), SKILL_DIR.parents[2])

    def test_schema_and_loader_agree_on_required_keys(self) -> None:
        schema = json.loads((SKILL_DIR / "schemas" / "project.schema.json").read_text(encoding="utf-8"))
        self.assertEqual(set(schema["required"]), set(module.REQUIRED_KEYS))
        self.assertEqual(set(schema["properties"]), set(module.REQUIRED_KEYS) | set(module.OPTIONAL_KEYS))

    def test_lane_order_and_activity(self) -> None:
        lanes = module.lane_names(self.config)
        self.assertEqual(lanes[0], self.config["lanes"]["backlog"])
        self.assertEqual(lanes[-1], self.config["lanes"]["done"])
        self.assertEqual(module.active_lanes(self.config), lanes[:-1])
        self.assertEqual(set(self.config["lane_statuses"]), set(lanes))

    def test_station_name_omits_platform_for_platform_bound_workstreams(self) -> None:
        config = copy.deepcopy(self.config)
        config["workstreams"] = {
            "main": {"branch": "main"},
            "win": {"branch": "windows-lane", "platform": "win"},
        }
        prefix = config["session_prefix"]
        self.assertEqual(module.station_name(config, "main", "mac"), f"{prefix}-main-mac")
        self.assertEqual(module.station_name(config, "win", "win", "jaegaw"), f"{prefix}-win-jaegaw")
        pattern = module.session_name_pattern(config)
        self.assertTrue(pattern.fullmatch(f"{prefix}-main-mac"))
        self.assertFalse(pattern.fullmatch("other-main-mac"))

    def test_closeout_matcher_supports_both_rules(self) -> None:
        by_name = module.closeout_card_matcher(
            dict(self.config, closeout_card={"filename_pattern": r"^X-\d{4}-close\.md$"})
        )
        self.assertTrue(by_name(Path("X-0001-close.md"), {}))
        self.assertFalse(by_name(Path("X-0001-other.md"), {}))
        by_field = module.closeout_card_matcher(
            dict(self.config, closeout_card={"front_matter": {"closeout": "true"}})
        )
        self.assertTrue(by_field(Path("anything.md"), {"closeout": "true"}))
        self.assertTrue(by_field(Path("anything.md"), {"closeout": '"True"'}))
        self.assertFalse(by_field(Path("anything.md"), {}))

    def test_rejects_unknown_key_missing_key_and_bad_lane(self) -> None:
        config = copy.deepcopy(self.config)
        config["extra"] = 1
        with self.assertRaises(module.ProjectConfigError):
            module.validate(config)
        config = copy.deepcopy(self.config)
        del config["lanes"]
        with self.assertRaises(module.ProjectConfigError):
            module.validate(config)
        config = copy.deepcopy(self.config)
        config["lanes"]["done"] = "Done Lane"
        with self.assertRaises(module.ProjectConfigError):
            module.validate(config)
        config = copy.deepcopy(self.config)
        config["closeout_card"] = {"filename_pattern": "x", "front_matter": {"closeout": "true"}}
        with self.assertRaises(module.ProjectConfigError):
            module.validate(config)


if __name__ == "__main__":
    unittest.main()
