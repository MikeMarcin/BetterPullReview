---
schema: "implement-rfc.card.v1"
id: "BPR-0575"
title: "Anchor RFC-BPR-0022 verification and closeout"
status: "Ready"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0573","BPR-0574","BPR-0242"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"RFC-AC-01","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-02","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-03","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-04","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-05","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-06","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-07","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-08","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-09","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-10","outcome":"pending","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"GitHub-only course accepted; actual desktop uninstall or explicit owner scope amendment pending."}},{"criterion":"RFC-AC-11","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-12","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-13","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-14","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"RFC-AC-15","outcome":"pass","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"Exact criterion surface and inherited unchanged evidence reconciled in docs/verification/RFC-BPR-0022.md; final GET-only continuity in m4-final-continuity.json."}},{"criterion":"independent-acceptance","outcome":"pending","revision":"0f3188093c846cbbb43f624dd48c817e0ea4eec4","evidence":{"kind":"observation","detail":"No separate independent report supplied. Selected recipe prohibits additional reviewer agents. Explicit owner-waiver proposal remains undecided."}}]
---

# Anchor RFC-BPR-0022 verification and closeout

Current round: both final scope decisions were accepted at hand-back 29.
[Mandate r4](../_checkpoints/BPR-0575/m4-r4-amendments.md) records the exact
AC-10 evidence substitution and independent-review waiver. Earlier pending
statements below describe their original rounds. The card is eligible for
final engineering closeout; actual desktop uninstall remains unobserved.


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
| Every materialized current/right source line in a text file already changed by the pull request exposes the ordinary comment gesture; file eligibility, exact head, path, line, and acting identity are revalidated before any write. | AC-01 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS automated/rendered; final composed candidate confirms native and file selection | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| A selected anchor accepted by the legacy line-comment operation publishes as one native line review comment on GitHub.com and GHES, independent of the off-hunk capability state. | AC-02 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS fixtures; native GHES unobserved per owner amendment | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| An off-hunk anchor on a repository whose exact operation is proven supported publishes as one native line review comment and records scoped positive evidence. | AC-03 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS supported-path fixtures under r3; native success unobserved and the four historic witnesses remain GET-only | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| An off-hunk anchor proven unsupported publishes as one ordinary file-level review comment whose first paragraph is the canonical immutable permalink and whose remaining prose is byte-preserved. | AC-04 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS fixtures and actual file comment4116623474 at RIGHT62, exact prefix/prose; GET-only recovery at0f3188093 | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| An unknown capability or placement attempts the intended native write; only a fully received exact unsupported rejection can authorize fallback. | AC-05 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS prospective r3 exact production selection and 29 refusal/restart courses; earlier live witnesses remain GET-only | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| Timeout, cancellation, connection loss, created-but-unparsed response, generic `422`, permission failure, rate limiting, malformed response, 5xx, or ambiguous reconciliation never starts a fallback POST. | AC-06 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS bounded hostile courses; live indeterminate native journal has no fallback | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| One logical publication produces at most one GitHub comment across retries, process death, schema migration, and restart; a possibly transmitted native attempt is reconciled GET-only and can never coexist with an eligible fallback POST. | AC-07 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS66 deterministic before/after interruptions with3 fresh sessions each; no OS-kill/power-loss claim | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| Capability evidence is repository-, actor-, profile-, server-contract-, operation-, API-, and classifier-revision scoped, freshness-bounded, and never inferred from host name, broad GHES version, readable comments, or status code alone. | AC-08 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS scoped invalidation/admission plus revision2 cache and exact PR2/actor/profile/source refusal tests | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| BetterPullReview retains both the semantic source anchor and the honest GitHub remote subject; it renders a valid compatibility thread at its source line without claiming GitHub stored a line subject. | AC-09 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS composed replay and actual FILE rehydration at Markdown62;3 scales/3 themes retained | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| A GitHub-only collaborator can see the linked code, read the complete prose, reply through ordinary GitHub UI, and retain that conversation after BetterPullReview is uninstalled. | AC-10 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS accepted GitHub-only inspection/reply and signed-app reply refresh; PENDING actual desktop uninstall or explicit scope amendment | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| Deleting the local database and refreshing from GitHub reconstructs a canonical compatibility anchor and reviewer prose; invalid, ambiguous, or foreign permalinks remain honestly file-level. | AC-11 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS fresh-host fixture with no DB or receipt; not an actual uninstall | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| External edits, replies, resolution changes, and deletion reconcile by canonical GitHub identity. Removing or changing the permalink updates or removes the semantic placement according to server truth and never revives stale local placement. | AC-12 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS automated fixtures plus signed-app read of external reply4116804415 at canonical root/line62 | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| A body requiring line-only semantics is not silently degraded through the file representation; it receives a typed incompatibility with no POST. | AC-13 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS typed incompatibility and no-write assertions | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| The complete gesture follows Qt intent -> core decision -> application host -> mutation coordinator -> GitHub adapter -> durable recovery -> server refresh -> incremental Qt projection, with no provider/widget bypass. | AC-14 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS final source; native2/file1/stored2,3 journal opens,2 session constructions asserted | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |
| Automated tests cannot reach a non-designated live target, and live native and fallback evidence is produced only through the existing attended, identity-bound mutation authority. | AC-15 in RFC-BPR-0022 ledger; exact command/surface/expected/observed there | PASS guards and exact attended fallback plus GET-only recovery; Windows gaps retained | M1–M3; D target0ff88046, profile6526f525, diagnostics699c921c, classifierdde294fb; m4-evidence.json and m4-r3-classifier-evidence.json |

