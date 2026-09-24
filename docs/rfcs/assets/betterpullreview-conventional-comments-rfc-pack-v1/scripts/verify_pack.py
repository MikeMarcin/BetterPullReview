#!/usr/bin/env python3
"""Read-only validation of this RFC archive, not verification of BPR implementation.

Requires Python 3.10+. Uses only the standard library and never invokes commands,
models, credential stores, the network or BPR. The counter oracle consumes already
normalized synthetic conversation facts; it is intentionally not a product parser.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path, PurePosixPath
import re
import sys
from typing import Any

LABELS = (
    "praise", "nitpick", "suggestion", "issue", "todo", "question",
    "thought", "chore", "note", "typo", "polish", "quibble",
)
STATES = ("unresolved", "resolved", "unknown", "not_applicable")
STATUSES = ("recognized", "plain", "unknown_label", "malformed", "limited")
EXPECTATIONS = ("unspecified", "blocking", "non_blocking", "if_minor", "conflicting")


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def safe_path(root: Path, relative: str) -> Path:
    """Reject absolute, escaping and platform-dependent archive references."""
    posix = PurePosixPath(relative)
    if not relative or posix.is_absolute() or ".." in posix.parts or "\\" in relative or ":" in relative:
        raise ValueError(f"unsafe relative path: {relative!r}")
    path = root / posix
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError(f"path escapes archive root: {relative!r}")
    return path


def frontmatter(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    if not lines or lines[0] != "---":
        raise ValueError(f"missing JSON-valued frontmatter: {path.name}")
    result: dict[str, Any] = {}
    for line in lines[1:]:
        if line == "---":
            return result
        key, delimiter, value = line.partition(":")
        if not delimiter or key in result:
            raise ValueError(f"malformed/duplicate frontmatter key: {path.name}: {line}")
        result[key] = json.loads(value.strip())
    raise ValueError(f"unterminated frontmatter: {path.name}")


def reachable(graph: dict[str, list[str]], node: str) -> set[str]:
    seen: set[str] = set()
    pending = list(graph.get(node, []))
    while pending:
        current = pending.pop()
        if current not in seen:
            seen.add(current)
            pending.extend(graph.get(current, []))
    return seen


def graph_errors(graph: dict[str, list[str]], kind: str, external: set[str] | None = None) -> list[str]:
    external = external or set()
    errors: list[str] = []
    for node, dependencies in graph.items():
        if len(dependencies) != len(set(dependencies)):
            errors.append(f"{kind} {node}: duplicate dependency")
        for dependency in dependencies:
            if dependency not in graph and dependency not in external:
                errors.append(f"{kind} {node}: unknown dependency {dependency}")
        if node in reachable(graph, node):
            errors.append(f"{kind} {node}: dependency cycle")
    return errors


def integrity_errors(root: Path) -> list[str]:
    errors: list[str] = []
    ledger = root / "SHA256SUMS"
    if not ledger.is_file():
        return ["missing SHA256SUMS"]
    inventory: dict[str, str] = {}
    for line in ledger.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if match is None:
            errors.append("malformed checksum ledger line")
            continue
        digest, relative = match.groups()
        if relative in inventory:
            errors.append(f"duplicate checksum path: {relative}")
            continue
        if relative == "SHA256SUMS":
            errors.append("checksum inventory must not include itself")
            continue
        inventory[relative] = digest
        path = safe_path(root, relative)
        if path.is_symlink() or not path.is_file():
            errors.append(f"missing or nonregular payload: {relative}")
        elif hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            errors.append(f"checksum mismatch: {relative}")
    observed = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            errors.append(f"symlink is not an archive payload: {path.relative_to(root)}")
        elif path.is_file() and path != ledger:
            observed.add(path.relative_to(root).as_posix())
    for relative in sorted(observed - inventory.keys()):
        errors.append(f"unlisted payload: {relative}")
    for relative in sorted(inventory.keys() - observed):
        errors.append(f"missing inventory payload: {relative}")
    return errors


def normalized_scope(units: list[dict[str, Any]], scope: dict[str, str]) -> list[dict[str, Any]]:
    """Small fixture oracle; it does not infer roots, parse bodies or fetch pages."""
    unique: dict[str, dict[str, Any]] = {}
    if scope.get("kind") not in ("pr", "file", "folder"):
        raise ValueError("invalid fixture scope")
    for unit in units:
        key = unit["id"]
        if key in unique and unique[key] != unit:
            raise ValueError(f"contradictory duplicate normalized identity: {key}")
        if unit["resolution"] not in STATES:
            raise ValueError(f"invalid resolution state: {key}")
        if unit["root_label"] is not None and unit["root_label"] not in LABELS:
            raise ValueError(f"unknown catalog ID in normalized unit: {key}")
        unique[key] = unit
    kept = []
    for unit in unique.values():
        if unit["visibility"] != "public" or unit["publication"] != "published" or unit["deleted"]:
            continue
        path = unit["file"]
        if scope["kind"] == "file" and path != scope["path"]:
            continue
        if scope["kind"] == "folder" and (path is None or not path.startswith(scope["path"].rstrip("/") + "/")):
            continue
        kept.append(unit)
    return kept


def counter_projection(units: list[dict[str, Any]], scope: dict[str, str], columns: list[str], mode: str) -> dict[str, Any]:
    if len(columns) != len(set(columns)) or not set(columns).issubset(LABELS):
        raise ValueError("invalid selected column IDs")
    if mode not in ("unresolved", "all"):
        raise ValueError("invalid fixture resolution mode")
    population = normalized_scope(units, scope)
    counts = dict.fromkeys(columns, 0)
    histogram = dict.fromkeys(STATES, 0)
    catchall = 0
    for unit in population:
        state = unit["resolution"]
        histogram[state] += 1
        if mode == "unresolved" and state != "unresolved":
            continue
        label = unit["root_label"]
        if label in counts:
            counts[label] += 1
        else:
            catchall += 1
    return {
        "included_total": sum(counts.values()) + catchall,
        "counts": counts,
        "catchall": catchall,
        "scope_total": len(population),
        "scope_states": histogram,
        "classification_complete": all(unit["classification_known"] for unit in population),
        "resolution_complete": histogram["unknown"] == 0,
    }


def validate(root: Path, *, check_integrity: bool = True, exhaustive: bool = True) -> list[str]:
    """Return actionable validation errors. Does not write any file."""
    errors: list[str] = []
    try:
        if check_integrity:
            errors.extend(integrity_errors(root))
        manifest = load_json(root / "PACK-MANIFEST.json")
        if manifest["expected_counts"] != {"rfcs": 5, "cards": 27, "milestones": 8, "requirements": 48, "labels": 12}:
            errors.append("unexpected declared pack counts")
        if manifest.get("product_validation_status") != "not_run" or manifest.get("runtime_code_included") is not False:
            errors.append("pack must not claim product validation or shipped runtime code")
        catalog = load_json(root / "specs/LABEL-CATALOG.json")["labels"]
        catalog_ids = [row["id"] for row in catalog]
        if tuple(catalog_ids) != LABELS or len(set(catalog_ids)) != 12:
            errors.append("catalog must contain exactly all twelve distinct supported labels")
        if any(row.get("first_class") is not True or row.get("icon_role") != "feedback.label." + row["id"] for row in catalog):
            errors.append("catalog first-class/icon role mismatch")
        schema = load_json(root / "specs/feedback-annotation.schema.json")
        if schema["properties"]["label"]["enum"] != list(LABELS) + [None]:
            errors.append("annotation schema label vocabulary drift")
        rfcs: dict[str, dict[str, Any]] = {}
        for row in manifest["proposed_rfcs"]:
            data = frontmatter(safe_path(root, row["path"]))
            if data["id"] in rfcs:
                errors.append("duplicate RFC ID")
            rfcs[data["id"]] = data
            if data["id"] != row["id"] or data["status"] != "Proposed":
                errors.append(f"RFC frontmatter mismatch: {row['id']}")
        expected_rfcs = {f"RFC-BPR-{i:04}" for i in range(47, 52)}
        if set(rfcs) != expected_rfcs:
            errors.append("new RFC allocation must be 0047 through 0051 before live remapping")
        errors.extend(graph_errors({key: row["depends_on"] for key, row in rfcs.items()}, "RFC", set(manifest["external_rfc_dependencies"])))
        cards: dict[str, dict[str, Any]] = {}
        for row in manifest["cards"]:
            data = frontmatter(safe_path(root, row["path"]))
            if data["id"] in cards:
                errors.append(f"duplicate card ID: {data['id']}")
            cards[data["id"]] = data
            if data["id"] != row["id"] or data["schema"] != "implement-rfc.card.v1" or data["status"] != "Backlog":
                errors.append(f"card frontmatter mismatch: {row['id']}")
            if not set(data["rfcs"]).issubset(rfcs):
                errors.append(f"card {data['id']}: unknown owning RFC")
            if data["metadata"].get("alias_requires_live_allocation") is not True:
                errors.append(f"card {data['id']}: missing live-allocation marker")
        if set(cards) != {f"PACK-CC-{i:02}" for i in range(1, 28)}:
            errors.append("card ID coverage must be PACK-CC-01 through PACK-CC-27")
        errors.extend(graph_errors({key: row["depends_on"] for key, row in cards.items()}, "card"))
        for n in range(23, 28):
            row = cards.get(f"PACK-CC-{n:02}", {})
            if row.get("closeout_for") != [f"RFC-BPR-{n + 24:04}"]:
                errors.append(f"incorrect closeout mapping on card {n}")

        plan = load_json(root / manifest["milestone_plan"])
        if plan["schema"] != "implement-rfc.milestone-plan.v1" or plan["id"] != manifest["id"] or plan["coverage"] != "complete":
            errors.append("invalid authored milestone plan identity/schema/coverage")
        if set(plan["scope"]["rfcs"]) != set(rfcs):
            errors.append("milestone RFC scope mismatch")
        milestones = {row["id"]: row for row in plan["milestones"]}
        if len(milestones) != 8 or len(plan["milestones"]) != 8:
            errors.append("milestone IDs must be unique and number eight")
        milestone_graph = {key: row["depends_on"] for key, row in milestones.items()}
        errors.extend(graph_errors(milestone_graph, "milestone"))
        work_owner: dict[str, str] = {}
        for name, row in milestones.items():
            if not row.get("outcome") or not row.get("exit_criteria"):
                errors.append(f"milestone {name}: missing outcome or exit criteria")
            for work in row["work"]:
                key = work["issue"]
                if key in work_owner:
                    errors.append(f"duplicate milestone work: {key}")
                work_owner[key] = name
                if key not in cards:
                    errors.append(f"milestone {name}: unknown card {key}")
                elif cards[key]["metadata"]["milestone"] != name:
                    errors.append(f"milestone metadata mismatch: {key}")
        if set(work_owner) != set(cards):
            errors.append("milestone work coverage differs from cards")
        for key, card in cards.items():
            owner = work_owner.get(key)
            if owner is None:
                continue
            allowed = reachable(milestone_graph, owner) | {owner}
            for dependency in card["depends_on"]:
                if dependency in work_owner and work_owner[dependency] not in allowed:
                    errors.append(f"card {key}: dependency crosses an undeclared milestone predecessor")

        reqdoc = load_json(root / manifest["requirements"])
        requirements = {row["id"]: row for row in reqdoc["requirements"]}
        req_ids = {f"R{i:02}" for i in range(1, 49)}
        if len(reqdoc["requirements"]) != 48 or set(requirements) != req_ids:
            errors.append("requirements must cover R01 through R48 exactly once")
        for key, row in requirements.items():
            if not row["cards"] or not set(row["cards"]).issubset(cards):
                errors.append(f"requirement {key}: missing/unknown card owner")
            if row["status"] != "not_run_product":
                errors.append(f"requirement {key}: unearned product result")
            backlinks = {alias for alias, card in cards.items() if key in card["metadata"]["acceptance"]}
            if backlinks != set(row["cards"]):
                errors.append(f"requirement {key}: card backlink mismatch")
        for key, row in cards.items():
            if not set(row["metadata"]["acceptance"]).issubset(req_ids):
                errors.append(f"card {key}: unknown requirement reference")

        parser = load_json(root / "fixtures/parser-cases.json")["cases"]
        ids = [row["id"] for row in parser]
        if len(ids) != len(set(ids)):
            errors.append("duplicate parser fixture IDs")
        recognized = set()
        for row in parser:
            if row["expected_status"] not in STATUSES or row["expected_expectation"] not in EXPECTATIONS:
                errors.append(f"invalid parser fixture expectation: {row['id']}")
            if row["expected_status"] == "recognized":
                recognized.add(row["expected_label"])
                if row["expected_label"] not in LABELS:
                    errors.append(f"unknown recognized fixture label: {row['id']}")
            elif row["expected_label"] is not None:
                errors.append(f"unrecognized fixture cannot assert a supported label: {row['id']}")
        if recognized != set(LABELS):
            errors.append("parser corpus does not cover all twelve labels")
        for row in load_json(root / "fixtures/header-edit-cases.json")["cases"]:
            before, after = row["before"], row["after"]
            if row["operation"].get("kind") == "parse_preview_copy_only" and before != after:
                errors.append(f"non-edit fixture changes source: {row['id']}")
            marker = "```suggestion"
            if marker in before and (marker not in after or before[before.index(marker):] != after[after.index(marker):]):
                errors.append(f"header edit changes protected native suggestion suffix: {row['id']}")
        for row in load_json(root / "fixtures/native-integration-cases.json")["cases"]:
            if row["product_result"] != "not_run":
                errors.append(f"unearned integration fixture result: {row['id']}")

        counter = load_json(root / "fixtures/counter-cases.json")
        for row in counter["cases"]:
            actual = counter_projection(counter["units"], row["scope"], row["columns"], row["mode"])
            if actual != row["expected"]:
                errors.append(f"counter fixture mismatch: {row['id']}")
        if exhaustive:
            for scope, mode in itertools.product(counter["exhaustive_scopes"], counter["exhaustive_modes"]):
                population = normalized_scope(counter["units"], scope)
                included = [row for row in population if mode == "all" or row["resolution"] == "unresolved"]
                for mask in range(1 << len(LABELS)):
                    columns = [label for index, label in enumerate(LABELS) if mask & (1 << index)]
                    projection = counter_projection(counter["units"], scope, columns, mode)
                    if projection["included_total"] != len(included):
                        errors.append(f"counter conservation failure: {scope}, {mode}, mask={mask}")
                        break
                    if projection["catchall"] != sum(row["root_label"] not in columns for row in included):
                        errors.append(f"counter catchall failure: {scope}, {mode}, mask={mask}")
                        break
                    for label in columns:
                        if projection["counts"][label] != sum(row["root_label"] == label for row in included):
                            errors.append(f"counter dedicated-label failure: {label}")
                            break
        predecessors = load_json(root / "references/PREDECESSOR-PACKS.json")
        if predecessors["new_proposed_range"] != ["RFC-BPR-0047", "RFC-BPR-0051"]:
            errors.append("predecessor/new allocation mismatch")
        for row in predecessors["import_order"]:
            expected = row["expected_sha256"] + "  " + row["archive"] + "\n"
            if safe_path(root, row["source_sidecar"]).read_text(encoding="utf-8") != expected:
                errors.append(f"predecessor checksum sidecar mismatch: {row['archive']}")
            if row["archive_bytes_verified_here"] is not False:
                errors.append("must not claim unavailable predecessor bytes were verified")
        if predecessors["import_order"] != manifest["predecessors"]:
            errors.append("predecessor manifest reference mismatch")
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as exc:
        errors.append(f"invalid or unreadable pack structure: {exc}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1], help="extracted RFC pack root")
    arguments = parser.parse_args()
    if sys.version_info < (3, 10):
        print("ERROR: Python 3.10 or newer is required.", file=sys.stderr)
        return 2
    errors = validate(arguments.root.resolve())
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("PASS: pack integrity, 5 RFCs, 27 cards, 8 milestones, 48 requirements and 12 labels.")
    print("PASS: fixture consistency and 32,768 normalized counter projections (4,096 subsets × 2 modes × 4 scopes).")
    print("NOT RUN: BPR implementation, live import, native UX/GitHub verification, models and predecessor ZIP payload validation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
