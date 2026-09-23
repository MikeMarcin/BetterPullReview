#!/usr/bin/env python3
"""Read-only, standard-library checks for this RFC pack, not BPR runtime tests.

Python 3.10+. This validates authored structure, checksums and synthetic examples.
It is deliberately not a substitute for implement-rfc's live importer, a complete
JSON Schema implementation, a GitHub contract probe or Qt editor verification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
RESULT_SCHEMA = "bpr.review-preparation.result.v1"


class PackError(ValueError):
    """A pack invariant failed."""


def need(condition: bool, message: str) -> None:
    if not condition:
        raise PackError(message)


def safe_path(root: Path, name: str) -> Path:
    need(isinstance(name, str) and bool(name), "Empty payload path")
    p = PurePosixPath(name)
    need(not p.is_absolute() and ".." not in p.parts and "\\" not in name,
         f"Unsafe payload path: {name}")
    path = root.joinpath(*p.parts)
    need(path.resolve().is_relative_to(root.resolve()), f"Escaping payload path: {name}")
    return path


def payload_names(root: Path) -> list[str]:
    names = []
    for p in root.rglob("*"):
        need(not p.is_symlink(), f"Symlink is not a payload: {p}")
        if not p.is_file() or "__pycache__" in p.parts or p.suffix == ".pyc":
            continue
        name = p.relative_to(root).as_posix()
        if name != "SHA256SUMS":
            names.append(name)
    return sorted(names)


def load(root: Path, name: str):
    path = safe_path(root, name)
    need(path.is_file(), f"Missing payload: {name}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (ValueError, UnicodeError) as exc:
        raise PackError(f"Invalid JSON: {name}: {exc}") from exc


def text(root: Path, name: str) -> str:
    path = safe_path(root, name)
    need(path.is_file(), f"Missing payload: {name}")
    return path.read_bytes().decode("utf-8")


def header_field(body: str, field: str):
    need(body.startswith("---\n"), "Missing front matter")
    front = body.split("\n---", 1)[0]
    m = re.search(r"^" + re.escape(field) + r":\s*(.+)$", front, re.M)
    need(m is not None, f"Missing front-matter field: {field}")
    try:
        return json.loads(m.group(1))
    except ValueError as exc:
        raise PackError(f"Expected JSON-compatible field: {field}") from exc


def ancestors(graph: dict[str, list[str]]) -> dict[str, set[str]]:
    done: dict[str, set[str]] = {}
    visiting: set[str] = set()

    def visit(node: str) -> set[str]:
        need(node in graph, f"Unknown dependency: {node}")
        need(node not in visiting, f"Dependency cycle: {node}")
        if node in done:
            return done[node]
        visiting.add(node)
        result: set[str] = set()
        for dep in graph[node]:
            result.add(dep)
            result.update(visit(dep))
        visiting.remove(node)
        done[node] = result
        return result

    for node in graph:
        visit(node)
    return done


def unique(items: list, key: str, label: str) -> dict:
    result = {item[key]: item for item in items}
    need(len(result) == len(items), f"Duplicate {label}")
    return result


def exact_keys(value: dict, keys: set[str], label: str) -> None:
    need(isinstance(value, dict) and set(value) == keys, f"Unexpected fields in {label}")


def string(value, maximum: int, label: str, nonempty: bool = False) -> None:
    need(isinstance(value, str), f"Expected string: {label}")
    need(len(value) <= maximum, f"Oversize string: {label}")
    need(not nonempty or bool(value.strip()), f"Empty value: {label}")


def source_refs(value, maximum: int, supplied: set[str] | None, label: str) -> None:
    need(isinstance(value, list) and len(value) <= maximum, f"Invalid refs: {label}")
    for ref in value:
        string(ref, 256, label)
        need(supplied is None or ref in supplied, f"Unknown source ref: {ref}")


def validate_result(value: dict, supplied: set[str] | None = None) -> None:
    """Validate only the pack's three result shapes and dynamic source membership."""
    need(isinstance(value, dict) and value.get("schema") == RESULT_SCHEMA, "Invalid result schema")
    kind = value.get("kind")
    if kind == "rewrite" and value.get("status") == "ok":
        exact_keys(value, {"schema", "kind", "status", "replacement_text", "notes"}, "rewrite result")
        string(value["replacement_text"], 65536, "replacement")
        need(len(value["replacement_text"].encode("utf-8")) <= 65536, "Replacement byte ceiling")
        notes = value["notes"]
        need(isinstance(notes, list) and len(notes) <= 8, "Invalid notes")
        for note in notes:
            exact_keys(note, {"kind", "text", "source_refs"}, "note")
            need(note["kind"] in {"recovered", "inferred", "uncertain"}, "Invalid note kind")
            string(note["text"], 1024, "note text")
            source_refs(note["source_refs"], 16, supplied, "note refs")
    elif kind == "rewrite" and value.get("status") == "insufficient_context":
        exact_keys(value, {"schema", "kind", "status", "reason"}, "insufficient context result")
        string(value["reason"], 1024, "reason", True)
    elif kind == "outline":
        exact_keys(value, {"schema", "kind", "stops"}, "outline result")
        need(isinstance(value["stops"], list) and len(value["stops"]) <= 24, "Invalid outline size")
        for stop in value["stops"]:
            exact_keys(stop, {"title", "anchor_ref", "related_refs", "reason", "author_question"}, "outline stop")
            for key, maximum in [("title", 180), ("anchor_ref", 256), ("reason", 240)]:
                string(stop[key], maximum, key, True)
            string(stop["author_question"], 240, "author question")
            source_refs([stop["anchor_ref"]], 1, supplied, "primary anchor")
            source_refs(stop["related_refs"], 8, supplied, "related refs")
            need(len(set(stop["related_refs"])) == len(stop["related_refs"]), "Duplicate related ref")
    else:
        raise PackError("Unknown result kind/status")


