---
schema: "implement-rfc.card.v1"
id: "BPR-0575"
title: "Anchor RFC-BPR-0022 verification and closeout"
status: "Backlog"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0573","BPR-0574","BPR-0242"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Anchor RFC-BPR-0022 verification and closeout


RFC-BPR-0022 cannot close before RFC-BPR-0008 is Auto-Verified.

## Closeout requirements

Copy the fifteen canonical acceptance criteria from the RFC verbatim into the live closeout table. For each row record the final-head direct verification surface, exact command or attended interaction, expected and observed result, durable evidence location, gate class, and producing commit.

## Acceptance

- Every imported implementation card is Done.
- RFC-BPR-0008 is Auto-Verified.
- All fifteen criteria have direct final-head evidence, including designated native and fallback GitHub-only interactions.
- No in-scope critical defect, unsupported inference, or duplicate-publication course remains.
- RFC/index/verification/board/source truth agree.
- `python3 scripts/verify_documentation.py --root .` and `git diff --check` pass.
- Only then is RFC-BPR-0022 marked Auto-Verified.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

RFC base: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

Requested RFC set: RFC-BPR-0022, RFC-BPR-0023; import position: 1 of 2.
Planned cards: BPR-0561, BPR-0562, BPR-0563, BPR-0564, BPR-0565, BPR-0566, BPR-0567, BPR-0568, BPR-0569, BPR-0570, BPR-0571, BPR-0572, BPR-0573, BPR-0574, BPR-0575.

## Closeout checklist

| Criterion | Planned check | Result | Producing commit |
|---|---|---|---|
| Every materialized current/right source line in a text file already changed by the pull request exposes the ordinary comment gesture; file eligibility, exact head, path, line, and acting identity are revalidated before any write. | AC-01 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| A selected anchor accepted by the legacy line-comment operation publishes as one native line review comment on GitHub.com and GHES, independent of the off-hunk capability state. | AC-02 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| An off-hunk anchor on a repository whose exact operation is proven supported publishes as one native line review comment and records scoped positive evidence. | AC-03 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| An off-hunk anchor proven unsupported publishes as one ordinary file-level review comment whose first paragraph is the canonical immutable permalink and whose remaining prose is byte-preserved. | AC-04 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| An unknown capability or placement attempts the intended native write; only a fully received exact unsupported rejection can authorize fallback. | AC-05 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| Timeout, cancellation, connection loss, created-but-unparsed response, generic `422`, permission failure, rate limiting, malformed response, 5xx, or ambiguous reconciliation never starts a fallback POST. | AC-06 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| One logical publication produces at most one GitHub comment across retries, process death, schema migration, and restart; a possibly transmitted native attempt is reconciled GET-only and can never coexist with an eligible fallback POST. | AC-07 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| Capability evidence is repository-, actor-, profile-, server-contract-, operation-, API-, and classifier-revision scoped, freshness-bounded, and never inferred from host name, broad GHES version, readable comments, or status code alone. | AC-08 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| BetterPullReview retains both the semantic source anchor and the honest GitHub remote subject; it renders a valid compatibility thread at its source line without claiming GitHub stored a line subject. | AC-09 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| A GitHub-only collaborator can see the linked code, read the complete prose, reply through ordinary GitHub UI, and retain that conversation after BetterPullReview is uninstalled. | AC-10 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| Deleting the local database and refreshing from GitHub reconstructs a canonical compatibility anchor and reviewer prose; invalid, ambiguous, or foreign permalinks remain honestly file-level. | AC-11 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| External edits, replies, resolution changes, and deletion reconcile by canonical GitHub identity. Removing or changing the permalink updates or removes the semantic placement according to server truth and never revives stale local placement. | AC-12 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| A body requiring line-only semantics is not silently degraded through the file representation; it receives a typed incompatibility with no POST. | AC-13 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| The complete gesture follows Qt intent -> core decision -> application host -> mutation coordinator -> GitHub adapter -> durable recovery -> server refresh -> incremental Qt projection, with no provider/widget bypass. | AC-14 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |
| Automated tests cannot reach a non-designated live target, and live native and fallback evidence is produced only through the existing attended, identity-bound mutation authority. | AC-15 in RFC-BPR-0022 verification ledger | Pending implementation and evidence | None |

No acceptance criterion is satisfied by import. Reuse passing evidence only
while its inputs are unchanged. Resume from the first planning card after a
later implementation request; re-read the live board and RFC dependencies.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
