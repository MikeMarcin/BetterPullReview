---
schema: "implement-rfc.card.v1"
id: "BPR-0794"
title: "Resolve the exact epoch pair for a large pull request selected diff"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0793"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Resolve the exact epoch pair for a large pull request selected diff

The read-only tests-disabled GitHub CLI profile reached **Pull request ready**
for the owner-retained `EpicGames/UnrealEngine#1773` and presented 1,523 files
and 19 review threads. Two file selections each refused `selected_file_stale`;
the epoch control had no exact pair and no selected diff was readable. BPR-0643,
BPR-0792 and BPR-0793 are Done for their bounded outcomes and remain terminal.
This is the separate selected-diff follow-up authorized by mandate r14.

## Acceptance

- Attribute the missing exact epoch pair and selected-file refusal through the
  public composition, retaining only fixed aggregate and failure metadata for
  the private target outside the checkout. Do not retain private response,
  path, object, thread, cursor, or diff content in repository artifacts or logs.
- Correct the responsible transition or host mapping without fabricating an
  epoch, relaxing exact authority/object checks, adding unbounded history, or
  silently treating incomplete returned commits as complete history. Focused
  positive and negative checks must cover the failure and stale selection.
- Focused core, host or Qt checks selected by the repair, a composed public-path
  check, documentation gate and one integration-focused check pass.
- A tests-disabled Mac application read through the already opted-in CLI
  profile shows the correct file list **and a readable selected diff** for the
  exact target, with source/anchor identity checked against the accepted epoch
  pair. A remaining explicit refusal is recorded honestly and leaves this
  acceptance open.
- No live pull request write, global `gh` account change, credential extraction
  or transfer, device-session change, or private content in the checkout occurs.

## Evidence

The previous fixed metadata-only product observation is recorded on BPR-0793
and BPR-0617. The append-only product diagnostic log attributes the absent
pair to `protocol_acquire/transport/unavailable` before epoch revision
preparation. A sandboxed, bounded `gh` Git probe reproduced nonzero full-clone
exits near the 32-MiB file ceiling. A blobless depth-one exact-base clone
fits the existing bounds and contains the accepted commit and root tree, but
not selected-file blobs. This is a possible first step, not a readable diff.
The fixed aggregate-only record is outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0794-evidence-2026-09-13/diagnosis-d12.json`
(SHA-256 `a68be3bd2c2b65ab3f749703107b4fbd04d53287d5e607467c64d62dca317311`).
No child stderr, response body, private object or credential was retained.
An adapter-only sparse clone cannot satisfy the current strict connected-pack
Git cache contract. A synthetic complete-pack/missing-blob/missing-parent
fixture confirmed that the incomplete cases refuse. A host-owned exact-object
request and separate sparse-residency/recovery contract are needed before a
selected diff can be sourced within unchanged limits. The source repair and
tests-disabled product criterion remain open; no second product read is
claimed.
