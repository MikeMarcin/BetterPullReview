#!/usr/bin/env python3
"""Offline artifact checks only. Does not execute BPR, a model, gh, or network I/O."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]

class PackError(ValueError):
    pass

def require(condition: bool, message: str) -> None:
    if not condition:
        raise PackError(message)

def load(root: Path, path: str):
    try:
        return json.loads((root / path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise PackError(f"Invalid JSON or missing file: {path}") from exc

def payload_names(root: Path) -> list[str]:
    names = []
    for p in root.rglob("*"):
        require(not p.is_symlink(), f"Symlink not allowed: {p.relative_to(root)}")
        if p.is_file() and p.name != "SHA256SUMS":
            names.append(p.relative_to(root).as_posix())
    return sorted(names)

def verify_hashes(root: Path) -> int:
    try:
        rows = (root / "SHA256SUMS").read_text().splitlines()
    except OSError as exc:
        raise PackError("Missing SHA256SUMS") from exc
    declared = {}
    for row in rows:
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", row)
        require(match is not None, "Invalid checksum row")
        digest, name = match.groups()
        require(name not in declared and not Path(name).is_absolute() and ".." not in Path(name).parts,
                "Invalid/duplicate checksum path")
        declared[name] = digest
    names = payload_names(root)
    require(set(names) == set(declared), "Payload inventory differs from checksums")
    require(len({n.casefold() for n in names}) == len(names), "Case-colliding paths")
    for name in names:
        actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
        require(actual == declared[name], f"Checksum mismatch: {name}")
    return len(names)

def acyclic(graph: dict[str, list[str]], label: str) -> None:
    visiting, done = set(), set()
    def visit(node: str) -> None:
        require(node in graph, f"Unknown {label} dependency: {node}")
        require(node not in visiting, f"Dependency cycle in {label}: {node}")
        if node in done:
            return
        visiting.add(node)
        for dep in graph[node]:
            visit(dep)
        visiting.remove(node)
        done.add(node)
    for node in graph:
        visit(node)

def ancestors(graph: dict[str, list[str]], node: str) -> set[str]:
    out = set()
    stack = list(graph[node])
    while stack:
        item = stack.pop()
        if item not in out:
            out.add(item)
            stack.extend(graph[item])
    return out

def check_envelope(obj: dict, commands: set[str]) -> None:
    require(set(obj) == {"schema", "command", "request_id", "status", "data", "warnings", "error"}, "Envelope keys")
    require(obj["schema"] == "bpr.tour.cli-result.v1", "Envelope schema")
    require(obj["command"] in commands, "Envelope command")
    require(isinstance(obj["request_id"], str) and bool(obj["request_id"]), "Request identity")
    require(obj["status"] in {"ok", "error", "conflict", "partial", "uncertain"}, "Envelope status")
    require(isinstance(obj["data"], dict) and isinstance(obj["warnings"], list), "Envelope data")
    if obj["status"] == "ok":
        require(obj["error"] is None, "Successful envelope has error")
    def ids(value):
        if isinstance(value, dict):
            for key, val in value.items():
                if key.endswith("_id") and val is not None:
                    require(isinstance(val, str), f"Lossless identifier must be string: {key}")
                if key.endswith("_ids"):
                    require(isinstance(val, list) and all(isinstance(v, str) for v in val), "Identifier array")
                ids(val)
        elif isinstance(value, list):
            for val in value:
                ids(val)
    ids(obj["data"])

def reference_splice(case: dict) -> tuple[str, str]:
    """Small prose-splice oracle for fixtures, not the product editor or authorization logic."""
    before = case["before"]
    if case["captured_revision"] != case["current_revision"]:
        return "stale", before
    raw = before.encode("utf-8")
    begin, end = case["begin"], case["end"]
    if not (isinstance(begin, int) and isinstance(end, int) and 0 <= begin <= end <= len(raw)):
        return "invalid_range", before
    try:
        raw[:begin].decode("utf-8")
        raw[begin:end].decode("utf-8")
        raw[end:].decode("utf-8")
        replacement = case["replacement"].encode("utf-8")
    except UnicodeError:
        return "invalid_range", before
    if raw[begin:end] == replacement:
        return "no_op", before
    return "applied", (raw[:begin] + replacement + raw[end:]).decode("utf-8")

def verify(root: Path = ROOT) -> dict:
    root = root.resolve()
    payload_count = verify_hashes(root)
    manifest = load(root, "PACK-MANIFEST.json")
    require(manifest["extension_only"] and manifest["new_files_only_overlay"], "Extension-only boundary")
    for key in ("product_implementation_performed", "live_service_verification_performed", "skill_installed"):
        require(manifest[key] is False, "Unsupported implementation/installation claim")
    expected_rfcs = {"RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"}
    rfcs = manifest["rfcs"]
    require({r["id"] for r in rfcs} == expected_rfcs and len(rfcs) == 3, "New RFC scope")
    overlay = {p.relative_to(root).as_posix() for p in (root / "repository-overlay").rglob("*") if p.is_file()}
    require(overlay == {r["path"] for r in rfcs}, "Parent or unexpected overlay file")
    definitions = {}
    for r in rfcs:
        text = (root / r["path"]).read_text()
        require(f'id: "{r["id"]}"' in text, "RFC frontmatter identity")
        for aid, requirement in re.findall(r"^- \*\*(AC\d+-\d+):\*\* (.+)$", text, re.M):
            require(aid not in definitions, "Duplicate acceptance ID")
            definitions[aid] = (r["id"], requirement)
    require(len(definitions) == 32, "Expected 32 acceptance definitions")
    plan = load(root, manifest["milestone_plan"])
    require(plan["schema"] == "implement-rfc.milestone-plan.v1", "Plan schema")
    require(set(plan["scope"]["rfcs"]) == expected_rfcs, "Plan re-imports parent scope")
    require(plan["coverage"] == "complete" and plan["unplanned_work"] == [], "Plan completeness")
    milestones = plan["milestones"]
    require(len(milestones) == 6, "Expected six milestones")
    mg = {m["id"]: m["depends_on"] for m in milestones}
    require(len(mg) == len(milestones), "Duplicate milestone ID")
    acyclic(mg, "milestones")
    cards = load(root, manifest["card_index"])["cards"]
    require(len(cards) == 20, "Expected 20 cards")
    cg = {c["id"]: c["depends_on"] for c in cards}
    require(len(cg) == len(cards), "Duplicate card ID")
    acyclic(cg, "cards")
    work = {}
    for m in milestones:
        require(m["outcome"] and m["exit_criteria"] and m["boundary"], "Empty milestone contract")
        for row in m["work"]:
            cid = row["issue"]
            require(cid not in work, "Duplicate milestone work")
            work[cid] = m["id"]
    require(set(work) == set(cg), "Milestone/card coverage")
    ownership = {}
    for c in cards:
        text = (root / c["path"]).read_text()
        require(f'id: "{c["id"]}"' in text and 'status: "Backlog"' in text, "Card frontmatter")
        require(c["milestone"] == work[c["id"]], "Card milestone differs")
        for dep in c["depends_on"]:
            require(work[dep] == work[c["id"]] or work[dep] in ancestors(mg, work[c["id"]]), "Card dependency violates milestone order")
        for aid in c["acceptance"]:
            require(aid in definitions, "Unknown requirement ownership")
            require(aid not in ownership, "Duplicate primary acceptance owner")
            ownership[aid] = c["id"]
    require(set(ownership) == set(definitions), "Acceptance ownership gap")
    matrix = load(root, manifest["acceptance_matrix"])
    require(matrix["product_evidence_status"] == "Not run", "Product evidence claim")
    require(len(matrix["requirements"]) == len(definitions), "Acceptance matrix count")
    for r in matrix["requirements"]:
        require(r["id"] in definitions, "Unknown matrix row")
        require((r["rfc"], r["requirement"]) == definitions[r["id"]], "Matrix/RFC text differs")
        require(r["primary_card"] == ownership[r["id"]], "Matrix ownership differs")
        require(r["status"] == "Not run" and not r["evidence"], "Fabricated product evidence")
    skill = manifest["skill"]
    require(skill["name"] == "pr-tour", "Skill name")
    sp = root / skill["path"]
    st = sp.read_text()
    require(sp.parent.name == "pr-tour" and re.search(r"^name: pr-tour$", st, re.M), "Skill name/frontmatter")
    require(len(st.split()) < 1200, "Main skill no longer thin")
    require(re.search(r"^description: .+", st, re.M) is not None, "Skill description")
    skill_files = [p for p in (root / "skill-source").rglob("*") if p.is_file()]
    require(all(p.suffix == ".md" for p in skill_files), "Executable code in thin skill")
    require(all(p == root/"skill-source/README.md" or sp.parent in p.parents for p in skill_files), "Second skill source")
    cat = load(root, "specs/command-catalog.json")
    require(cat["default_output"] == "human" and cat["machine_flag"] == "--json", "Human default output")
    require(cat["skill_name"] == "pr-tour" and cat["no_generic_api_passthrough"], "Thin wrapper contract")
    require([c["name"] for c in cat["commands"] if c["remote_write"]] == ["publish"], "Unexpected remote writer")
    commands = {c["name"] for c in cat["commands"]}
    schema = load(root, "specs/cli-result.schema.json")
    require(set(schema["properties"]["command"]["enum"]) == commands, "Schema/catalog commands")
    for folder in ("examples/cli", "examples/assist"):
        for path in (root / folder).glob("*.json"):
            obj = load(root, path.relative_to(root).as_posix())
            if obj["schema"] == "bpr.tour.cli-result.v1":
                check_envelope(obj, commands)
            else:
                require(obj["schema"] == "bpr.review-preparation.result.v1", "Parent result schema fork")
                require(set(obj) == {"schema", "kind", "status", "replacement_text", "notes"}, "Unexpected model result authority")
    for case in load(root, "examples/text-ranges.json")["cases"]:
        require(reference_splice(case) == (case["expected_status"], case["expected"]), f"Reference splice mismatch: {case['id']}")
    for scenario in load(root, "examples/scenario-catalog.json")["scenarios"]:
        require(scenario["status"] == "Not run" and set(scenario["acceptance"]) <= set(definitions), "Scenario claims/scope")
    for path in root.rglob("*.md"):
        text = path.read_text(encoding="utf-8")
        for dest in re.findall(r"(?<!!)\[[^\]\n]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)", text):
            if dest.startswith(("https://", "http://", "mailto:", "#")):
                continue
            target = (path.parent / dest.split("#",1)[0]).resolve()
            require(target == root or root in target.parents, f"Escaping local link: {path.name} -> {dest}")
            require(target.exists(), f"Broken local link: {path.name} -> {dest}")
    baseline = load(root, manifest["source_baseline"])
    require(baseline["parent_pack"]["id"] == manifest["parent_pack_id"], "Parent identity")
    require(baseline["parent_pack"]["user_reports_already_imported"] and not baseline["parent_pack"]["live_import_mapping_observed"], "Parent observation scope")
    require(baseline["parent_pack"]["payload_checksums_verified"], "Parent checksums not recorded")
    for key in ("source_snapshot", "parent_pack"):
        require(re.fullmatch("[0-9a-f]{64}", baseline[key]["sha256"]) is not None, "Input hash shape")
    required_seams = {"src/application/CMakeLists.txt", "src/adapters/github/CMakeLists.txt", "src/app/CMakeLists.txt", ".mask.json"}
    require(required_seams <= {e["path"] for e in baseline["source_snapshot"]["source_entries"]}, "Missing source seam")
    return {"status":"passed", "scope":"offline_artifact_checks_only", "payload_files":payload_count,
            "rfcs":len(rfcs), "cards":len(cards), "milestones":len(milestones), "acceptance_requirements":len(definitions),
            "product_implementation_performed":False, "live_service_verification_performed":False}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        result = verify(args.root)
    except (PackError, KeyError, TypeError, OSError) as exc:
        if args.json:
            print(json.dumps({"status":"failed", "error":str(exc)}))
        else:
            print(f"Pack validation failed: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Pack validation passed: {result['rfcs']} RFCs, {result['cards']} cards, "
              f"{result['milestones']} milestones, {result['acceptance_requirements']} requirements, "
              f"{result['payload_files']} checked payload files.")
        print("Artifact checks only; no BPR, native, model, or live-service verification was performed.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
