---
id: "RFC-BPR-0056"
title: "Frozen Feedback Rounds and Portable Handoffs"
status: "Auto-Verified"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0055"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0056: Frozen Feedback Rounds and Portable Handoffs

## Outcome and decisions

A finished round is one complete assignment assembled deterministically from the review, not one prompt per comment. Freeze exact selected item/source revisions, replacements, order, goal and constraints into a canonical payload with a digest. The user need not restate the review in a separate prompt or approve each item again merely to share it.

Copy to Clipboard and Save to File are first-class baseline delivery. Markdown is understandable by a human or any harness without BPR. A bounded review bundle additionally preserves exact structured data and selected attachments. Direct retrieval and integrated delivery consume the same frozen payload; no rich agent-only representation and no lossy human summary.

A copied/saved/retrieved round is not necessarily received, understood or completed. Delivery receipts describe observations only. Repeated export of the same round reuses its identity. Subsequent edits are successor drafts/amendments rather than mutation of an assignment already in flight.

Export only selected content and disclose channel/scope. Validate incoming bundles as untrusted data with exact hashes, bounded expansion and path checks; no automatic source writes or executable content. Over-limit reads fail or expose explicit continuation/completeness. A consumer must read the entire instruction set before implementation; silently exporting the first page is not success.

## Normative contracts

The detailed contracts [ROUNDS-AND-PORTABILITY.md](assets/bpr-review-cycles-v1/specs/ROUNDS-AND-PORTABILITY.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC56-01:** Freeze every selected note, exact suggestion, source binding, explicit action and chosen order into one complete deterministic payload.
- **RC56-02:** Frozen payloads/digests never change; identical freezes reuse identity and edits create a new round/amendment.
- **RC56-03:** Clipboard, Markdown, bundle, CLI retrieval and adapter delivery preserve the same canonical selection and byte semantics.
- **RC56-04:** A human without BPR can understand the complete Markdown assignment and proposed changes; rendering safely handles embedded fences and whitespace.
- **RC56-05:** Bundle/import validation rejects bad hashes, duplicate keys/members, traversal, symlink entries, compression abuse and invalid versions before admission.
- **RC56-06:** Unselected scratch/history/provenance, absolute local paths and credentials do not leak into default outputs.
- **RC56-07:** Copy/save/retrieval/adapter/acknowledgment observations remain distinct and do not imply delivery completion or review acceptance.
- **RC56-08:** Limits/pagination never silently drop feedback; completeness/counts/digest are explicit and oversized assignment partitioning requires a user decision.
- **RC56-09:** Interrupted/repeated freeze/export/import operations recover without partial ready state or duplicate logical round identity.
- **RC56-10:** Export policy applies consistently to all channels, including plain folders of unknown origin, without disguising a denied disclosure as a fallback.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).
