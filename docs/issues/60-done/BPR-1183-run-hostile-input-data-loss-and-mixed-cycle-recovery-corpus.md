---
schema: "implement-rfc.card.v1"
id: "BPR-1183"
title: "Run hostile input, data-loss and mixed-cycle recovery corpus"
status: "Done"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0056", "RFC-BPR-0060"]
depends_on: ["BPR-1152", "BPR-1160", "BPR-1165", "BPR-1178", "BPR-1181"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC54-03", "RC56-05", "RC60-05", "RC60-06"], "pack_alias": "PACK-CYCLE-41", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC54-03","outcome":"pass","revision":"60c438c02","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1183's share, macOS arm64 Debug tour-cli graph. application_review_cycle_recovery_corpus (shipped bpr and the composed desktop as real processes): a root replaced at its location, a link there to another folder and a moved root are each refused with nothing recorded, in words that say retrying will not help (the terminal offers no Next; the window says the same through Capture revision); the folder now there opens only as its own cycle; moved back, the root captures again. Defect fixed: root_changed said 'replaced while it was read' with a retry that is refused again. Negative control: the old words fail the course. Descendant paths and link escapes are BPR-1148's and BPR-1181's evidence. Explicit rebind has no product surface and stays open (follow-up reported)."}},{"criterion":"RC56-05","outcome":"pass","revision":"60c438c02","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1183's share at the seams that exist: no bundle import is shipped, so the hostile-archive corpus is BPR-1160's and BPR-1162's core, host and fuzz evidence; core_review_cycle_round, review_cycle_bundle and review_cycle_attach passed again at 60c438c02. Through bpr: duplicate JSON keys refused; a bundle saved under a full disk or killed mid-save is whole or absent with no temporary file left; case-colliding paths (src/Cache.cpp, src/cache.cpp on a case-sensitive APFS image) are carried as two paths and make no aliasing bundle members."}},{"criterion":"RC60-05","outcome":"pass","revision":"60c438c02","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1183's share, macOS arm64. Through bpr: the same response twice is one response, another under its identifier is conflict (reused_identifier), duplicate keys are invalid_input, a repeated operation ID is conflict, each with nothing changed; a reply to a lost round is refused (wrong_digest) in the new cycle that took its name; a corrupted round writes, prints and grants nothing and stays listed; a corrupted or missing pin never shows damaged or live bytes in its place. Execution and forged-authority refusals are BPR-1181's unchanged evidence."}},{"criterion":"RC60-06","outcome":"pass","revision":"60c438c02","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1183's share, macOS arm64. 30 SIGKILLs across capture, notes, goals, freezes, responses and saves, the desktop owner killed mid-write (bpr exits 6 uncertain) and a 64 KiB file-size limit (capture, response, freeze, save) each leave the cycle before or after, the store passing integrity_check with no dangling object, no claimed delivery, the earlier round byte for byte and the second cycle unchanged; repeats are applied once. A store SQLite cannot read is now store_damaged, left as it is (defect fixed: was 'a different version'); a lost store reconstructs nothing; the reviewer's copy restores it; a schema-9 bpr refuses the upgraded store without writing it, and it and an agent-free bpr read F1 byte for byte through the current desktop. Negative controls: four mutations each fail the course (and the storage contract for the store). Owner-channel bounds are BPR-1181's evidence."}},{"criterion":"BPR-1183:boundary","outcome":"pass","revision":"60c438c02","evidence":{"kind":"command","detail":"Focused selection 24 of 24 serially in build/bpr1183 (the corpus, security, two-round, parity, desktop and handoff courses, cycle_cli_public_path, core and host review-cycle rows, sqlite_review_cycle_storage, qt_review_cycle_view_1_0, round-trip composition contract with the three pins printed unchanged, architecture_boundaries, test_network_isolation_manifest, coding_format_check); clang-format 22 and 23 agree; clang-tidy no finding on a changed line; DOCUMENTATION_OK and verify_documentation_tests OK. No independent reviewer ran (coordinator-implementer). Linux and Windows owed to stations, commands in the ledger."}}]
---

Pack alias: **PACK-CYCLE-41** (live `BPR-1183`, allocated at import).

# BPR-1183: Run hostile input, data-loss and mixed-cycle recovery corpus

## Outcome and bounded work

Combine filesystem, archive, canonical payload, source correspondence, response and publication failure cases at production integration seams. Verify containment, retained evidence, truthful unknowns and unrelated PR/cycle isolation.

## Acceptance and direct evidence

Every failure scenario in verification/SCENARIOS.json has a concrete test/check/result. Include disk full, corrupt retained bytes, stale drafts, rebind, duplicate JSON, case collisions, missing public/private history and uncertain delivery.

Criteria: RC54-03, RC56-05, RC60-05, RC60-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Result (2026-10-04)

Promoted at `e310cc48a`, claimed at `6a3047c59`, landed as `60c438c02`.
Owned paths, decisions, every command and its result, the negative controls
and the scenario-by-scenario map are in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1183-the-hostile-input-data-loss-and-mixed-cycle-recovery-corpus-2026-10-04).

- `application_review_cycle_recovery_corpus` drives the shipped `bpr` and
  the reviewer's desktop as real processes through:
  - 30 kill points across capture, notes, goals, freezes, responses and
    saves;
  - the owner killed mid-write;
  - a full disk;
  - damaged and missing retained material;
  - a damaged and a lost store;
  - stale drafts, a writer that never stops, and replaced, linked and moved
    roots;
  - duplicates;
  - case collisions on a case-sensitive volume;
  - given them, a schema-9 `bpr` and an agent-free `bpr` on one profile.
  After each, the store is whole, the cycle is before or after, the earlier
  round reads byte for byte, and a second cycle never changes.
- Four defects were fixed at their source. A damaged store was reported as
  another version's. A replaced root was "replaced while it was read", with a
  retry that is refused again. Missing retained bytes were "no file". A
  repeat of an applied command said its ID named another request.
- Every scenario in the pack's `SCENARIOS.json` is mapped to its check and
  result; the failure scenarios each have one.

Open, not gating this card: a moved or replaced root has no explicit rebind in
the terminal or the window (the core has one, BPR-1148). A moved project keeps
its cycle only by moving back. The follow-up card is for the main integrator
to allocate. Linux and Windows station runs are owed; their commands are in
the ledger.
