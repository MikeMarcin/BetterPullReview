---
schema: "implement-rfc.card.v1"
id: "BPR-0574"
title: "Reconcile RFC-BPR-0022 documentation and simplify the implementation"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0572"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ff0f7711eae2f9e2a57bd919810d8147b2b4cb06","evidence":{"kind":"observation","detail":"Owning round-trip and provider boundaries distinguish semantic and remote subject and single state machine."}},{"criterion":"AC2","outcome":"pass","revision":"ff0f7711eae2f9e2a57bd919810d8147b2b4cb06","evidence":{"kind":"observation","detail":"Scope/classifier revisions/migration/cancellation/limits/mixed-client/database-loss/support documented."}},{"criterion":"AC3","outcome":"pass","revision":"ff0f7711eae2f9e2a57bd919810d8147b2b4cb06","evidence":{"kind":"observation","detail":"Current docs consistently say production unsupported tuple absent and generic422 cannot enable fallback."}},{"criterion":"AC4","outcome":"pass","revision":"ff0f7711eae2f9e2a57bd919810d8147b2b4cb06","evidence":{"kind":"observation","detail":"Bounded simplification review confirms centralized policy, narrow ports and changed-root parsing outside paint/scroll; stale docs removed, mechanical format only."}},{"criterion":"AC5","outcome":"pass","revision":"ff0f7711eae2f9e2a57bd919810d8147b2b4cb06","evidence":{"kind":"observation","detail":"Final composed2/2, authority checks, strict scoped tidy,7-file format, Mac manifest+66 tests, documentation+64 tests and whitespace pass."}}]
---

# Reconcile RFC-BPR-0022 documentation and simplify the implementation

Update the owning product, architecture, provider, review-round-trip, diff, UX, security, verification, fixture, and operator documents, then remove duplicated or representation-leaking machinery.

## Acceptance

- Semantic anchor versus remote subject and the fallback state machine are documented once at their owning boundaries.
- Capability scope, classifier revision, migration, cancellation, limits, mixed-client, database-loss, and support diagnostics are explicit.
- No stale document implies all line comments are native or that generic 422 authorizes fallback.
- Simplification removes duplicated policy, broad capability access, full-state resets, and unnecessary parsing on paint/scroll paths.
- Focused tests, documentation structure gate, and `git diff --check` pass after simplification.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

## Accepted D reconciliation — mandate r2

The owning round-trip document defines one policy/state machine, codec grammar,
schema4 migration, capacity, scope, cancellation and restart contract. Provider
wire/classifier documentation preserves the exact missing production tuple and
anchor-scoped cache. Product/system/diff/UX/security/threat/operator documents,
fixture catalog, master matrix and release gates now agree and link those owners.
The stale milestone-A “preparatory” wording is removed. README names current
availability without claiming live acceptance. The final15-row ledger records
expected and observed results, command/surface, gate and exact producing evidence.

Simplification self-review covered core policy, the serialized provider/SQLite
boundary, changed-root placement cache and incremental Qt projection. Existing
A–C code already centralizes policy and parsing; no additional behavior refactor
was justified. The only D code cleanup is mechanical formatting in the touched
application TU. No broad capability, parser on paint/scroll, duplicate policy,
or full-state model reset was introduced. No independent review is claimed.

Production candidate0ff88046 passes the actual native/file public course and
Qt1002/2 (17.79s), with unchanged M1–M3 evidence explicitly reused. The exact
sealed Release capsule passes build/inventory/signature/mounted preflight.
Manifest candidatebd9eb897 preserves unqualified Windows names as pending and
passes the Mac gate and66 guard tests; the separate491-row live graph passes
its exact one-lane check. Documentation passes and64 validator tests pass;
`git diff --check` is clean. [D evidence](../_checkpoints/BPR-0575/m4-evidence.json)
records commands, log hashes and corrected failures. This card completes only
independent documentation/simplification, not0573 live acceptance or RFC closeout.
