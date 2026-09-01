from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path
from unittest import mock


SKILL_DIR = Path(__file__).resolve().parents[1]
RENDERER = SKILL_DIR / "scripts" / "render_runtime_adapters.py"
VALIDATOR = SKILL_DIR / "scripts" / "validate_portable_agents.py"
ROUTING = json.loads((SKILL_DIR / "config" / "routing.json").read_text(encoding="utf-8"))
PROJECT = json.loads((SKILL_DIR / "config" / "project.json").read_text(encoding="utf-8"))

VALIDATOR_SPEC = importlib.util.spec_from_file_location("validate_portable_agents", VALIDATOR)
if VALIDATOR_SPEC is None or VALIDATOR_SPEC.loader is None:
    raise RuntimeError(f"could not load portable-agent validator at {VALIDATOR}")
VALIDATOR_MODULE = importlib.util.module_from_spec(VALIDATOR_SPEC)
VALIDATOR_SPEC.loader.exec_module(VALIDATOR_MODULE)


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

    def expected_banner(self, role: str, provider: str) -> str:
        route = self.expected_route(role, provider)
        effort = route["effort"] if route["effort"] is not None else "none (unsupported)"
        return f"Role: {role} | Model: {route['model']} | Effort: {effort}"

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

            worker_route = self.expected_route("rfc-worker", "codex")
            worker = tomllib.loads((root / ".codex/agents/rfc-worker.toml").read_text())
            self.assertEqual(worker["model"], worker_route["model"])
            self.assertEqual(worker["model_reasoning_effort"], "xhigh")
            self.assertEqual(worker["sandbox_mode"], "workspace-write")
            self.assertTrue(worker["agents"]["enabled"])

            verifier_route = self.expected_route("rfc-verifier", "codex")
            verifier = tomllib.loads((root / ".codex/agents/rfc-verifier.toml").read_text())
            self.assertEqual(verifier["model"], verifier_route["model"])
            self.assertEqual(verifier["sandbox_mode"], "workspace-write")
            self.assertIn("Do not edit source", verifier["developer_instructions"])
            self.assertTrue(verifier["agents"]["enabled"])

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

            for role in ROUTING["assignments"]:
                codex = tomllib.loads((root / f".codex/agents/{role}.toml").read_text())
                self.assertTrue(
                    codex["developer_instructions"].startswith(
                        "Before any analysis or tool call"
                    )
                )
                self.assertIn(self.expected_banner(role, "codex"), codex["developer_instructions"])

                claude = (root / f".claude/agents/{role}.md").read_text()
                body = claude.split(f"# {role}\n\n", 1)[1]
                self.assertTrue(body.startswith("- Before any analysis or tool call"))
                self.assertIn(self.expected_banner(role, "claude"), body)

            self.assertIn("rfc_utility_<purpose>", scout["developer_instructions"])
            self.assertNotIn("rfc_utility_<purpose>", utility["developer_instructions"])

            claude_utility_route = self.expected_route("rfc-utility", "claude")
            claude_utility = (root / ".claude/agents/rfc-utility.md").read_text()
            self.assertIn(f'model: "{claude_utility_route["model"]}"', claude_utility)
            self.assertIn("permissionMode: plan", claude_utility)
            self.assertNotIn("tools: Agent", claude_utility)
            self.assertNotIn("\neffort:", claude_utility)

            claude_scout = (root / ".claude/agents/rfc-scout.md").read_text()
            self.assertIn("tools: Read, Grep, Glob, Bash, Agent(rfc-utility)", claude_scout)

            claude_verifier = (root / ".claude/agents/rfc-verifier.md").read_text()
            self.assertIn("permissionMode: acceptEdits", claude_verifier)
            self.assertIn("Agent(rfc-utility)", claude_verifier)
            self.assertNotIn("tools: Read, Grep, Glob, Bash, Edit", claude_verifier)

            claude_critical_route = self.expected_route("rfc-critical-reviewer", "claude")
            claude_critical = (root / ".claude/agents/rfc-critical-reviewer.md").read_text()
            self.assertIn(f'model: "{claude_critical_route["model"]}"', claude_critical)
            self.assertIn("effort: max", claude_critical)

            shim = (root / ".claude/skills/implement-rfc/SKILL.md").read_text()
            self.assertIn(".agents/skills/implement-rfc/SKILL.md", shim)

            manifest = json.loads(
                (root / ".agents/skills/implement-rfc/generated/runtime-adapters.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(manifest["schema"], "implement-rfc.runtime-adapters.v1")
            self.assertEqual(manifest["project"], PROJECT["project"])
            self.assertEqual(manifest["assignments"], ROUTING["assignments"])

            portable = json.loads(
                (root / ".agents/skills/implement-rfc/generated/portable-manifest.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(portable["schema"], "implement-rfc.portable-manifest.v1")
            portable_paths = {entry["path"] for entry in portable["files"]}
            self.assertIn("SKILL.md", portable_paths)
            self.assertIn("scripts/render_runtime_adapters.py", portable_paths)
            self.assertIn("references/pack-lifecycle.md", portable_paths)
            self.assertNotIn("config/project.json", portable_paths)
            self.assertNotIn("references/project.md", portable_paths)
            self.assertFalse(any(path.startswith("generated/") for path in portable_paths))

            claude_worker = (root / ".claude/agents/rfc-worker.md").read_text()
            for addendum in PROJECT["role_addenda"].get("rfc-worker", []):
                self.assertIn(addendum, claude_worker)
            attended = PROJECT["attended_review_skill"]
            if attended is None:
                self.assertIn("record the pending checkpoint and its exact ask", claude_worker)
                self.assertNotIn("{attended_review_skill}", claude_worker)
            else:
                self.assertIn(f".agents/skills/{attended}/SKILL.md", claude_worker)
            self.assertNotIn("{attended_review_skill}", claude_worker)

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

    def test_check_detects_portable_manifest_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            rendered = self.run_renderer(root)
            self.assertEqual(rendered.returncode, 0, rendered.stderr)
            path = root / ".agents/skills/implement-rfc/generated/portable-manifest.json"
            path.write_text(path.read_text().replace('"files"', '"file_list"'), encoding="utf-8")
            check = self.run_renderer(root, "--check")
            self.assertEqual(check.returncode, 1)
            self.assertIn("portable-manifest.json", check.stderr)

    def test_compare_reports_portable_differences(self) -> None:
        ours = {"SKILL.md": "a", "scripts/x.py": "b", "README.md": "c"}
        theirs = {"SKILL.md": "a", "scripts/x.py": "changed", "tests/t.py": "d"}
        findings = VALIDATOR_MODULE.compare_manifests(ours, theirs)
        self.assertEqual(
            findings,
            ["only here: README.md", "differs: scripts/x.py", "only there: tests/t.py"],
        )
        self.assertEqual(VALIDATOR_MODULE.compare_manifests(ours, dict(ours)), [])

    def test_portable_skill_and_role_sources_do_not_pin_vendor_models(self) -> None:
        skill = (SKILL_DIR / "SKILL.md").read_text(encoding="utf-8")
        roles = (SKILL_DIR / "config" / "roles.json").read_text(encoding="utf-8")
        for source in (skill, roles):
            self.assertNotIn("gpt-5.", source)
            self.assertNotIn("claude-opus", source)
            self.assertNotIn("claude-sonnet", source)
            self.assertNotIn("claude-haiku", source)

    def test_portable_sources_name_no_project(self) -> None:
        """The portable set must not mention this project or the one it came from."""
        renderer_spec = importlib.util.spec_from_file_location("render_under_test", RENDERER)
        assert renderer_spec is not None and renderer_spec.loader is not None
        renderer = importlib.util.module_from_spec(renderer_spec)
        renderer_spec.loader.exec_module(renderer)
        # The skill was first extracted from a repository whose name is spelled
        # here in two halves so this file itself stays out of the forbidden set.
        forbidden = ("ai" "to", PROJECT["project"], PROJECT["display_name"], PROJECT["session_prefix"] + "-")
        for path in renderer.portable_files(SKILL_DIR):
            text = path.read_text(encoding="utf-8", errors="replace").lower()
            for token in forbidden:
                self.assertNotIn(
                    token.lower(), text, f"{path.relative_to(SKILL_DIR)} names {token!r}"
                )

    def test_portable_policy_requires_role_names_and_startup_banners(self) -> None:
        skill = (SKILL_DIR / "SKILL.md").read_text(encoding="utf-8")
        delegation = (SKILL_DIR / "references" / "delegated-execution.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("rfc_scout_backend_selection", delegation)
        self.assertIn("rfc_utility_cache_inventory", delegation)
        self.assertIn("Role: <role> | Model: <model> | Effort: <effort>", delegation)

    def test_validator_executes_absolute_paths_but_displays_stable_paths(self) -> None:
        repo_root_argument = "/explicit/repo-root"
        original_cwd = Path.cwd()

        with tempfile.TemporaryDirectory() as temp:
            os.chdir(temp)
            try:
                self.assertFalse((Path.cwd() / ".git").exists())
                stdout = io.StringIO()
                with (
                    mock.patch.object(
                        VALIDATOR_MODULE.sys,
                        "argv",
                        [
                            "validate_portable_agents.py",
                            "--repo-root",
                            repo_root_argument,
                        ],
                    ),
                    mock.patch.object(
                        VALIDATOR_MODULE.subprocess,
                        "call",
                        side_effect=[0, 17],
                    ) as call,
                    contextlib.redirect_stdout(stdout),
                ):
                    code = VALIDATOR_MODULE.main()
            finally:
                os.chdir(original_cwd)

        self.assertEqual(code, 17)
        self.assertEqual(call.call_count, 2)

        test_command = call.call_args_list[0].args[0]
        self.assertEqual(
            test_command,
            [
                sys.executable,
                "-m",
                "unittest",
                "discover",
                "-s",
                str(SKILL_DIR / "tests"),
                "-p",
                "test_*.py",
            ],
        )
        self.assertTrue(Path(test_command[5]).is_absolute())

        renderer_command = call.call_args_list[1].args[0]
        self.assertEqual(
            renderer_command,
            [
                sys.executable,
                str(RENDERER),
                "--check",
                "--repo-root",
                repo_root_argument,
            ],
        )
        self.assertTrue(Path(renderer_command[1]).is_absolute())
        self.assertEqual(renderer_command[-2:], ["--repo-root", repo_root_argument])

        self.assertEqual(
            stdout.getvalue(),
            "\n".join(
                [
                    "+ "
                    f"{sys.executable} -m unittest discover -s "
                    ".agents/skills/implement-rfc/tests -p test_*.py",
                    "+ "
                    f"{sys.executable} "
                    ".agents/skills/implement-rfc/scripts/render_runtime_adapters.py "
                    f"--check --repo-root {repo_root_argument}",
                    "",
                ]
            ),
        )
        self.assertNotIn(str(SKILL_DIR), stdout.getvalue())


if __name__ == "__main__":
    unittest.main()
