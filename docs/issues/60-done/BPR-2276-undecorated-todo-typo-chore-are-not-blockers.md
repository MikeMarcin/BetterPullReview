---
schema: "implement-rfc.card.v1"
id: "BPR-2276"
title: "Undecorated todo, typo and chore comments are not blockers"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"change","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2276--BPR-2276--cc0cf2677f34.json","decided_by":"owner, 2026-11-18"}
---

# BPR-2276: Undecorated todo, typo and chore comments are not blockers

## Owner decision (2026-11-18)

"Nothing is a blocker unless we don't want the PR to merge without it fixed."

## Change

The label catalog gave `todo`, `typo` and `chore` a blocking weight
([[BPR-2264]], [[BPR-2267]]). They now weigh non-blocking, as `suggestion`
does; `issue` still blocks. A written `(blocking)` still makes any label
block. Their guidance no longer reads as required: `necessary_change` and
`required_before_acceptance` become `asks_for_change` and
`asks_for_process_work`, and the catalog descriptions, the review-feedback
guidance and the Comment labels Feature say the same.

## Acceptance

- AC1. An undecorated `todo`, `typo` or `chore` counts 0 blockers.
- AC2. `todo (blocking)` counts 1 blocker and keeps its decoration when the
  agent's suggested comment is drafted; `todo (non-blocking)` drops it.
- AC3. `issue` is unchanged: it blocks undecorated, and `issue (blocking)`
  drops its decoration.
- AC4. No shipped prompt, catalog text or guidance name tells the agent that
  a todo, typo or chore blocks or is required.
- AC5. Focused tests, `architecture_boundaries`, `coding-format-check` and
  `python3 scripts/verify_documentation.py --root .` pass.

## Evidence (336088c3a)

- AC1, AC2: `core_review_preflight` counts `todo:`, `typo:` and `chore:`
  drafts as 0 blockers and 3 guidance rows, `todo (blocking):` as 1 pending
  blocker and `todo (non-blocking):` as none, and a published
  `todo (blocking):` among my blockers. `core_agent_comment_suggestion`
  keeps `todo (blocking)` in a drafted comment and drops
  `todo (non-blocking)` and `typo (non-blocking)`. Passed.
- AC3: `core_feedback_convention` keeps `issue` blocking on its own and
  pins every catalog weight; the pack's `LABEL-CATALOG.json` is revision 2
  with the renamed guidance and `verify_pack.py` passes. Passed.
- AC4: `application_writing_delivery` pins review-feedback-v3 to its text
  and checks that Comment labels (v2) says only `issue` blocks and never
  "necessary". Passed.
- Also passed: `qt_review_submission_editor_scale_{100,125,200}`,
  `qt_feedback_label_settings`,
  `application_review_submission_composition_public_path`.
- AC5: `architecture_boundaries`, `architecture_boundary_contract`,
  `coding-format-check` and `python3 scripts/verify_documentation.py --root .`
  passed on the lane working tree.
