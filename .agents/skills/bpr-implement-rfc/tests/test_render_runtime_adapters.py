from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path


SKILL_DIR = Path(__file__).resolve().parents[1]
RENDERER = SKILL_DIR / "scripts" / "render_runtime_adapters.py"
ROUTING = json.loads((SKILL_DIR / "config" / "routing.json").read_text(encoding="utf-8"))


class RenderRuntimeAdaptersTests(unittest.TestCase):
    def run_renderer(self, repo_root: Path, *extra: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(RENDERER), "--repo-root", str(repo_root), *extra],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    def expected_route(self, role: str, provider: str) -> dict[str, object]:
        model_class = ROUTING["assignments"][role]
        return ROUTING["providers"][provider]["classes"][model_class]

    def expected_startup_identity(self, role: str, provider: str) -> str:
        route = self.expected_route(role, provider)
        effort = route["effort"] or "not-supported"
        return f'Role: {role} | Model: {route["model"]} | Effort: {effort}'

    def test_renders_deterministic_native_agents_from_shared_assignments(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            first = self.run_renderer(root)
            self.assertEqual(first.returncode, 0, first.stderr)

            utility_route = self.expected_route("rfc-utility", "codex")
            utility = tomllib.loads((root / ".codex/agents/rfc-utility.toml").read_text())
            self.assertEqual(utility["model"], utility_route["model"])
            self.assertEqual(utility["model_reasoning_effort"], utility_route["effort"])
            self.assertEqual(utility["sandbox_mode"], "read-only")
            self.assertFalse(utility["agents"]["enabled"])

            scout_route = self.expected_route("rfc-scout", "codex")
            scout = tomllib.loads((root / ".codex/agents/rfc-scout.toml").read_text())
            self.assertEqual(scout["model"], scout_route["model"])
            self.assertEqual(scout["model_reasoning_effort"], scout_route["effort"])
            self.assertTrue(scout["agents"]["enabled"])
            self.assertEqual(scout["agents"]["max_concurrent_threads_per_session"], 2)
            self.assertIn("name beginning with rfc-utility", scout["developer_instructions"])

            worker_route = self.expected_route("rfc-worker", "codex")
            worker = tomllib.loads((root / ".codex/agents/rfc-worker.toml").read_text())
            self.assertEqual(worker["model"], worker_route["model"])
            self.assertEqual(worker["model_reasoning_effort"], "xhigh")
            self.assertEqual(worker["sandbox_mode"], "workspace-write")
            self.assertTrue(worker["agents"]["enabled"])
            self.assertEqual(worker["agents"]["max_concurrent_threads_per_session"], 2)
            self.assertIn("explicit exclusive allowed files", worker["developer_instructions"])
            self.assertIn("prescribed test/build matrices", worker["developer_instructions"])

            reviewer_route = self.expected_route("rfc-reviewer", "codex")
            reviewer = tomllib.loads((root / ".codex/agents/rfc-reviewer.toml").read_text())
            self.assertEqual(reviewer["model"], reviewer_route["model"])
            self.assertEqual(reviewer["model_reasoning_effort"], "xhigh")
            self.assertTrue(reviewer["agents"]["enabled"])

            critical_route = self.expected_route("rfc-critical-reviewer", "codex")
            critical = tomllib.loads(
                (root / ".codex/agents/rfc-critical-reviewer.toml").read_text()
            )
            self.assertEqual(critical["model"], critical_route["model"])
            self.assertEqual(critical["model_reasoning_effort"], "max")
            self.assertTrue(critical["agents"]["enabled"])

            claude_utility_route = self.expected_route("rfc-utility", "claude")
            claude_utility = (root / ".claude/agents/rfc-utility.md").read_text()
            self.assertIn(f'model: "{claude_utility_route["model"]}"', claude_utility)
            self.assertIn("permissionMode: plan", claude_utility)
            self.assertNotIn("tools: Agent", claude_utility)
            self.assertNotIn("\neffort:", claude_utility)

            claude_scout = (root / ".claude/agents/rfc-scout.md").read_text()
            self.assertIn("tools: Read, Grep, Glob, Bash, Agent", claude_scout)

            claude_worker = (root / ".claude/agents/rfc-worker.md").read_text()
            self.assertIn(
                "tools: Read, Grep, Glob, Bash, Edit, Write, Skill, Agent",
                claude_worker,
            )

            claude_critical_route = self.expected_route("rfc-critical-reviewer", "claude")
            claude_critical = (root / ".claude/agents/rfc-critical-reviewer.md").read_text()
            self.assertIn(f'model: "{claude_critical_route["model"]}"', claude_critical)
            self.assertIn("effort: max", claude_critical)

            for role in sorted(ROUTING["assignments"]):
                codex_agent = tomllib.loads(
                    (root / ".codex/agents" / f"{role}.toml").read_text()
                )
                self.assertEqual(codex_agent["name"], role)
                self.assertIn(
                    self.expected_startup_identity(role, "codex"),
                    codex_agent["developer_instructions"],
                )

                claude_agent = (root / ".claude/agents" / f"{role}.md").read_text()
                self.assertIn(f"name: {role}", claude_agent)
                self.assertIn(
                    self.expected_startup_identity(role, "claude"),
                    claude_agent,
                )

            shim = (root / ".claude/skills/bpr-implement-rfc/SKILL.md").read_text()
            self.assertIn("name: betterpullreview-implement-rfc", shim)
            self.assertIn(".agents/skills/bpr-implement-rfc/SKILL.md", shim)

            manifest = json.loads(
                (root / ".agents/skills/bpr-implement-rfc/generated/runtime-adapters.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(manifest["schema"], "betterpullreview.implement-rfc.runtime-adapters.v1")
            self.assertEqual(manifest["assignments"], ROUTING["assignments"])

            check = self.run_renderer(root, "--check")
            self.assertEqual(check.returncode, 0, check.stderr)

    def test_candidate_override_is_applied_without_changing_role_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            override = root / "routing.candidate.json"
            override.write_text(
                json.dumps(
                    {
                        "providers": {
                            "codex": {"classes": {"balanced": {"model": "candidate-codex"}}},
                            "claude": {"classes": {"balanced": {"model": "claude-candidate"}}},
                        }
                    }
                ),
                encoding="utf-8",
            )
            rendered = self.run_renderer(root, "--routing-override", str(override))
            self.assertEqual(rendered.returncode, 0, rendered.stderr)
            scout = tomllib.loads((root / ".codex/agents/rfc-scout.toml").read_text())
            self.assertEqual(scout["model"], "candidate-codex")
            self.assertIn(
                "Role: rfc-scout | Model: candidate-codex | Effort: high",
                scout["developer_instructions"],
            )
            claude_scout = (root / ".claude/agents/rfc-scout.md").read_text()
            self.assertIn('model: "claude-candidate"', claude_scout)
            self.assertIn(
                "Role: rfc-scout | Model: claude-candidate | Effort: high",
                claude_scout,
            )
            self.assertIn("Do not edit files", claude_scout)

    def test_check_detects_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            rendered = self.run_renderer(root)
            self.assertEqual(rendered.returncode, 0, rendered.stderr)
            path = root / ".codex/agents/rfc-worker.toml"
            path.write_text(path.read_text() + "# drift\n", encoding="utf-8")
            check = self.run_renderer(root, "--check")
            self.assertEqual(check.returncode, 1)
            self.assertIn("stale generated file", check.stderr)

    def test_portable_skill_and_role_sources_do_not_pin_vendor_models(self) -> None:
        skill = (SKILL_DIR / "SKILL.md").read_text(encoding="utf-8")
        roles = (SKILL_DIR / "config" / "roles.json").read_text(encoding="utf-8")
        for source in (skill, roles):
            self.assertNotIn("gpt-5.", source)
            self.assertNotIn("claude-opus", source)
            self.assertNotIn("claude-sonnet", source)
            self.assertNotIn("claude-haiku", source)
            self.assertNotIn("claude-fable", source)

    def test_bpr_sources_preserve_project_identity_and_remove_aito_paths(self) -> None:
        sources = [
            SKILL_DIR / "SKILL.md",
            SKILL_DIR / "README.md",
            SKILL_DIR / "config" / "roles.json",
            SKILL_DIR / "references" / "delegated-execution.md",
            SKILL_DIR / "references" / "model-routing.md",
            SKILL_DIR / "references" / "remote-routing.md",
            SKILL_DIR / "references" / "runtime-compatibility.md",
            SKILL_DIR / "scripts" / "render_runtime_adapters.py",
            SKILL_DIR / "scripts" / "station_lease.py",
        ]
        combined = "\n".join(path.read_text(encoding="utf-8") for path in sources)
        self.assertIn("BetterPullReview", combined)
        self.assertIn("docs/issues/NEXT_ID", combined)
        self.assertIn(".agents/skills/bpr-implement-rfc/SKILL.md", combined)
        self.assertNotIn("AITO", combined)
        self.assertNotIn("aito.implement-rfc", combined)
        self.assertNotIn(".agents/skills/implement-rfc/", combined)

    def test_worker_and_review_roles_keep_bpr_safety_boundaries(self) -> None:
        roles = json.loads(
            (SKILL_DIR / "config" / "roles.json").read_text(encoding="utf-8")
        )["roles"]
        worker = " ".join(roles["rfc-worker"]["instructions"])
        critical = " ".join(roles["rfc-critical-reviewer"]["instructions"])
        self.assertIn("core decides, the host performs, Qt presents", worker)
        self.assertIn("explicit exclusive allowed files", worker)
        self.assertIn("Edit no file outside them", worker)
        self.assertIn("every modified file", worker)
        self.assertIn("docs/issues/NEXT_ID", worker)
        self.assertIn("live GitHub or GHES pull request", worker)
        self.assertIn("stop at five workflow child lanes", worker)
        self.assertIn("only read-only rfc-utility descendants", worker)
        self.assertIn("credentials and authentication authority", critical)
        self.assertIn("Git cache and SQLite durability", critical)
        self.assertIn("only read-only rfc-utility descendants", critical)

    def test_skill_and_config_keep_runtime_headroom_and_workflow_caps(self) -> None:
        skill = (SKILL_DIR / "SKILL.md").read_text(encoding="utf-8")
        fragment = tomllib.loads(
            (SKILL_DIR / "config/codex-config.fragment.toml").read_text(
                encoding="utf-8"
            )
        )
        roles = json.loads(
            (SKILL_DIR / "config/roles.json").read_text(encoding="utf-8")
        )["roles"]

        self.assertEqual(fragment["agents"]["max_concurrent_threads_per_session"], 10)
        self.assertIn("runtime's advertised active-child concurrency cap", skill)
        self.assertIn("simultaneous, not lifetime", skill)
        self.assertIn("at most one writer", skill)
        self.assertIn("runtime-visible name that begins with its semantic role", skill)
        self.assertIn("Role: <role> | Model: <model> | Effort: <effort>", skill)
        self.assertFalse(roles["rfc-utility"]["nested_utility"])
        for role in (
            "rfc-scout",
            "rfc-worker",
            "rfc-reviewer",
            "rfc-critical-reviewer",
        ):
            self.assertTrue(roles[role]["nested_utility"])


if __name__ == "__main__":
    unittest.main()
