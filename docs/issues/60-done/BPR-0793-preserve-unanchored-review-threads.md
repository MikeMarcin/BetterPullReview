---
schema: "implement-rfc.card.v1"
id: "BPR-0793"
title: "Preserve review threads with unavailable line anchors"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0792"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0d9f3d71","evidence":{"kind":"artifact","detail":"Bounded in-memory gh GraphQL aggregate as MikeMarcin: 19 threads, 15 LINE null/null roots; external metadata-only record thread-diagnosis-d11.json SHA-256 6822f4ed08d10de67f3523e3385b66998056e6423aeff3eb2017f2777158b63c. No private response content retained."}},{"criterion":"AC2","outcome":"pass","revision":"0d9f3d71","evidence":{"kind":"command","detail":"Focused paged parser, fixture normalizer, core validator and composed host projection tests pass 4/4; strengthened identity cases recheck 2/2. Existing malformed negative cases remain passing. build/bpr0793-focused-tests.log and build/bpr0793-focused-recheck.log."}},{"criterion":"AC3","outcome":"pass","revision":"0d9f3d71","evidence":{"kind":"command","detail":"Composed GitHub.com public path 1/1, documentation gate DOCUMENTATION_OK, tests-disabled Release Mac build, clang-format dry-run, git diff --check. Logs build/bpr0793-composed-tests.log, build/bpr0793-final-doc-gate.log, build/bpr0793-product-build.log. Independent critical review found no critical/high defect."}},{"criterion":"AC4","outcome":"pass","revision":"0d9f3d71","evidence":{"kind":"observation","detail":"One tests-disabled CLI-profile read of EpicGames/UnrealEngine#1773 reached Pull request ready with 1,523 files and 19 threads. First two selected files refused selected_file_stale with no exact epoch pair and no readable diff: next explicit metadata-only refusal. External record product-read-d11.json SHA-256 65b7c5adda82d1431335176db3491b55ae0785c1ee07d03a5c6c8d600ad47916. gh account MikeMarcin before/after; no live PR writes."}}]
---

# Preserve review threads with unavailable line anchors

The read-only tests-disabled CLI-profile attempt at the exact owner-retained
`EpicGames/UnrealEngine#1773` passed REST commit and file paging, then refused
`malformed_response` at `ListReviewThreads` before publishing a file list or
selected diff. BPR-0643 and BPR-0792 are Done for their bounded outcomes and
remain terminal. This is a separate RFC-BPR-0013 follow-up under mandate r13.

## Acceptance

- Attribute the failure with a bounded read through the existing selected
  `gh` account. Record only fixed aggregate metadata outside the checkout;
  retain no private response body, path, comment, thread ID, cursor or token in
  repository files or logs.
- Preserve a GraphQL `LINE` thread whose nullable `line` and `originalLine`
  are both absent with its exact remote subject, thread/comment identities,
  path, diff side and unavailable source-line anchor. Do not derive a line
  from comments or relabel the thread as file-level. Current and outdated
  threads with coordinates keep their existing semantics. Continue to refuse
  malformed identities, invalid line values, file-subject coordinates,
  inconsistent pages, duplicate records and resource limits atomically.
- Focused parser/core/projection tests and one composed read-path check pass,
  along with the documentation gate and a selected integration check.
- Attempt one tests-disabled Mac product read of the exact target. Record a
  visible correct file list and readable selected diff, or the next explicit
  metadata-only refusal. Do not write to the live pull request, change the
  global `gh` account, transfer a credential, or change a device session.

## Evidence

The bounded, in-memory `gh` diagnostic as `MikeMarcin` found 19 GraphQL review
threads, including 15 `LINE` threads whose root `line` and `originalLine` were
both null (14 outdated, one current). The fixed aggregate-only record is outside
the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0793-evidence-2026-09-13/thread-diagnosis-d11.json`
(SHA-256 `6822f4ed08d10de67f3523e3385b66998056e6423aeff3eb2017f2777158b63c`).
No response body, path, comment, thread ID, cursor or token was retained.

The parser, fixture normalizer and application validator now keep these exact
remote `LINE` threads with unavailable anchors. Focused parser, core and host
projection tests pass 4/4 in `build/bpr0793-focused-tests.log`; the strengthened
identity cases pass 2/2 in `build/bpr0793-focused-recheck.log`. The composed
GitHub.com read path passes in `build/bpr0793-composed-tests.log`; the
documentation gate passes in `build/bpr0793-doc-gate.log`. The tests-disabled
Release Mac app builds in `build/bpr0793-product-build.log`. Independent critical
review found no critical or high changed-surface finding.

One tests-disabled app read through the authorized CLI profile reached **Pull
request ready** for the exact target and displayed 1,523 actionable files and
19 review threads. The first two selected comparisons each refused
`selected_file_stale`; the UI reported no exact epoch pair and no readable
comparison. This is the next explicit product boundary, not evidence of a
readable selected diff. The fixed metadata-only record is outside the checkout
at
`~/Library/Application Support/BetterPullReview/BPR-0793-evidence-2026-09-13/product-read-d11.json`
(SHA-256 `65b7c5adda82d1431335176db3491b55ae0785c1ee07d03a5c6c8d600ad47916`).
The `gh` account remained `MikeMarcin`; no live PR write, credential transfer
or device-session change occurred.
