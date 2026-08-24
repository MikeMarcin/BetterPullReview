#!/usr/bin/env python3
"""Hold BPR-0417's constructed deep-conversation projection contract."""

from __future__ import annotations

import copy
import json
import re
import unittest
from collections import defaultdict
from pathlib import Path
from typing import Any


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_PATH = (
    REPOSITORY_ROOT
    / "tests"
    / "fixtures"
    / "github"
    / "conversations"
    / "deep-thread-v1"
    / "projection.json"
)
SCHEMA = "betterpullreview.github-conversation-projection-fixture.v1"
PROJECTION_FIELDS = (
    "Id",
    "ParentId",
    "Kind",
    "AuthorLogin",
    "Body",
    "Path",
    "Line",
    "OriginalLine",
    "DiffSide",
    "AnchorAvailability",
    "ReviewState",
    "Resolved",
    "Outdated",
    "Deleted",
)
VALID_IDS = (
    "review-thread:deep-thread-v1",
    "review-comment:deep-thread-v1-c01",
    "review-comment:deep-thread-v1-c02",
    "review-comment:deep-thread-v1-c02-sibling",
    "review-comment:deep-thread-v1-c03",
    "review-comment:deep-thread-v1-c04",
    "review-comment:deep-thread-v1-c04-sibling",
    "review-comment:deep-thread-v1-c05",
    "review-comment:deep-thread-v1-c06",
)
HOSTILE_IDS = {
    "hostile-absent-parent": (
        "review-thread:deep-thread-v1-absent-parent",
        "review-comment:deep-thread-v1-orphan",
    ),
    "hostile-cycle": (
        "review-thread:deep-thread-v1-cycle",
        "review-comment:deep-thread-v1-cycle-c01",
        "review-comment:deep-thread-v1-cycle-c02",
    ),
}
IDENTITY = re.compile(r"^review-(?:thread|comment):[a-z0-9][a-z0-9-]*$")


