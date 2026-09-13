#!/usr/bin/env python3
"""Local-only CLI publication fixture for the tests-disabled macOS application.

Prepare, then launch and operate the ordinary Connect, draft/Submit review and
CLI confirmation UI. No consent is supplied by this fixture. The child never
writes files or opens a listener. A typed UI terminal must be observed externally;
exit status is not public-path evidence. Transport tests own exact POST counts.

Sequence: prepare/positive; Connect and load the complete PR and selected file
comparison; for an inline root comment, select an explicit right-side source
line (a replacement group may prefer the unsupported left side); control/hold;
submit through the ordinary confirmation; externally observe the gh POST process;
control/published within 15 seconds with the exact --body and inline --path/--line.
Hold serves empty baselines and mutation target identity, then holds only the POST
response until published makes reconciliation receipts visible. Publication is
an immediate-response alternative. Account-mismatch, target-mismatch or ambiguous
can instead be selected after initial load. Start fresh for each independent case.
It is an external server-state simulation, not proof that a POST occurred.
"""
import argparse
import json
from pathlib import Path
import shutil
import sys
import time

sys.dont_write_bytecode = True
import rfc0015_cli_read_fixture as read_fixture

PULL = "/repos/octo-org/review-repository/pulls/17"
COURSES = ("positive", "publication", "hold", "account-mismatch", "target-mismatch", "ambiguous", "published")


def prepare(directory, body):
    result = read_fixture.prepare(directory, "positive")
    shutil.copyfile(Path(read_fixture.__file__), directory / "bin/rfc0015_cli_read_fixture.py")
    executable = directory / "bin/gh"
    executable.write_text(f"#!{sys.executable}\n" + Path(__file__).read_text().split("\n", 1)[1])
    executable.chmod(0o700)
    control(directory, "positive", body)
    return result


def control(directory, course, body, path="review.cpp", line=1):
    if (course not in COURSES or not body or len(body.encode()) > 16384
            or path not in ("review.cpp", "secondary.cpp") or not 1 <= line <= 100):
        raise ValueError("invalid synthetic course or body")
    control_path = directory / "publication.json"
    temporary = directory / "publication.next"
    temporary.write_text(json.dumps({"course": course, "body": body, "path": path, "line": line}))
    temporary.replace(control_path)


def receipt(fixture, payload, review):
    result = dict(payload, id=9001 if review else 9002,
                  node_id="PRR_fixture9001" if review else "PRRC_fixture9002",
                  user=fixture["routes"]["/user"])
    if review:
        result.pop("event")
        result.update(state={"COMMENT": "COMMENTED", "APPROVE": "APPROVED",
                             "REQUEST_CHANGES": "CHANGES_REQUESTED"}[payload["event"]],
                      submitted_at="2026-09-13T12:00:00Z",
                      pull_request_url="https://api.github.com" + PULL)
    return result


def respond(body, status=200):
    wire = (f"HTTP/2.0 {status} OK\r\nContent-Type: application/json\r\n"
            "X-RateLimit-Limit: 5000\r\nX-RateLimit-Remaining: 4999\r\n"
            "X-RateLimit-Reset: 2000000000\r\nX-RateLimit-Resource: core\r\n\r\n")
    sys.stdout.buffer.write((wire + json.dumps(body)).encode())
    return 0


def fake_gh():
    directory = Path(__file__).resolve().parents[1]
    fixture = json.loads((directory / "fixture.json").read_text())
    state = json.loads((directory / "publication.json").read_text())
    args = sys.argv[1:]
    # The reused read responder's trace is disabled even outside the sandbox.
    read_fixture.record_operation = lambda *_: None
    if len(args) < 7 or args[0] != "api":
        return read_fixture.fake_gh()
    endpoint = "/" + args[1].lstrip("/").split("?", 1)[0]
    if args[2:6] != ["--hostname", "github.com", "--include", "--method"]:
        return 64
    if args[6] == "GET":
        if endpoint == "/user" and state["course"] == "account-mismatch":
            return respond(dict(fixture["routes"]["/user"], id=999, node_id="U_other", login="other-reviewer"))
        if endpoint == PULL and state["course"] != "positive":
            target = fixture["routes"][PULL]
            repository = fixture["routes"]["/repos/octo-org/review-repository"]
            for side in ("base", "head"):
                target[side]["repo"]["node_id"] = repository["node_id"]
            if state["course"] == "target-mismatch":
                target["head"]["sha"] = "f" * 40
            return respond(target)
        if endpoint in (PULL + "/reviews", PULL + "/comments"):
            if state["course"] != "published":
                return respond([])
            review = endpoint.endswith("/reviews")
            payload = {"body": state["body"], "commit_id": fixture["head"]}
            payload.update({"event": "COMMENT"} if review else
                           {"path": state["path"], "line": state["line"], "side": "RIGHT"})
            return respond([receipt(fixture, payload, review)])
    if args[6] != "POST" or endpoint == "/graphql":
        return read_fixture.fake_gh()
    if endpoint not in (PULL + "/reviews", PULL + "/comments") or args[-2:] != ["--input", "-"]:
        return 64
    raw = sys.stdin.buffer.read(262145)
    if len(raw) > 262144:
        return 65
    try:
        payload = json.loads(raw)
        review = endpoint.endswith("/reviews")
        fields = {"body", "commit_id", "event"} if review else {"body", "commit_id", "path", "side", "line"}
        if (set(payload) != fields or payload["commit_id"] != fixture["head"]
                or not isinstance(payload["body"], str) or not payload["body"]
                or (review and payload["event"] not in ("COMMENT", "APPROVE", "REQUEST_CHANGES"))
                or (not review and (payload["path"] not in ("review.cpp", "secondary.cpp")
                    or payload["side"] != "RIGHT" or type(payload["line"]) is not int
                    or not 1 <= payload["line"] <= 100))):
            return 64
    except (ValueError, TypeError, KeyError):
        return 64
    if state["course"] == "hold":
        # External automation observes this POST process, then atomically changes
        # server state. No trace, response body or request is written by the child.
        deadline = time.monotonic() + 15
        while state["course"] == "hold" and time.monotonic() < deadline:
            time.sleep(0.05)
            state = json.loads((directory / "publication.json").read_text())
        if state["course"] != "published":
            return 1
    if state["course"] == "ambiguous":
        return 1  # Accepted request shape, then lost response; no repeat evidence inferred.
    return respond(receipt(fixture, payload, review), 200 if review else 201)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "control", "launch"))
    parser.add_argument("--directory", type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--course", choices=COURSES, default="positive")
    parser.add_argument("--body", default="Synthetic CLI publication fixture.")
    parser.add_argument("--path", choices=("review.cpp", "secondary.cpp"), default="review.cpp")
    parser.add_argument("--line", type=int, default=1)
    parser.add_argument("--app", type=Path)
    parser.add_argument("--cmake-cache", type=Path)
    parser.add_argument("--seconds", type=int, default=180)
    args = parser.parse_args()
    if args.action == "prepare":
        print(json.dumps(prepare(args.directory, args.body)))
    elif args.action == "control":
        control(args.directory, args.course, args.body, args.path, args.line)
        print(json.dumps({"course": args.course}))
    elif args.app and args.cmake_cache and 1 <= args.seconds <= 300:
        read_fixture.launch(args)
    else:
        parser.error("launch requires --app, --cmake-cache and 1..300 seconds")


if __name__ == "__main__":
    if Path(__file__).name == "gh":
        sys.exit(fake_gh())
    main()
