---
schema: "implement-rfc.card.v1"
id: "BPR-1399"
title: "Correct merge-gates pack critical-review counterexamples"
status: "Done"
rfcs: ["RFC-BPR-0068", "RFC-BPR-0069", "RFC-BPR-0070"]
depends_on: ["BPR-1392", "BPR-1395", "BPR-1398"]
priority: "P1"
metadata: {"mandate_revision":"merge-gates-r1","draft_pack":"merge-gates-and-review-waiting-v1","rfc_base_sha":"9857d596769468c0585c475467175e3998a33530","reviewed_candidate":"15365e9df15ff51b366f9735c887dccdd80cae9e","owner":"/root/review_corrections"}
acceptance: [{"criterion":"C01","outcome":"pass","revision":"eae174cb95e7b66c97739b0a8dd1e6c5ac71af2b","evidence":{"kind":"observation","detail":"Page/byte counterexamples repaired; final provider, native/CLI compiler and paged/anonymous public replay5/5 pass; composed Overview/Home3/3.","refs":["docs/issues/_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json","docs/issues/_checkpoints/merge-gates-and-review-waiting-v1/post-review-handoff.json"]}},{"criterion":"C02","outcome":"pass","revision":"eae174cb95e7b66c97739b0a8dd1e6c5ac71af2b","evidence":{"kind":"observation","detail":"Fresh and complete-empty REST fallback replace prior code-owner requests without inventing origin; direct provider/window/Home mapping pass.","refs":["docs/issues/_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json","docs/issues/_checkpoints/merge-gates-and-review-waiting-v1/post-review-handoff.json"]}},{"criterion":"C03","outcome":"pass","revision":"eae174cb95e7b66c97739b0a8dd1e6c5ac71af2b","evidence":{"kind":"observation","detail":"FailedChecks-only codec/host and separate-process SQLite restart retain last-known freshness; existing Home reason includes Last known without other known fields.","refs":["docs/issues/_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json","docs/issues/_checkpoints/merge-gates-and-review-waiting-v1/post-review-handoff.json"]}}]
---

# Correct merge-gates pack critical-review counterexamples

The authorized independent critical reviewer reproduced three blocking failures
on the integrated candidate despite passing existing provider/window tests.
Historical Done cards and their evidence remain intact; this follow-up owns the
correction round under the existing mandate.

## Acceptance

- C01 / RFC-BPR-0068 AC04: bounded optional checks/status reads cannot consume
  capacity required to read otherwise available mandatory PR content/search.
  Preserve page/byte ceilings, cancellation and exact authority refusal.
- C02 / RFC-BPR-0069 AC01/AC05: a fresh complete REST actor roster replaces a
  prior same-head code-owner roster when richer GraphQL data is unavailable.
  Keep code-owner richness qualified; a complete empty fallback clears requests.
- C03 / RFC-BPR-0070 AC04: codec, separate-process SQLite restart and queue-host
  restoration preserve a partial failed check as last known when no request,
  assessment or complete check set was observed. Home exposes that qualification.

## Engineering plan

Exercise direct failing regressions first, repair each boundary, run focused
provider/host/codec/storage/Home checks and one composed integration pass. Record
the independent findings and evidence-backed dispositions in the pack checkpoint
and ledgers. Commit explicit paths, preserve unrelated state, and hand the exact
fixed candidate to the coordinator for narrow counterexample verification.
No new human criterion or visual layout change is required.

## Final independent review consumption

The coordinator supplied and consumed `/root/critical_review`’s narrow independent
confirmation (`rfc-reviewer-critical`, `gpt-6-astra`, `xhigh`) on
`fea984d3148bbdab37ee8606b0cbc3121d6635a4`. C01/C02/C03 are resolved,
with no remaining blocking or nonblocking findings in the corrected counterexamples.
The reviewer’s independent probes and focused 5/5 pass are retained in
[the review record](../_checkpoints/merge-gates-and-review-waiting-v1/critical-review-dispositions.json).
[Final pack closeout](../_checkpoints/merge-gates-and-review-waiting-v1/pack-closeout.json)
accepts the authorized pack under `merge-gates-r1`; earlier pending-review notes
and original failure evidence remain historical. Existing qualification limits
and unrelated obligations are unchanged. All reviewer/engineering commands are
quiescent; no broad tests or product changes were made for this final bookkeeping.