class FixtureContractError(ValueError):
    """Raised when the committed projection no longer satisfies BPR-0417."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FixtureContractError(message)


def require_exact_keys(
    value: Any, expected: tuple[str, ...], label: str
) -> None:
    require(type(value) is dict, f"{label} must be an object")
    require(
        tuple(value) == expected,
        f"{label} keys/order must be {expected!r}, found {tuple(value)!r}",
    )


def canonical_text(document: dict[str, Any]) -> str:
    return json.dumps(document, ensure_ascii=False, indent=2) + "\n"


def require_no_recorded_claims(value: Any, path: str = "$") -> None:
    if type(value) is dict:
        for key, nested in value.items():
            require(
                key not in {"recorded_from", "recorded_on"},
                f"constructed provenance cannot contain {path}.{key}",
            )
            require_no_recorded_claims(nested, f"{path}.{key}")
    elif type(value) is list:
        for index, nested in enumerate(value):
            require_no_recorded_claims(nested, f"{path}[{index}]")


def require_optional_line(value: Any, label: str) -> None:
    require(
        value is None or (type(value) is int and value > 0),
        f"{label} must be null or a positive integer",
    )


def validate_projection_nodes(
    nodes: Any, case_id: str, expected_ids: tuple[str, ...]
) -> list[dict[str, Any]]:
    require(type(nodes) is list and nodes, f"{case_id}: projection must be nonempty")
    identities: list[str] = []
    for index, node in enumerate(nodes):
        label = f"{case_id}: projection[{index}]"
        require_exact_keys(node, PROJECTION_FIELDS, label)
        identity = node["Id"]
        require(
            type(identity) is str and IDENTITY.fullmatch(identity) is not None,
            f"{label}: Id must use a stable review-thread/review-comment identity",
        )
        identities.append(identity)
        parent = node["ParentId"]
        require(
            parent is None
            or (type(parent) is str and IDENTITY.fullmatch(parent) is not None),
            f"{label}: ParentId must be null or a stable projection identity",
        )
        require(
            node["Kind"] in {"review_thread", "review_comment"},
            f"{label}: Kind is not a projection enum value used by this fixture",
        )
        for field in ("AuthorLogin", "Body", "Path"):
            require(type(node[field]) is str, f"{label}: {field} must be a string")
        require_optional_line(node["Line"], f"{label}: Line")
        require_optional_line(node["OriginalLine"], f"{label}: OriginalLine")
        require(
            node["DiffSide"] in {None, "left", "right"},
            f"{label}: DiffSide is not a github_conversation_diff_side value",
        )
        require(
            node["AnchorAvailability"]
            in {
                "current",
                "selected_comparison_mismatch",
                "selected_line_out_of_bounds",
                "outdated_revision",
                "file_level",
                "missing_side",
                "missing_current_line",
            },
            f"{label}: AnchorAvailability is not a projection enum value",
        )
        require(
            node["ReviewState"]
            in {
                "none",
                "pending",
                "commented",
                "approved",
                "changes_requested",
                "dismissed",
            },
            f"{label}: ReviewState is not a projection enum value",
        )
        for field in ("Resolved", "Outdated", "Deleted"):
            require(type(node[field]) is bool, f"{label}: {field} must be boolean")

    require(len(identities) == len(set(identities)), f"{case_id}: identities must be unique")
    require(
        tuple(identities) == expected_ids,
        f"{case_id}: stable projection identity order drifted",
    )
    return nodes


def validate_valid_case(case: dict[str, Any]) -> None:
    require_exact_keys(
        case,
        (
            "id",
            "classification",
            "provider_reachable",
            "assertion_scope",
            "root_comment_reply_edge",
            "projection",
        ),
        "valid case",
    )
    require(case["id"] == "valid-deep-wide-tree", "valid case id drifted")
    require(case["classification"] == "valid_projection", "valid classification drifted")
    require(case["provider_reachable"] is True, "valid case must be provider-reachable")
    require(
        case["assertion_scope"] == "bpr-0418-qt-rendering",
        "valid case must name its BPR-0418 rendering consumer",
    )
    edge = case["root_comment_reply_edge"]
    require_exact_keys(
        edge,
        ("comment_id", "reply_to", "projected_parent_id"),
        "valid root-comment edge",
    )
    require(edge["reply_to"] is None, "the first provider comment replyTo must be null")

    nodes = validate_projection_nodes(case["projection"], case["id"], VALID_IDS)
    by_id = {node["Id"]: node for node in nodes}
    thread_roots = [node for node in nodes if node["Kind"] == "review_thread"]
    require(len(thread_roots) == 1, "valid case must contain exactly one review-thread root")
    root = thread_roots[0]
    require(root is nodes[0] and root["ParentId"] is None, "review-thread root must be first and parentless")
    require(
        edge
        == {
            "comment_id": VALID_IDS[1],
            "reply_to": None,
            "projected_parent_id": VALID_IDS[0],
        },
        "root/null comment reply edge metadata drifted",
    )
    require(
        by_id[edge["comment_id"]]["ParentId"] == edge["projected_parent_id"],
        "a null provider replyTo must project the first comment under the thread root",
    )

    prior: set[str] = set()
    reply_depth: dict[str, int] = {}
    children: defaultdict[str, list[str]] = defaultdict(list)
    for node in nodes:
        identity = node["Id"]
        parent = node["ParentId"]
        if node["Kind"] == "review_thread":
            require(parent is None, "the valid review-thread root must remain parentless")
        else:
            require(parent in prior, f"valid parent {parent!r} must precede {identity!r}")
            parent_node = by_id[parent]
            require(
                parent_node["Kind"] in {"review_thread", "review_comment"},
                f"{identity!r} has an invalid projection parent kind",
            )
            reply_depth[identity] = (
                0 if parent_node["Kind"] == "review_thread" else reply_depth[parent] + 1
            )
            children[parent].append(identity)
        prior.add(identity)

    require(max(reply_depth.values()) >= 5, "valid chain must be at least five reply edges deep")
    sibling_depths = {
        reply_depth[child_ids[0]]
        for parent, child_ids in children.items()
        if by_id[parent]["Kind"] == "review_comment" and len(child_ids) >= 2
    }
    require(
        len(sibling_depths) >= 2,
        "valid tree must contain sibling branches at two or more reply depths",
    )


def validate_hostile_case(case: dict[str, Any]) -> list[dict[str, Any]]:
    require_exact_keys(
        case,
        (
            "id",
            "classification",
            "provider_reachable",
            "assertion_scope",
            "not_evidence_for",
            "projection",
        ),
        f"hostile case {case.get('id')!r}",
    )
    case_id = case["id"]
    require(case_id in HOSTILE_IDS, f"unexpected hostile case {case_id!r}")
    require(case["classification"] == "hostile_projection", f"{case_id}: classification drifted")
    require(case["provider_reachable"] is False, f"{case_id}: must be non-provider-reachable")
    require(
        case["assertion_scope"] == "qt_defensive_row_preservation",
        f"{case_id}: must be scoped to Qt defensive row preservation",
    )
    require(
        case["not_evidence_for"] == "github_or_provider_acceptance",
        f"{case_id}: must disclaim GitHub/provider acceptance evidence",
    )
    return validate_projection_nodes(case["projection"], case_id, HOSTILE_IDS[case_id])


def validate_absent_parent(nodes: list[dict[str, Any]]) -> None:
    identities = {node["Id"] for node in nodes}
    missing = [
        node["ParentId"]
        for node in nodes
        if node["ParentId"] is not None and node["ParentId"] not in identities
    ]
    require(
        missing == ["review-comment:deep-thread-v1-missing"],
        "hostile absent-parent case must contain exactly one absent parent",
    )


def validate_cycle(nodes: list[dict[str, Any]]) -> None:
    by_id = {node["Id"]: node for node in nodes}
    first = by_id["review-comment:deep-thread-v1-cycle-c01"]
    second = by_id["review-comment:deep-thread-v1-cycle-c02"]
    require(
        first["ParentId"] == second["Id"] and second["ParentId"] == first["Id"],
        "hostile cycle case must contain the exact two-comment cycle",
    )
    require(
        all(node["ParentId"] is None or node["ParentId"] in by_id for node in nodes),
        "hostile cycle must not also be an absent-parent case",
    )


def validate_fixture(document: Any, raw: str) -> None:
    require(type(document) is dict, "fixture must be a JSON object")
    require(raw == canonical_text(document), "fixture must use canonical two-space normalized JSON")
    require_exact_keys(document, ("schema", "provenance", "cases"), "fixture")
    require(document["schema"] == SCHEMA, "fixture schema/version drifted")
    require_exact_keys(
        document["provenance"],
        ("kind", "created_for", "intended_consumer", "recorded_evidence", "description"),
        "provenance",
    )
    require(
        document["provenance"]
        == {
            "kind": "constructed",
            "created_for": "BPR-0417",
            "intended_consumer": "BPR-0418",
            "recorded_evidence": False,
            "description": "Hand-authored deterministic github_conversation_projection data.",
        },
        "constructed provenance or intended consumer drifted",
    )
    require_no_recorded_claims(document)

    cases = document["cases"]
    require(type(cases) is list and len(cases) == 3, "fixture must contain one valid and two hostile cases")
    require(
        [case.get("id") for case in cases]
        == ["valid-deep-wide-tree", "hostile-absent-parent", "hostile-cycle"],
        "case order or identity drifted",
    )
    validate_valid_case(cases[0])
    absent_nodes = validate_hostile_case(cases[1])
    cycle_nodes = validate_hostile_case(cases[2])
    validate_absent_parent(absent_nodes)
    validate_cycle(cycle_nodes)

    all_ids = [node["Id"] for case in cases for node in case["projection"]]
    require(
        len(all_ids) == len(set(all_ids)),
        "valid and hostile projection identities must remain disjoint",
    )
    valid_ids = {node["Id"] for node in cases[0]["projection"]}
    hostile_ids = {
        node["Id"] for case in cases[1:] for node in case["projection"]
    }
    require(
        valid_ids.isdisjoint(hostile_ids),
        "hostile shapes must not be mixed into valid provider data",
    )


def load_fixture() -> tuple[dict[str, Any], str]:
    raw = FIXTURE_PATH.read_text(encoding="utf-8")
    return json.loads(raw), raw


class Bpr0417DeepConversationFixtureContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.document, self.raw = load_fixture()

    def assert_mutation_rejected(self, mutate, message: str) -> None:
        changed = copy.deepcopy(self.document)
        mutate(changed)
        with self.assertRaisesRegex(FixtureContractError, message):
            validate_fixture(changed, canonical_text(changed))

    def test_committed_fixture_satisfies_contract(self) -> None:
        validate_fixture(self.document, self.raw)

    def test_noncanonical_bytes_are_rejected(self) -> None:
        with self.assertRaisesRegex(FixtureContractError, "canonical"):
            validate_fixture(self.document, self.raw + " ")

    def test_recorded_provenance_claim_is_rejected(self) -> None:
        self.assert_mutation_rejected(
            lambda document: document["provenance"].__setitem__(
                "recorded_from", "https://example.invalid/pull/1"
            ),
            "provenance",
        )

    def test_shallower_reply_chain_is_rejected(self) -> None:
        self.assert_mutation_rejected(
            lambda document: document["cases"][0]["projection"][8].__setitem__(
                "ParentId", "review-comment:deep-thread-v1-c01"
            ),
            "five reply edges",
        )

    def test_siblings_at_only_one_depth_are_rejected(self) -> None:
        def weaken(document: dict[str, Any]) -> None:
            nodes = document["cases"][0]["projection"]
            nodes[3]["ParentId"] = "review-thread:deep-thread-v1"
            nodes[6]["ParentId"] = "review-comment:deep-thread-v1-c02-sibling"

        self.assert_mutation_rejected(weaken, "sibling branches")

    def test_forward_parent_in_valid_data_is_rejected(self) -> None:
        self.assert_mutation_rejected(
            lambda document: document["cases"][0]["projection"][4].__setitem__(
                "ParentId", "review-comment:deep-thread-v1-c04"
            ),
            "must precede",
        )

    def test_hostile_case_cannot_claim_provider_reachability(self) -> None:
        self.assert_mutation_rejected(
            lambda document: document["cases"][1].__setitem__(
                "provider_reachable", True
            ),
            "non-provider-reachable",
        )

    def test_absent_parent_shape_cannot_be_removed(self) -> None:
        self.assert_mutation_rejected(
            lambda document: document["cases"][1]["projection"][1].__setitem__(
                "ParentId", "review-thread:deep-thread-v1-absent-parent"
            ),
            "exactly one absent parent",
        )

    def test_cycle_shape_cannot_be_removed(self) -> None:
        self.assert_mutation_rejected(
            lambda document: document["cases"][2]["projection"][1].__setitem__(
                "ParentId", "review-thread:deep-thread-v1-cycle"
            ),
            "two-comment cycle",
        )

    def test_hostile_node_cannot_be_mixed_into_valid_data(self) -> None:
        def mix(document: dict[str, Any]) -> None:
            hostile = copy.deepcopy(document["cases"][1]["projection"][1])
            document["cases"][0]["projection"].append(hostile)

        self.assert_mutation_rejected(mix, "stable projection identity order")


if __name__ == "__main__":
    unittest.main(verbosity=2)
