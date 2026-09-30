---
schema: "implement-rfc.card.v1"
id: "BPR-0143"
title: "Mixed-client acceptance against a designated repository"
status: "Parked"
rfcs: []
depends_on: ["BPR-0142"]
parked_on: "awaiting-live-github"
metadata: {"origin_rfc":"RFC-BPR-0008","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC-0008 closeout dependency"}
---

# Mixed-client acceptance against a designated repository

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this unfinished acceptance out of RFC-BPR-0008's
terminal scope. This card remains Parked; no live case is newly passed. Its
original path preserves the approved immutable packet and all historical
evidence below. The [amended RFC](../../rfcs/RFC-BPR-0008-github-review-round-trip-interactions.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0008.md) retain the explicit handoff.

This follow-up also owns the wider unreplayed closeout obligations: native
resolve/unresolve and viewed/unviewed behavior where supported, explicit
capability degradation, an editable-local conflict resolved in favor of server
truth, and the designated uninstall/database-loss course that reopens and
reconstructs the shared review without losing GitHub content. Preserve the
exact server identities, author/text/version and postconditions for each case.
Fake-provider or restart-recovery success does not prove these live sequences.

Next: revalidate the existing approved four-action packet and target before
execution through a permitted course. Obtain the separately bounded authority
and disposable local roots for the additional capability/state-loss work.
Inspect applicable workflow triggers first; no GitHub Actions, push, broad
deletion or extra message is authorized by this deferral.

RFC-BPR-0008 requires that mixed-client edits, replies, deletion, resolution,
and reopening are preserved. BPR-0138 proves the reconciliation against a fake
provider, which establishes that the projection handles each shape correctly.
It does not establish that GitHub produces those shapes.

## Exactly what is missing

The designated repository and token from BPR-0142, plus a **second client**
acting on the same pull request — the web UI is sufficient — while the
workbench is open.

## Required direct evidence

- A comment created externally appears after refresh, with its GitHub id.
- A comment edited externally updates rather than duplicating.
- A comment deleted externally disappears rather than lingering.
- A thread resolved and reopened externally reaches the workbench in both
  directions.
- A local assumption is shown never to overwrite an unknown external change
  without an explicit user action.

## Current slice — 2026-09-05

BPR-0142 is Done. Claim this card to reconcile the existing external reply
evidence, identify the remaining bounded live operations and prepare their
exact continuation. Preserve remembered sign-in and reuse the existing test
thread; no further message or destructive action is claimed by preparation.

## Prepared mixed-client continuation — 2026-09-05

The existing external creation/reply-return evidence is retained in
[the reply result](../../verification/reviews/web-comment-reply-return-v001/attended-result.md).
A [bounded packet](../../verification/reviews/mixed-client-thread-state-v001/operator-packet.md)
now names the exact reply edit/deletion and parent resolve/reopen sequence.
No new remote operation or acceptance is claimed. The app's ordinary Refresh
replaces accepted server snapshots and can show each required visible state.
Its internal canonical-ID mapping and server-wins behavior have fixture
coverage; numeric-ID visibility and a live editable-local conflict remain
explicit observation limits. The closeout's database-loss/uninstall sequence
also remains separately unproved; do not substitute restart recovery tests.

Park this live checkpoint while continuing the independent BPR-0144 production
review workflow. Receive a plain owner instruction for the prepared remote
sequence, then revalidate the exact target and observe each actual result.

## Four actions approved; live execution unavailable — 2026-09-05

The owner answered “Approve the four test actions”. The
[recorded response and result](../../verification/reviews/mixed-client-thread-state-v001/validation-record.json)
bind the unchanged packet. Computer Use stopped when selecting Google Chrome
because its current URL was disallowed. No action ran and no alternate route
was attempted. Approval is complete; this card now parks on live execution.
Revalidate the exact identities when a permitted course becomes available.
Continue independent whole-review implementation meanwhile.

## Owner designation of BPR-Test — 2026-09-30

On 2026-09-30 the owner wrote: "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". That covers the repository and the remote
actions this card needs (the four approved test actions on the existing
BPR-Test#1 thread, and the wider edits, deletion and resolve/reopen). A
second client is also available: the website or `gh api` under the same
account suffices, as the card says.

The live target qualification milestone did not run it. What is still
missing is the observation: each required result is what the **shipped
application shows after Refresh while the workbench is open** under the
remembered sign-in, and the database-loss course reopens that application.
That is an attended course (the prepared [operator packet](../../verification/reviews/mixed-client-thread-state-v001/operator-packet.md)),
and the earlier attempt stopped at a browser-control boundary, not at
authority. No unattended check reads the workbench's projection in its place,
and reading BPR's own store back would not be the evidence the card asks for.

The card stays Parked on `awaiting-live-github`. Next: an attended session
that revalidates BPR-Test#1's identities, performs the four approved actions
from a second client, and records the workbench after each Refresh. No live
object was made for it.
