---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-08"
title: "Preserve bytes, modes and special-file limitations"
status: "Backlog"
rfcs: ["RFC-BPR-0054"]
depends_on: ["PACK-CYCLE-06"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-06", "RC54-07"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-08: Preserve bytes, modes and special-file limitations

## Outcome and bounded work

Implement raw byte versus Git-object identities, UTF-8 byte/display mapping, CRLF/BOM/no-final-newline and mode handling. Distinguish pointer/materialized LFS content, unsupported encoding, binary, symlink/gitlink and sparse unavailable material without executing transformations.

## Acceptance and direct evidence

Byte-golden corpus, Unicode multibyte boundaries, invalid encoding, executable changes, binary/LFS and unsupported backend cases. Compare retained hashes with original bytes and ensure no lossy normalized replacement.

Criteria: RC54-06, RC54-07.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
