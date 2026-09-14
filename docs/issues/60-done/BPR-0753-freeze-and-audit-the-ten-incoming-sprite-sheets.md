---
schema: "implement-rfc.card.v1"
id: "BPR-0753"
title: "Freeze and audit the ten incoming sprite sheets"
status: "Done"
rfcs: ["RFC-BPR-0030"]
depends_on: ["BPR-0752"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"intake-contract","acceptance":["A1","A2"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Ten exact inputs and no duplicate clip","outcome":"pass","revision":"d70c595eb15c76bba88dbd8721dd1400eb32ee3e","evidence":{"kind":"command","detail":"All 55 archived checksums and ten distinct input SHA-256 values passed; audit_incoming.py fresh result equals archived raw audit. Mutable RFC-BPR-0030-intake ledger records IDs and source dimensions."}},{"criterion":"Actual grids, matte and bounded corrections","outcome":"pass","revision":"d70c595eb15c76bba88dbd8721dd1400eb32ee3e","evidence":{"kind":"observation","detail":"Ledger records five fractional grids, alpha/perimeter facts, raw idle frame-7 blink, matte/identity defects and bounded downstream repairs; installed rest structural validator passed without body check. No final visual acceptance implied."}},{"criterion":"Candidate and source/runtime separation","outcome":"pass","revision":"d70c595eb15c76bba88dbd8721dd1400eb32ee3e","evidence":{"kind":"observation","detail":"Original 1254-square rest is a prop-free candidate for a separate 1024-square derivative; all proposed crops remain unapproved and ten source PNGs remain only in immutable intake archive. Prior rights context is recorded without a new derivative verdict. Documentation gate returned DOCUMENTATION_OK."}}]
---

# BPR-0753: Freeze and audit the ten incoming sprite sheets

## Outcome

Bind exact incoming PNGs, crop proposals, alpha and identity defects. Select the high-resolution rest as the candidate master, and reuse applicable project rights context without inventing a final derivative verdict.

## Acceptance

- All ten hashes match the supplied inputs; no duplicate tool delivery becomes an extra clip.
- Actual integer/fractional grids, matte facts, idle blink and identity drift have bounded corrective tasks.
- Incoming sources remain outside runtime resources; proposed crops are labeled unapproved.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.

## Execution record

2026-09-14, producing source revision `d70c595eb15c76bba88dbd8721dd1400eb32ee3e`:
see the [mutable intake ledger](../../verification/RFC-BPR-0030-intake.md) for
all ten exact hashes/generation IDs, decoded grids, per-sheet alpha facts,
unapproved crop proposals, candidate master and bounded corrective tasks.

- `audit_incoming.py`: passed all ten incoming hashes; fresh audit equals the
  archived raw audit exactly. Independent checksum verification passed all 55
  pack entries; ten unique IDs/hashes and actual decoded grid arithmetic passed.
- Installed sprite-sheet validator: rest's explicit 1254-square layout passed
  with `--no-body-check`; this establishes no identity or matte approval.
- Agent inspection confirmed idle frame 7 closed eyes and first-frame
  chest-band/mouth/paw/proportion drift. No source bytes changed or entered
  runtime resources. Rest is a candidate; its future master target is 1024 square.
- Prior BPR-0287 authority is relevant context; exact new derivative disposition,
  master and complete catalog visual acceptance remain downstream. No human,
  native-runtime, or final RFC A1/A2 verdict is claimed.
- Documentation gate: `python3 scripts/verify_documentation.py --root .` passed (`DOCUMENTATION_OK`).
