#!/usr/bin/env python3
"""[[BPR-2177]]. The fixture agent's answers, streams and scripted failures.

Runs the built fixture agent as `claude` and as `codex` on a request of each
kind the window sends and checks it answers in kind, in more than one step,
and fails or hits a usage limit when asked to. No network, no cost.

    fixture_agent_tests.py FIXTURE_AGENT_FOLDER
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
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


def check_steerable(claude: Path) -> None:
    """[[BPR-2153]]. `BPR_FIXTURE_AGENT_STEERABLE=1`: a kept connection that
    takes a priority-next correction after its current tool call and names
    it in the result's `user_message_uuids`, then answers a second turn."""
    env = dict(os.environ)
    env.update({"BPR_FIXTURE_AGENT_STEERABLE": "1", "BPR_FIXTURE_AGENT_SLOW_STEP_MS": "40", "BPR_FIXTURE_AGENT_PAUSE_MS": "1"})
    process = subprocess.Popen([str(claude), "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose"],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
    message = lambda text, uuid, priority=None: json.dumps({"type": "user", "message": {"role": "user", "content": [{"type": "text", "text": text}]}, "uuid": uuid, **({"priority": priority} if priority else {})}, separators=(",", ":")) + "\n"
    first, correction, second = "aaaaaaaa-0000-4000-8000-000000000001", "b5ee7e00-0000-4000-8000-000002000000", "cccccccc-0000-4000-8000-000000000003"
    watchdog = threading.Timer(60, process.kill)
    watchdog.start()
    try:
        process.stdin.write(message("Take your time", first))
        process.stdin.flush()
        seen = []
        while True:
            line = process.stdout.readline()
            expect(line != "", f"the steerable fixture answers: {seen}")
            frame = json.loads(line)
            seen.append(frame)
            if frame.get("type") == "system":
                expect(frame.get("claude_code_version") == "2.1.319", "it reports the qualified version")
            if frame.get("type") == "assistant" and len(seen) == 2:
                process.stdin.write(message("Look at b.cpp instead", correction, "next"))
                process.stdin.flush()
            if frame.get("type") == "result":
                break
        result = seen[-1]
        expect(result["user_message_uuids"] == [first, correction] and result["queued_turn_count"] == 0, f"the result names both inputs: {result}")
        expect("Took your correction after step" in result["result"] and "Look at b.cpp instead" not in result["result"], "the correction was taken after a tool call, and not repeated")
        expect(sum(1 for frame in seen if frame.get("type") == "result") == 1, "one turn, one result")
        process.stdin.write(message("What does this change do?", second))
        process.stdin.flush()
        while True:
            frame = json.loads(process.stdout.readline())
            if frame.get("type") == "result":
                expect(frame["user_message_uuids"] == [second] and frame["uuid"] != result["uuid"], "the kept process answers a second turn under a new result identity")
                break
        process.stdin.close()
        expect(process.wait(timeout=30) == 0, "it ends when its input does")
    finally:
        watchdog.cancel()
        if process.poll() is None:
            process.kill()


def check_tour_revision(claude: Path) -> None:
    """The whole tour's chat proposes what its question names: "Drop stop N"
    that removal, "Rewrite stop N" a new explanation for that stop, and both
    when one turn asks both. Any other turn changes nothing."""
    def revision(question: str) -> dict:
        request = 'bpr.tour-revise.v2\n{"this_turn":{"question":"' + question + '"}}'
        message = json.dumps({"type": "user", "message": {"role": "user", "content": [{"type": "text", "text": request}]}}) + "\n"
        done = run(claude, ["-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose"], message)
        expect(done.returncode == 0, f"the tour's chat answers {question!r}: {done.stderr}")
        return json.loads(claude_answer(done.stdout))

    dropped = revision("Drop stop 2")
    expect(dropped["remove_stops"] == [2] and dropped["stop_texts"] == [] and "taking stop 2 out of the tour" in dropped["reply"], f"Drop stop 2 proposes that removal: {dropped}")
    rewritten = revision("Rewrite stop 1")
    expect(rewritten["remove_stops"] == [] and [text["stop"] for text in rewritten["stop_texts"]] == [1] and rewritten["stop_texts"][0]["explanation"].startswith("Fixture rewrite:")
           and "a new explanation for stop 1" in rewritten["reply"], f"Rewrite stop 1 proposes a new explanation for it: {rewritten}")
    both = revision("Rewrite stop 1 and Drop stop 2")
    expect(both["remove_stops"] == [2] and [text["stop"] for text in both["stop_texts"]] == [1], f"one turn may ask both: {both}")
    plain = revision("Is the tour in a good order?")
    expect(plain["remove_stops"] == [] and plain["stop_texts"] == [] and "Nothing in the tour changes" in plain["reply"], f"any other turn changes nothing: {plain}")


def check_held_turn(claude: Path) -> None:
    """A course holds a turn open with a file: while `hold-turns` stands
    beside the agent's own executable its turn waits, and it answers once the
    file is gone. Only that copy of the agent is held."""
    with tempfile.TemporaryDirectory() as root:
        own = Path(root) / claude.name
        shutil.copy2(claude, own)
        hold = Path(root) / "hold-turns"
        hold.write_text("")
        env = dict(os.environ)
        env.update({"BPR_FIXTURE_AGENT_STEERABLE": "0", "BPR_FIXTURE_AGENT_PAUSE_MS": "1"})
        message = json.dumps({"type": "user", "message": {"role": "user", "content": [{"type": "text", "text": "What does this change do?"}]}}) + "\n"
        process = subprocess.Popen([str(own), "-p"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
        watchdog = threading.Timer(60, process.kill)
        watchdog.start()
        try:
            process.stdin.write(message)
            process.stdin.close()
            time.sleep(1.0)
            expect(process.poll() is None, "a turn waits while hold-turns stands beside the agent")
            hold.unlink()
            output = process.stdout.read()
            expect(process.wait(timeout=30) == 0 and "Fixture answer" in claude_answer(output), "and answers once the file is gone")
        finally:
            watchdog.cancel()
            if process.poll() is None:
                process.kill()
        # The built agent, with no such file beside it, is not held.
        started = time.monotonic()
        free = subprocess.run([str(claude), "-p"], input=message, capture_output=True, text=True, timeout=30, env=env, check=False)
        expect(free.returncode == 0 and time.monotonic() - started < 20.0, "an agent with no hold-turns beside it answers at once")


def main() -> int:
    folder = Path(sys.argv[1])
    # The build copies the agent as claude.exe and codex.exe on Windows.
    suffix = ".exe" if sys.platform == "win32" else ""
    claude = folder / f"claude{suffix}"
    codex = folder / f"codex{suffix}"
    expect(claude.is_file() and codex.is_file(), "the fixture agent is built as claude and codex")
    expect("Claude Code" in run(claude, ["--version"], "").stdout, "claude --version")
    expect(json.loads(run(claude, ["auth", "status", "--json"], "").stdout)["loggedIn"], "claude auth status")
    expect("fixture" in run(codex, ["--version"], "").stdout, "codex --version")
    expect(run(codex, ["login", "status"], "").returncode == 0, "codex login status")

    user = lambda text: json.dumps({"type": "user", "message": {"role": "user", "content": [{"type": "text", "text": text}]}}) + "\n"
    cases = {
        "returning.operation\tproposed_fix": "**Partly**",
        "returning.operation\taddressed_threads\nreturning.thread.0.identity\treview-thread:PRRT_cc_1\nreturning.thread.1.identity\treview-thread:PRRT_cc_2\n": "```bpr-claims\nreview-thread:PRRT_cc_1: Partly",
        # docs/design/report-templates-v1: each report keeps to its template.
        "returning.operation\taddressed_threads\nreturning.thread.0.identity\treview-thread:PRRT_cc_1\n": "## Partly\n\n### ",
        "returning.operation\tlatest_upload": "# What changed since your review",
        "explain.path\tsrc/engine/renderer.cpp": "## How it works\n\n### resize()",
        "What could this change break?": "## Likely\n\n### A packaged build starts with no shaders",
        "bpr.tour-draft.v2": '"schema":"bpr.tour-draft.v2"',
        "bpr.review-preparation": '"kind":"rewrite"',
        "Write a commit message": "Clamp the resize",
        "Review this pull request; answer with a ```bpr-findings block": "```bpr-findings",
        "Review this pull request (findings in a ```bpr-findings block)": "## Must fix\n\n### issue (blocking): A zero height still asserts",
        "What is this? (findings go in a ```bpr-findings block)": "Fixture answer",
        "What does this change do?": "Fixture answer",
        # [[BPR-2281]]. Draft Description's material: a description in three
        # parts.
        "Pull request description draft (bpr.description-draft.v1)\n\nTitle: Clamp": "## Summary\n\nEach view now clears and draws its own scene",
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
    # [[BPR-2281]]. It is a description, not a comment: its three parts, no
    # title, no comment label and no canned-answer line to paste with it.
    plain = claude_answer(run(claude, ["-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose"], user("Pull request description draft (bpr.description-draft.v1)\n\nTitle: Clamp")).stdout)
    expect(plain.startswith("## Summary") and "## What changed" in plain and "## How to test" in plain and "Fixture agent" not in plain,
           f"a description in its three parts, title-free: {plain!r}")
    findings = claude_answer(run(claude, ["-p"], user("Review this pull request; answer with a ```bpr-findings block")).stdout)
    block = findings.split("```bpr-findings\n", 1)[1].split("\n```", 1)[0]
    expect(len(json.loads(block)["findings"]) == 2, "the findings block is JSON with two findings")
    # [[BPR-2219]]. A review is answered as a real agent answers one: a few
    # words beside its tool calls, then the report, which opens with its
    # title and verdict and whose findings block lists the report's findings.
    reviewed = run(claude, ["-p"], user("Review this pull request; answer with a ```bpr-findings block")).stdout
    said = [block["text"] for frame in frames(reviewed) if frame.get("type") == "assistant" for block in frame["message"]["content"] if block.get("type") == "text"]
    expect(len(said) == 4 and said[0].startswith("I'll start with the brief") and said[-1].startswith("# Review of #21\n\n**Verdict:** Needs changes\n\n"),
           f"a review is three working notes, then the report: {[text[:40] for text in said]}")
    result = [frame for frame in frames(reviewed) if frame.get("type") == "result"][-1]["result"]
    expect(result == said[-1], "the result is the last message, the report")
    listed = json.loads(block)["findings"]
    expect([(finding["path"], finding["startLine"], finding["endLine"]) for finding in listed]
           == [("src/engine/renderer.cpp", 12, 13), ("src/engine/shaders/post/tonemap.frag", 5, 5)]
           and "`src/engine/renderer.cpp:12-13`" in said[-1] and "`src/engine/shaders/post/tonemap.frag:5`" in said[-1],
           "the findings block lists the report's own findings at their places")

    # Look ask-v004: a tool-free run (What changed since my review) is told
    # `--tools ""`; its init frame lists no tool and it reads no file.
    free = run(claude, ["--tools", "", "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose"], user("returning.operation\tlatest_upload"))
    first = frames(free.stdout)
    expect(first[0]["type"] == "system" and first[0]["tools"] == [] and first[0]["mcp_servers"] == [], f"a tool-free run's init lists no tool: {first[0]}")
    expect(not any(frame.get("type") == "user" or '"tool_use"' in json.dumps(frame) for frame in first), "a tool-free run uses no tool")
    expect("# What changed since your review" in claude_answer(free.stdout), "and still answers")
    narrow = frames(run(claude, ["--tools", "Read,Grep,Glob,Bash", "-p"], user("anything")).stdout)
    expect(narrow[0]["tools"] == ["Read", "Grep", "Glob", "Bash"], f"a read-only run's init lists its tools: {narrow[0]}")

    crashed = run(claude, ["-p"], user("FIXTURE_CRASH"))
    expect(crashed.returncode != 0 and "scripted" in crashed.stderr, "a scripted crash fails")
    limited = run(claude, ["-p"], user("anything"), {"BPR_FIXTURE_AGENT_FAIL": "usage-limit"})
    expect(limited.returncode != 0 and "usage limit" in claude_answer(limited.stdout), "a scripted usage limit says so")
    limited = run(codex, ["exec"], "FIXTURE_USAGE_LIMIT")
    expect(limited.returncode != 0 and "usage limit" in limited.stdout, "codex's usage limit says so")
    check_steerable(claude)
    check_tour_revision(claude)
    check_held_turn(claude)
    print("fixture-agent-answers:pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
