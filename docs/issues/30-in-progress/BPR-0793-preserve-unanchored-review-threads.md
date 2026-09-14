---
schema: "implement-rfc.card.v1"
id: "BPR-0793"
title: "Preserve review threads with unavailable line anchors"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0792"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
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

Pending implementation and exact-target verification.
