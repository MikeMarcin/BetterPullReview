---
schema: "implement-rfc.card.v1"
id: "BPR-1770"
title: "Batch direct Home review membership and explain refresh work"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner authorization 2026-10-26 via coordinator; mandate home-requested-repairs-r1","reservation":"docs/issues/_reservations/BPR-1770--BPR-1770--0361a441a92d.json","baseline":"e6e69cc2c6a429209ac3462579a4cb685855fa86","related_cards":["BPR-1680","BPR-1694","BPR-1769"],"milestone_plan":{"id":"home-requested-repairs-20261026","path":"docs/rfcs/plans/home-requested-repairs-20261026/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a8a5d18556a0601857f63feeea8955ccd9358227","evidence":{"kind":"command","detail":"core_home_discovery and github_read_contract pass: exact user-review-requested:@me on GitHub.com/GHES3.18, matching256-byte/ten-repository planning, canonical whitelist/continuation/refusals. Existing per-request account authority checks and anonymous refusals retained."}},{"criterion":"AC2","outcome":"pass","revision":"a8a5d18556a0601857f63feeea8955ccd9358227","evidence":{"kind":"command","detail":"github_paged_read passes:9retained+30rejected candidates require4requests, repeated on2refreshes (serial baseline31; regression failed before). Batched anchored membership, viewer node/login, pagination/absence/revision/alias/cursor/duplicate/cancellation/page/shared32request bounds and REST-limit recovery tested. Native+CLI composed discovery and Home waiting pass; native save8vs9requests, explicit refresh5vs6, restart13vs14. No private GHES latency retest."}},{"criterion":"AC3","outcome":"pass","revision":"a8a5d18556a0601857f63feeea8955ccd9358227","evidence":{"kind":"command","detail":"Provider/host/core/logger/export checks pass, including real composed diagnostic file and identifying/anonymized export allowlists. Appended safe counts and closed completeness reasons preserve both historical line forms. Final retained WaitingLimited→summary_limit correction passes pager+native composition rechecks2/2."}},{"criterion":"AC4","outcome":"pass","revision":"a8a5d18556a0601857f63feeea8955ccd9358227","evidence":{"kind":"artifact","detail":"frontend-design skill loaded. qt_release_maintenance_scale_100 passes; real offscreen export-options render inspected with exact label fully fitting and default unchecked. Tooltip and summary name supplemental in-memory verbose records; retention/privacy/default behavior unchanged."}},{"criterion":"AC5","outcome":"pass","revision":"a8a5d18556a0601857f63feeea8955ccd9358227","evidence":{"kind":"command","detail":"Final focused integration15/15 passes76.97s; post-review affected checks2/2 pass33.61s. Scoped31file clang-format22.1.8 and focused7production-unit tidy pass (final pager rerun clean). Both architecture boundaries pass; documentation gate and67documentation tests pass. Core-only configure/build succeeds; full run324/329 passes. Five unrelated gates fail: baseline format, MSVC guard, isolation structure/manifest and registry; minimal baseline export tests and unchanged CMake inputs reproduce causes. See docs/verification/BPR-1770.md; no unrelated fixes or acceptance gaps hidden."}}]
---

# Batch direct Home review membership and explain refresh work

The owner authorized all three requested-review performance repairs and asked
that the export checkbox say “Include verbose logs from the last 3 minutes”.
The supplied private diagnostic export showed 37 requested-discovery requests
and about 27 seconds, with no timeout, failure or lane wait; its composition
cannot be reconstructed as a particular count of team-only candidates.

## Acceptance

- AC1: Search uses the direct-user qualifier supported by GitHub.com and the
  selected GHES contract; whitelist, query bounds and repository batching agree.
  Search hits never establish membership or replace exact account authority.
- AC2: Unknown requested candidates use bounded GraphQL membership batches,
  combining identity anchor and first roster page. Absence requires exhaustive
  pagination, with identity/revision, alias, cancellation and size/work bounds.
  Supported fallback stays safe; only direct matches receive optional summaries.
  Repeated refreshes revalidate negative membership. Existing full Home anchor
  recovery uses the narrow membership projection.
- AC3: Standard and anonymized exports carry closed counts for candidates,
  retained reuse, direct matches, rejections and roster recovery, plus the exact
  completeness reason through provider, host and core. Old log lines still parse;
  bounded formatter/parser and export allowlists reject extra content.
- AC4: Checkbox plainly names supplemental in-memory verbose logs from the last
  three minutes, with unchanged default, privacy and retention behavior.
- AC5: Meaningful regression and composed transport/request-count tests pass,
  alongside focused security, format/tidy, core/boundary and documentation checks.

## Implementation plan

One milestone: establish a failing request-count regression; add a closed
membership query and strict paginated parser; use it for requested candidates
and recovery; carry safe discovery counts; update wording; verify and self-review.
No private-server retest or native performance measurement is claimed.

## Verification and execution record

Implemented at `a8a5d18556a0601857f63feeea8955ccd9358227`. [Verification](../../verification/BPR-1770.md)
records transport bounds, safe counters, native export-dialog wording and the
five baseline gate failures. The final focused integration run passed 15/15;
the final retained-summary diagnostic correction passed its two affected checks.

The nine-retained/thirty-rejected regression fell from 31 serial requests to
four and repeats four on the next explicit refresh. The composed native course
uses 8 requests for saving a repository (previous 9), 5 for explicit Refresh
(previous 6), and 13 for restart (previous 14). These fixture counts are not a
measurement of the owner's private server.

No human judgement, independent review, installation or release acceptance was
required or claimed for this bounded repair. No attended candidate or server
was created.
