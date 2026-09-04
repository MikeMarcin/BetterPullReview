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
        # Approved role assignments are an independent expectation, not derived
        # from the routing file whose accidental edits this test should catch.
        expected = {
            "rfc-utility": ("gpt-5.6-luna", "xhigh", "claude-haiku-4-5", None, "read-only", False),
            "rfc-test-runner": ("gpt-5.6-luna", "xhigh", "claude-haiku-4-5", None, "build-write", False),
            "rfc-scout": ("gpt-5.6-sol", "low", "claude-opus-5", "low", "read-only", True),
            "rfc-worker-routine": ("gpt-5.6-sol", "high", "claude-opus-5", "high", "workspace-write", True),
            "rfc-worker": ("gpt-6-astra", "high", "claude-fable-5-1", "high", "workspace-write", True),
            "rfc-reviewer": ("gpt-6-astra", "high", "claude-fable-5-1", "high", "read-only", True),
            "rfc-critical-reviewer": ("gpt-6-astra", "xhigh", "claude-fable-5-1", "xhigh", "read-only", True),
        }
        self.assertEqual(set(ROUTING["assignments"]), set(expected))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            first = self.run_renderer(root)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertIn("rendered 17 files", first.stdout)

            for role, (model, effort, claude_model, claude_effort, mutation, nested) in expected.items():
                with self.subTest(role=role):
                    codex = tomllib.loads((root / f".codex/agents/{role}.toml").read_text())
                    self.assertEqual(codex["name"], role)
                    self.assertEqual(codex["model"], model)
                    self.assertEqual(codex["model_reasoning_effort"], effort)
                    sandbox = "read-only" if mutation == "read-only" else "workspace-write"
                    self.assertEqual(codex["sandbox_mode"], sandbox)
                    self.assertEqual(codex["agents"]["enabled"], nested)
                    instructions = codex["developer_instructions"]
                    self.assertTrue(instructions.startswith("Before any analysis or tool call"))
                    self.assertIn(self.expected_banner(role, "codex"), instructions)
                    self.assertEqual("rfc_utility_<purpose>" in instructions, nested)

                    claude = (root / f".claude/agents/{role}.md").read_text()
                    frontmatter = claude.split("---", 2)[1].strip().splitlines()
                    header = dict(line.split(": ", 1) for line in frontmatter)
                    self.assertEqual(header["name"], role)
                    self.assertEqual(json.loads(header["model"]), claude_model)
                    self.assertEqual(header.get("effort"), claude_effort)
                    permission = "plan" if mutation == "read-only" else "acceptEdits"
                    self.assertEqual(header["permissionMode"], permission)
                    tools = set(header["tools"].split(", "))
                    expected_tools = {"Read", "Grep", "Glob", "Bash"}
                    if mutation == "workspace-write":
                        expected_tools.update({"Edit", "Write", "Skill"})
                    if nested:
                        expected_tools.add("Agent(rfc-utility)")
                    self.assertEqual(tools, expected_tools)
                    body = claude.split(f"# {role}\n\n", 1)[1]
                    self.assertTrue(body.startswith("- Before any analysis or tool call"))
                    self.assertIn(self.expected_banner(role, "claude"), body)

            for provider, extension in (("codex", "toml"), ("claude", "md")):
                for output_root in (root, SKILL_DIR.parents[2]):
                    self.assertFalse((output_root / f".{provider}/agents/rfc-verifier.{extension}").exists())

            shim = (root / ".claude/skills/implement-rfc/SKILL.md").read_text()
            self.assertIn(".agents/skills/implement-rfc/SKILL.md", shim)
            manifest_path = root / ".agents/skills/implement-rfc/generated/runtime-adapters.json"
            manifest = json.loads(manifest_path.read_text())
            self.assertEqual(manifest["schema"], "implement-rfc.runtime-adapters.v1")
            self.assertEqual(manifest["assignments"], ROUTING["assignments"])
            self.assertEqual(len(manifest["files"]), 15)
            self.assertEqual(manifest["project"], PROJECT["project"])

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

            for worker in ("rfc-worker", "rfc-worker-routine"):
                claude_worker = (root / f".claude/agents/{worker}.md").read_text()
                codex_worker = tomllib.loads((root / f".codex/agents/{worker}.toml").read_text())
                for content in (claude_worker, codex_worker["developer_instructions"]):
                    for addendum in PROJECT["role_addenda"].get("rfc-worker", []):
                        self.assertIn(addendum, content)
                    for addendum in PROJECT["role_addenda"].get(worker, []):
                        self.assertIn(addendum, content)
                    attended = PROJECT["attended_review_skill"]
                    if attended is None:
                        self.assertIn("record the pending checkpoint and its exact ask", content)
                    else:
                        self.assertIn(f".agents/skills/{attended}/SKILL.md", content)
                    self.assertNotIn("{attended_review_skill}", content)

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
                "Role: rfc-scout | Model: candidate-codex | Effort: low",
                scout["developer_instructions"],
            )
            claude_scout = (root / ".claude/agents/rfc-scout.md").read_text()
            self.assertIn('model: "claude-candidate"', claude_scout)
            self.assertIn(
                "Role: rfc-scout | Model: claude-candidate | Effort: low",
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
