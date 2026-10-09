---
schema: "implement-rfc.card.v1"
id: "BPR-0143"
title: "Mixed-client acceptance against a designated repository"
status: "Parked"
rfcs: []
depends_on: ["BPR-0142"]
parked_on: "human-review"
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

## Reassessment after the BPR-Test designation — 2026-10-05

The owner's 2026-10-05 designation (root `AGENTS.md`) covers every remote
action this card needs, and the single `MikeMarcin` account is enough: the
second client can be `gh api` rather than the website, which also avoids the
browser-control stop of 2026-09-05. Each of the four approved actions has a
direct `gh api` form (PATCH and DELETE on the pull request review comment,
GraphQL `resolveReviewThread` and `unresolveReviewThread`).

A read-only `gh api` revalidation as `MikeMarcin` found the packet's
identities unchanged: BPR-Test#1 open, not draft, head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`; parent comment 3940608913 on
RIGHT line 1 of `bpr-live-review-fixture.txt` with body `BetterPullReview
attended round-trip verification.`; reply 3941498416 (in reply to
3940608913) with body `BetterPullReview web reply return verification.`;
their thread `PRRT_kwDOTyvof86fjN_V` unresolved. A second unresolved thread
(comment 4116400082, by `MikeMarcin`) now exists on the PR and is outside the
packet. BPR-Test has no workflows (`actions/workflows` total 0).

The four actions were not run. Each result is what the shipped application
shows after Refresh with the workbench open, observed between actions, and
this session has no way to see the application's window. Running them
without that observation would spend the only edit-then-delete sequence on
the disposable reply. Reading BetterPullReview's own store back is not the
evidence the card asks for. The database-loss course also reopens the
application.

The service wait is over and the remaining wait is attendance, so the card
moves from `awaiting-live-github` to `human-review`. The prepared
[operator packet](../../verification/reviews/mixed-client-thread-state-v001/operator-packet.md)
and the owner's approval of its four actions still apply; only its second
client changes from the website to `gh api`. No live object was made for it.

**Owner question:** will you attend one session at the Mac in which the agent
performs the four approved actions on BPR-Test#1 through `gh api`, one at a
time, and you press Refresh in BetterPullReview after each and confirm what
the workbench shows?

## Owner session prepared, 2026-10-09 (main, Mac)

The owner question above becomes a wizard he runs himself. He makes each of the four approved changes from a second client (`gh` in Terminal), then presses Command-R in BetterPullReview and answers Yes or No. No agent waits between actions, and none drives the browser.

- The application is the shipped one: BetterPullReview v0.1.5, the signed private prerelease (DMG sha256 `b7c9378cc93c93c6435b1970456596281d346f09d45202fb558a92ae32e2e727`, matching the release's `.sha256` asset; `spctl` accepted it as Notarized Developer ID and `stapler validate` passed), downloaded to `/Users/jaeger/.mask/wizard/bpr-live-github-20261009-files/`.
- A read-only `gh api` revalidation as `MikeMarcin` (`X-RateLimit-Remaining` 4970) found the packet unchanged: BPR-Test#1 open, head `17990ba0c2fb50338141f35c0717f0f99639c7ba`; comment 3940608913 and reply 3941498416 with their packet bodies; thread `PRRT_kwDOTyvof86fjN_V` unresolved; no workflows.
- Wizard `bpr0143-mixed-client-20261009-v001` (session `1d8cfa4b-00c5-4ca7-b8a2-0ffafc4d302d`, 8 steps), state `/Users/jaeger/.mask/wizard/bpr0143-mixed-client-20261009-v001`, source root this checkout; its loopback host runs detached. The page's URL carries a token and is handed to the owner directly, not committed. Steps: open v0.1.5; a baseline Yes/No; edit, delete, resolve and reopen, each with the exact `gh api` command and a Yes/No on what BetterPullReview shows after Refresh; notes; then the agent's `gh api` read of the final server state.
- Still outside this session, as the packet says: the live editable-local conflict and the database-loss or uninstall course. Neither is claimed.