def splice_exact(before: str, start: int, end: int, replacement: str) -> str:
    """Synthetic UTF-8 span oracle; does not simulate Qt undo or grapheme policy."""
    raw = before.encode("utf-8")
    need(type(start) is int and type(end) is int and 0 <= start <= end <= len(raw), "Invalid span")
    try:
        prefix = raw[:start].decode("utf-8")
        raw[start:end].decode("utf-8")
        suffix = raw[end:].decode("utf-8")
    except UnicodeError as exc:
        raise PackError("Span splits a UTF-8 code point") from exc
    return prefix + replacement + suffix


def verify(root: Path = ROOT) -> dict:
    root = root.resolve()
    names = payload_names(root)
    sums = text(root, "SHA256SUMS").splitlines()
    recorded = {}
    for line in sums:
        m = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        need(m is not None, "Malformed checksum line")
        digest, name = m.groups()
        safe_path(root, name)
        need(name not in recorded, f"Duplicate checksum: {name}")
        recorded[name] = digest
    need(set(names) == set(recorded), "Checksum inventory differs from payload")
    for name in names:
        need(hashlib.sha256(safe_path(root, name).read_bytes()).hexdigest() == recorded[name],
             f"Checksum mismatch: {name}")
        if name.endswith(".json"):
            load(root, name)

    manifest = load(root, "PACK-MANIFEST.json")
    need(manifest["schema"] == "bpr.review-tour.rfc-pack.v1", "Manifest schema")
    need(not manifest["product_implementation_performed"] and not manifest["live_service_verification_performed"],
         "Design pack must not claim product verification")
    for key in ("card_index", "milestone_plan", "milestones_document", "implementation_prompt", "source_baseline", "acceptance_matrix"):
        need(safe_path(root, manifest[key]).is_file(), f"Missing manifest target: {key}")
    need(len(manifest["default_hooks"]) == 4, "Expected four independent hooks")
    for name in manifest["default_hooks"]:
        need(safe_path(root, name).is_file(), f"Missing hook: {name}")

    rfcs = unique(manifest["rfcs"], "id", "RFC")
    need(len(rfcs) == 5, "Expected five RFCs")
    requirements = {}
    for rid, rfc in rfcs.items():
        body = text(root, rfc["path"])
        need(header_field(body, "id") == rid and rid in Path(rfc["path"]).name, "RFC identity mismatch")
        need(header_field(body, "status") == "Proposed", "RFC status must remain Proposed")
        for req, wording in re.findall(r"^- \*\*(AC\d{2}-\d{2}):\*\* (.+)$", body, re.M):
            need(req not in requirements, f"Duplicate requirement: {req}")
            requirements[req] = (rid, wording, rfc["path"])
    need(len(requirements) == 43, "Expected 43 RFC acceptance requirements")
    overlays = {n for n in names if n.startswith("repository-overlay/")}
    need(overlays == {r["path"] for r in rfcs.values()}, "Overlay may contain only the five new RFCs")

    cards = unique(load(root, manifest["card_index"])["cards"], "alias", "card")
    need(len(cards) == 26, "Expected 26 cards")
    for cid, card in cards.items():
        body = text(root, card["path"])
        need(header_field(body, "id") == cid, f"Card identity mismatch: {cid}")
        need(header_field(body, "schema") == "implement-rfc.card.v1", f"Card schema: {cid}")
        for key in ("depends_on", "rfcs"):
            need(header_field(body, key) == card[key], f"Card {key} mismatch: {cid}")
        need(set(card["requirements"]) <= set(requirements), f"Unknown card requirement: {cid}")
        need(set(card["rfcs"]) <= set(rfcs), f"Unknown card RFC: {cid}")
    ancestors({cid: c["depends_on"] for cid, c in cards.items()})

    plan = load(root, manifest["milestone_plan"])
    need(plan["schema"] == "implement-rfc.milestone-plan.v1" and plan["id"] == manifest["id"], "Plan identity/schema")
    need(plan["coverage"] == "complete" and plan["unplanned_work"] == [], "Plan coverage")
    need(set(plan["scope"]["rfcs"]) == set(rfcs), "Plan RFC scope mismatch")
    milestones = unique(plan["milestones"], "id", "milestone")
    need(len(milestones) == 8, "Expected eight milestones")
    ancestry = ancestors({mid: m["depends_on"] for mid, m in milestones.items()})
    planned, planned_rfcs = {}, []
    for mid, milestone in milestones.items():
        need(bool(milestone["outcome"]) and bool(milestone["exit_criteria"]) and bool(milestone["boundary"]), "Empty milestone contract")
        for work in milestone["work"]:
            if set(work) == {"issue"}:
                cid = work["issue"]
                need(cid in cards and cid not in planned, f"Unknown/duplicate planned card: {cid}")
                need(cards[cid]["milestone"] == mid, f"Card milestone mismatch: {cid}")
                planned[cid] = mid
            elif set(work) == {"rfc"}:
                planned_rfcs.append(work["rfc"])
            else:
                raise PackError("Invalid work reference")
    need(set(planned) == set(cards), "Missing planned card")
    need(len(planned_rfcs) == len(rfcs) and set(planned_rfcs) == set(rfcs), "Missing/duplicate planned RFC closeout")
    for cid, card in cards.items():
        for dep in card["depends_on"]:
            need(planned[dep] == planned[cid] or planned[dep] in ancestry[planned[cid]], f"Unordered milestone dependency: {cid}")
    need("manual-maintenance" in ancestry["scoped-ai"], "AI must follow complete manual fixture milestone")

    matrix = unique(load(root, manifest["acceptance_matrix"])["requirements"], "id", "acceptance row")
    need(set(matrix) == set(requirements), "Acceptance coverage mismatch")
    for req, row in matrix.items():
        need((row["rfc"], row["requirement"], row["rfc_path"]) == requirements[req], f"Acceptance text mismatch: {req}")
        need(row["implementation_status"] == "not_run" and row["evidence"] == [], f"Invented runtime evidence: {req}")
        need(bool(row["owner_cards"]) and bool(row["scenarios"]), f"Unowned/untested requirement: {req}")
        for cid in row["owner_cards"]:
            need(cid in cards and req in cards[cid]["requirements"], f"Requirement owner mismatch: {req}")
    for cid, card in cards.items():
        for req in card["requirements"]:
            need(cid in matrix[req]["owner_cards"], f"Missing reciprocal requirement ownership: {cid}/{req}")

    links_checked = 0
    for name in names:
        if not name.endswith(".md"):
            continue
        for target in re.findall(r"(?<!!)\[[^\]\n]+\]\(([^\s)]+)\)", text(root, name)):
            if target.startswith("#") or urlsplit(target).scheme:
                continue
            linked = (safe_path(root, name).parent / unquote(target.split("#")[0])).resolve()
            need(linked.is_relative_to(root) and linked.is_file(), f"Broken local Markdown link: {name} -> {target}")
            links_checked += 1

    route = load(root, "examples/github-route.json")
    need(route["synthetic"] and urlsplit(route["host"]).hostname.endswith(".invalid"), "Examples must be synthetic")
    overview = text(root, route["index_body_path"])
    stop_urls = [s["url"] for s in route["stops"]]
    need(len(set(stop_urls)) == len(stop_urls), "Duplicate website stop")
    index_urls = re.findall(r"^\d+\. \[[^\]]+\]\(([^)]+)\)", overview, re.M)
    need(index_urls == stop_urls, "Website index order/targets mismatch")
    for i, stop in enumerate(route["stops"]):
        body = text(root, stop["body_path"])
        need(stop["url"].split("#")[0] == route["pull_request"], "Cross-PR comment link")
        need(route["index_url"] in body, "Missing overview navigation")
        if i:
            need(stop_urls[i - 1] in body, "Missing previous navigation")
        if i + 1 < len(stop_urls):
            need(stop_urls[i + 1] in body, "Missing next navigation")
        need(load(root, f"examples/api/update-comment-{i + 1}.json") == {"body": body}, "Comment update fixture mismatch")
    fallback = text(root, "examples/manual-tour/file-subject-fallback.md")
    need(fallback.startswith(route["host"] + "/") and "?plain=1#L" in fallback.splitlines()[0]
         and fallback.splitlines()[1] == "", "Fallback must start with canonical source permalink")

    create = load(root, "examples/api/create-pending-review.json")
    need("event" not in create, "Pending creation must omit event")
    need(re.fullmatch(r"[0-9a-f]{40}", create["commit_id"]) is not None, "Expected pinned commit")
    need(len(create["comments"]) == len(stop_urls), "Pending batch membership mismatch")
    allowed = {"path", "body", "line", "side", "start_line", "start_side", "position"}
    for c in create["comments"]:
        need(set(c) <= allowed, "Undocumented field in REST batch comment")
    response = load(root, "examples/api/pending-review-response.json")
    need(response["state"] == "PENDING", "Expected pending response")
    staged = unique(load(root, "examples/api/staged-comments-response.json"), "id", "staged comment ID")
    need(set(staged) == {s["comment_id"] for s in route["stops"]}, "Remote comment membership mismatch")
    for stop, candidate in zip(route["stops"], create["comments"]):
        remote = staged[stop["comment_id"]]
        need(remote["pull_request_review_id"] == response["id"] and remote["html_url"] == stop["url"], "Remote identity/URL mismatch")
        need(all(remote[k] == candidate[k] for k in ("path", "body", "line", "side")), "Staged comment mapping mismatch")
    submit = load(root, "examples/api/submit-review.json")
    need(submit == {"event": "COMMENT", "body": overview}, "Submission must be COMMENT with complete index")

    refs = set(load(root, "examples/ai/outline-context.json")["supplied_source_refs"])
    for filename in ("rewrite-result.json", "insufficient-context-result.json", "outline-result.json"):
        validate_result(load(root, "examples/ai/" + filename), refs)
    cases = load(root, "examples/text-transactions.json")["cases"]
    unique(cases, "name", "splice case")
    for case in cases:
        result = splice_exact(case["before"], case["start_utf8"], case["end_utf8"], case["replacement"])
        need(result == case["expected"], f"Splice fixture mismatch: {case['name']}")
        need(case["creates_undo_entry"] == (result != case["before"]), f"No-op undo mismatch: {case['name']}")
    return {"status": "passed", "scope": "offline_pack_only", "payload_files": len(names),
            "rfcs": len(rfcs), "cards": len(cards), "milestones": len(milestones),
            "acceptance_requirements": len(requirements), "local_markdown_links": links_checked,
            "synthetic_splice_cases": len(cases), "product_runtime_tested": False,
            "live_github_tested": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT, help="Pack directory to inspect")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable output")
    args = parser.parse_args()
    try:
        result = verify(args.root)
    except (PackError, OSError, KeyError, TypeError, UnicodeError) as exc:
        if args.json:
            print(json.dumps({"status": "failed", "error": str(exc)}))
        else:
            print(f"Pack validation failed: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Pack validation passed: {result['rfcs']} RFCs, {result['cards']} cards, "
              f"{result['milestones']} milestones, {result['acceptance_requirements']} requirements.")
        print(f"Checked {result['payload_files']} payload checksums, {result['local_markdown_links']} local links "
              f"and {result['synthetic_splice_cases']} synthetic text transactions.")
        print("Offline artifact checks only. No BPR runtime or live GitHub verification was performed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
