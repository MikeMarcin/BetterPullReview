---
schema: "implement-rfc.card.v1"
id: "BPR-1185"
title: "Finish documentation, examples and installed-package verification"
status: "Ready"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: ["BPR-1182", "BPR-1183", "BPR-1184"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC53-10", "RC60-01", "RC60-02", "RC60-03", "RC61-01", "RC61-10"], "pack_alias": "PACK-CYCLE-43", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-43** (live `BPR-1185`, allocated at import).

# BPR-1185: Finish documentation, examples and installed-package verification

## Outcome and bounded work

Update current product/domain/git/ledger/round-trip/AI/security/storage/build docs and user help. Publish exact supported CLI/schema/skill/capability guidance and limitations. Re-run one integration-focused package course, not every historical project test indiscriminately.

## Acceptance and direct evidence

Native documentation structure check, runnable help/examples, installed agent-free/automation-off evidence, complete per-criterion ledger with open gates explicit and no accidental changes to settled predecessor status.

Criteria: RC53-10, RC60-01, RC60-02, RC60-03, RC61-01, RC61-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Input from BPR-1182 (2026-10-04): native runs owed, not gating

[[BPR-1182]] closed with these runs recorded as owed, at the integrator's
direction. They belong with this card's installed-package course:

- Windows (`bpr-main-win`): the six `dev` rows, the five agent-free rows,
  the native two-round course on the desktop, and the agent-free WiX MSI
  installed over a full one and back with the profile's cycles reopening.
  The exact commands are in the ledger's BPR-1182 section of
  [RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).
- Linux: the `xcb` two-round course once the owner clears the shared WSLg
  desktop, and the agent-free DEB on an Ubuntu 24.04 station.
- macOS: the desktop and editor rows on cocoa, which need the keyboard
  (nobody else using the machine).
