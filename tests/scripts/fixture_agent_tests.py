#!/usr/bin/env python3
"""[[BPR-2177]]. The fixture agent's answers, streams and scripted failures.

Runs the built fixture agent as `claude` and as `codex` on a request of each
kind the window sends and checks it answers in kind, in more than one step,
and fails or hits a usage limit when asked to. No network, no cost.

    fixture_agent_tests.py FIXTURE_AGENT_FOLDER
"""

import json
import os
import subprocess
import sys
from pathlib import Path


def run(program: Path, arguments: list[str], text: str, environment: dict[str, str] | None = None) -> subprocess.CompletedProcess:
    env = dict(os.environ)
    env.update(environment or {})
    return subprocess.run([str(program), *arguments], input=text, capture_output=True, text=True, timeout=30, env=env, check=False)


def frames(output: str) -> list[dict]:
    return [json.loads(line) for line in output.splitlines() if line.strip()]


def expect(condition: bool, message: str) -> None:
    if not condition:
        print(f"FAILED: {message}")
        sys.exit(1)


def claude_answer(output: str) -> str:
    results = [frame for frame in frames(output) if frame.get("type") == "result"]
    expect(len(results) == 1, f"one result frame:\n{output}")
    return results[0]["result"]


def main() -> int:
    folder = Path(sys.argv[1])
    claude = folder / "claude"
    codex = folder / "codex"
    expect(claude.is_file() and codex.is_file(), "the fixture agent is built as claude and codex")
    expect("Claude Code" in run(claude, ["--version"], "").stdout, "claude --version")
    expect(json.loads(run(claude, ["auth", "status", "--json"], "").stdout)["loggedIn"], "claude auth status")
    expect("fixture" in run(codex, ["--version"], "").stdout, "codex --version")
    expect(run(codex, ["login", "status"], "").returncode == 0, "codex login status")

    user = lambda text: json.dumps({"type": "user", "message": {"role": "user", "content": [{"type": "text", "text": text}]}}) + "\n"
    cases = {
        "returning.operation\tproposed_fix": "**Partly**",
        "returning.operation\tlatest_upload": "# What changed since your review",
        "explain.path\tsrc/engine/renderer.cpp": "## What changed",
        "What could this change break?": "## Rendering",
        "bpr.tour-draft.v1": '"schema":"bpr.tour-draft.v1"',
        "bpr.review-preparation": '"kind":"rewrite"',
        "Write a commit message": "Clamp the resize",
        "Review this pull request; answer with a ```bpr-findings block": "```bpr-findings",
        "What is this? (findings go in a ```bpr-findings block)": "Fixture answer",
        "What does this change do?": "Fixture answer",
    }
    for request, wanted in cases.items():
        done = run(claude, ["-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose"], user(request))
        expect(done.returncode == 0, f"claude answers {request!r}: {done.stderr}")
        steps = [frame["type"] for frame in frames(done.stdout)]
        expect(steps[0] == "system" and steps.count("assistant") >= 2 and steps[-1] == "result", f"claude streams in steps: {steps}")
        expect(wanted in claude_answer(done.stdout), f"claude answers {request!r} with {wanted!r}")
        answer = run(codex, ["exec", "--json"], request)
        expect(answer.returncode == 0, f"codex answers {request!r}")
        messages = [frame["item"]["text"] for frame in frames(answer.stdout) if frame.get("type") == "item.completed" and frame["item"]["type"] == "agent_message"]
        expect(len(messages) == 1 and wanted in messages[0], f"codex answers {request!r} with {wanted!r}")
    findings = claude_answer(run(claude, ["-p"], user("Review this pull request; answer with a ```bpr-findings block")).stdout)
    block = findings.split("```bpr-findings\n", 1)[1].split("\n```", 1)[0]
    expect(len(json.loads(block)["findings"]) == 2, "the findings block is JSON with two findings")

    crashed = run(claude, ["-p"], user("FIXTURE_CRASH"))
    expect(crashed.returncode != 0 and "scripted" in crashed.stderr, "a scripted crash fails")
    limited = run(claude, ["-p"], user("anything"), {"BPR_FIXTURE_AGENT_FAIL": "usage-limit"})
    expect(limited.returncode != 0 and "usage limit" in claude_answer(limited.stdout), "a scripted usage limit says so")
    limited = run(codex, ["exec"], "FIXTURE_USAGE_LIMIT")
    expect(limited.returncode != 0 and "usage limit" in limited.stdout, "codex's usage limit says so")
    print("fixture-agent-answers:pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