Import supplied no product acceptance. A–C now have direct evidence and are
landed. D replays affected public/authority paths and reuses unchanged evidence.
BPR-0573 is Done after exact human acceptance and canonical signed-app reply refresh.
This card is prepared for human-review; actual-uninstall and independent-acceptance dispositions remain pending.
The accepted r3 owner amendment permits closeout ahead of Deferred RFC0020/0021
once all remaining0022 requirements pass. See the [D frontier](../_checkpoints/BPR-0575/m4-frontier.md)
for exact remaining work and the prepared candidate.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

## Mandate r3 decisions

The [accepted owner amendments](../_checkpoints/BPR-0575/m4-r3-amendments.md)
supersede the earlier pending-decision statements. AC-03 uses the verified
supported-path fixtures with native success unobserved. The exact prospective
classifier2 contract and the narrow0020/0021 ordering exception are approved.
Actual fallback, GET-only recovery and accepted collaborator continuity are observed.
Actual-uninstall evidence and independent acceptance remain open; BPR-0573 is now Done.


## Actual fallback and GET-only recovery

The exact signed app Published once at RIGHT62 and created file comment4116623474.
Receipt and read placeholder corrections at24b7f66aa/0f3188093 passed focused checks.
A fresh signed course reopened PR2 without Publish or Retry, recovered only that
sixth journal to succeeded_fallback, and rendered the correct semantic line62.
All five earlier journals and the sole remote root remain unchanged. Normal
quit/detach passed106.86s. [Direct recovery evidence](../_checkpoints/BPR-0575/m4-r3-get-recovery-success.json)
and the verification ledger bind these observations. This does not finish0573
or supply human/independent acceptance.


## Final engineering round — mandate r3

Assignment `/root/rfc0022_m4_final`, start `5a20a1188`, unchanged production
source `0f3188093`. BPR-0573 finished at `71e28c464`.
[Final continuity](../_checkpoints/BPR-0575/m4-final-continuity.json) binds the
accepted human decision and independently retrieved external reply, signed-app
read/render at line62, six unchanged journals, and normal69.39s lifecycle.
No new publication occurred. All other canonical rows retain their unchanged
passing evidence and accepted r2/r3 scope.

Actual desktop uninstall remains unobserved. Independent technical acceptance
remains separately required; the selected coordinator-implementer recipe forbids
extra reviewer agents and no independent report was supplied. Two explicit owner
proposals preserve those requirements until accepted. They change acceptance
only if accepted; they never become runtime observations or an independent report.
RFC status stays Proposed and final main integration remains with the coordinator.

Prepared [uninstall evidence amendment](../_checkpoints/BPR-0575/m4-final-uninstall-proposal.md)
and [independent-review waiver](../_checkpoints/BPR-0575/m4-final-independent-proposal.md)
each ask one separate Accept/Reject judgment. [Verification](../_checkpoints/BPR-0575/m4-final-verification.json)
records four focused checks, network manifest, documentation gate,64 validator
tests and whitespace passing. No independent unattended work remains before those
decisions; the existing source and four earlier accepted verdicts remain intact.
