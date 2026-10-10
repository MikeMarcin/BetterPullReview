---
schema: "implement-rfc.card.v1"
id: "BPR-0143"
title: "Mixed-client acceptance against a designated repository"
status: "Parked"
rfcs: []
depends_on: ["BPR-0142"]
parked_on: "human-review"
metadata: {"origin_rfc":"RFC-BPR-0008","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC-0008 closeout dependency","related_cards":["BPR-1374"]}
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

## Owner wizard withdrawn, 2026-10-09

The owner said in chat on 2026-10-09 that this course is agent work: "this
all seems like stuff you can do without me". Wizard
`bpr0143-mixed-client-20261009-v001` (session
`1d8cfa4b-00c5-4ca7-b8a2-0ffafc4d302d`) is withdrawn. `wizard.py stop`
stopped its loopback host, and `pause` closed the session at 1 of 8 steps.
None of its gh steps ran. The v0.1.5 DMG downloaded for it was not used.

## Agent-run mixed-client course, 2026-10-09 (main `ec16c550b`, Mac)

The agent ran the course itself. Two `gh` accounts acted as the second
client: `MikeMarcinTest` (id 340367457, pull-only on BPR-Test), the
owner's test account added on 2026-10-09, and `MikeMarcin` (id 7605339).
Each command was pinned with
`GH_TOKEN="$(gh auth token --user …)"`, so no token was printed and nobody
ran `gh auth switch`. The application ran offscreen and signed in through
its ordinary **Use GitHub CLI** path as `MikeMarcin` (account 7605339).
Nothing drove or captured the desktop.

Before the course, BPR-Test had no workflows (`actions/workflows`
`total_count` 0), no `.github` directory and no hooks. A new pull request
was used, and BPR-Test#1 was not touched. The packet's approved texts
(reply `BetterPullReview web reply return verification.`, edited to
`BetterPullReview web reply edit verification.`) were reused on it.
Nothing else was posted, apart from the thread's root.

**Target.** The course created branch `bpr-0143/mixed-client-20261009`
from main `e2a44aa8`, through the API with no git push. It added one commit,
`f3996e06c184ab9d21ee926eb73aa1344d824b23`, which adds
`bpr-0143-mixed-client.txt`. On that branch it opened BPR-Test#47
(`PR_kwDOTyvof88AAAABHohqFg`, author `MikeMarcin`). At the end, #47 was
closed unmerged (`closed_at` 2026-10-10T00:54:42Z) and the branch deleted.
At closing, #47 had 0 review threads, 0 reviews, 0 review comments and 0
issue comments, and its file was `UNVIEWED`.

**Observation.** A local, unmerged, unpushed probe on branch
`claude/bpr-0143-live-probe` (`36a093af2`, one source over `ec16c550b`'s
product) was built two ways:

- **A**: Release, tests on. It writes the accepted snapshot, which holds
  GitHub node and database IDs, author, body, `updatedAt`, resolution,
  permissions and per-file viewed state.
- **B**: Release, tests off. This is the shipped composition, with the
  live review-mutation provider and local drafts. In a tests-on build both
  are composed only over a fixture.

Each opened `MikeMarcin/BPR-Test#47` from Home. After every external action
the course pressed the public **Refresh** (`github_refresh_action`). It then
recorded each conversation-model row: identity, parent, author,
resolved/deleted, `DisplayRole`, `AccessibleTextRole` and
`AccessibleDescriptionRole`. It also recorded the thread cards, the
files-drawer rows, the review editor's draft and status, and the in-place
reply editor's text. Each Refresh completed in 12-25 s. Rate limits, from
`X-RateLimit-Remaining` response headers: MikeMarcin core 4997 to 4375 and
GraphQL 4983 to 4801; MikeMarcinTest core 4991 to 4986. Neither came near
the 1,500 margin.

| Case | Server action and identity (second client) | What the application shows after Refresh | Result |
|---|---|---|---|
| External create | `MikeMarcinTest` POST line comment, RIGHT line 1: id **4235562728**, node `PRRC_kwDOTyvof878dY7o`, body `BetterPullReview mixed-client verification.`, updated 00:14:27Z | A: thread `PRRT_kwDOTyvof86q-93Y` with comment `PRRC_kwDOTyvof878dY7o`, `database_id=4235562728`, author `MikeMarcinTest`. Row `review-comment:PRRC_kwDOTyvof878dY7o` reads "Review comment by @MikeMarcinTest \| BetterPullReview mixed-client verification." in display and accessible text | pass |
| External reply | `MikeMarcinTest` reply: id **4235564393**, node `PRRC_kwDOTyvof878dZVp`, `in_reply_to` 4235562728, body `BetterPullReview web reply return verification.` | A: second comment, `database_id=4235564393`, `reply_to=PRRC_kwDOTyvof878dY7o`, nested under its parent | pass |
| External edit | `MikeMarcinTest` PATCH 4235564393 to `BetterPullReview web reply edit verification.`, updated 00:43:00Z | A and B: the same identity `PRRC_kwDOTyvof878dZVp` shows the new text. The thread still has 2 comments, with no duplicate. A: `updated_at=2026-10-10T00:43:00Z` | pass |
| External delete | `MikeMarcinTest` DELETE 4235564393 (204; GET then 404) | A and B: the reply row is gone. The thread has 1 comment and the card 1 post. Its now-empty review `PRR_kwDOTyvof88AAAABRm1WEA` also left GitHub | pass |
| External resolve | `MikeMarcinTest` `resolveReviewThread` was refused (FORBIDDEN, no permission). `MikeMarcin` (gh) resolved; `resolvedBy` MikeMarcin | A and B: "Resolved review thread — bpr-0143-mixed-client.txt:1 (head/right side)". Resolved is true; A shows viewer_resolve denied and unresolve allowed | pass |
| External reopen | `MikeMarcinTest` `unresolveReviewThread` was refused (FORBIDDEN). `MikeMarcin` (gh) reopened | A and B: "Open review thread — …". Resolved is false; resolve allowed and unresolve denied | pass |
| Local assumption never overwrites | B held an unsent reply draft on the thread (`ReplyToThread=PRRT_kwDOTyvof86q-93Y`, body `Local unsent reply draft held while another client edits.`) while `MikeMarcinTest` edited the reply | B showed the server's edited text. GitHub afterwards held only the two `MikeMarcinTest` comments and their two reviews, with the edited body. `MikeMarcin` had published nothing and had no pending review. No automatic action wrote anything | pass |
| Viewed / unviewed | `MikeMarcin` (gh) `markFileAsViewed`, then `unmarkFileAsViewed` (viewed state is per viewer, so the test account cannot set it) | A: the accepted snapshot went `unviewed` to `viewed` to `unviewed`. No surface presents it: the files-drawer row's display, accessible text and tooltip are only the path | read: pass; presented: no |
| Capability degradation | none | B, the shipped composition under the gh profile: the review editor says "Publication is available; native thread resolution and viewed-state recording are unavailable." The thread menu's **Resolve** is disabled with tooltip "Resolution is not available for this thread.", and `resolution_offered=0` | pass |
| Editable-local conflict | B held a reply draft (`…held while another client deletes the thread root.`) and `MikeMarcinTest` deleted the root 4235562728 (204) | A and B: the thread and its card left, so the server's truth was taken, and nothing was published. The draft also left without a word: no card, no notice, and `editor_draft: none` | server truth: pass; local words: fail, see [[BPR-1374]] |
| Database loss | All 11 `state/v1/*.sqlite3` were deleted from A's closed profile root, then A was reopened on the same root | A rebuilt the databases. Thread, comment identities, database ids, author, body, `updatedAt`, resolution, permissions, capabilities and viewed state are line-for-line those before the loss. The review row matches GitHub's one remaining review | pass |
| Uninstall | A's whole profile root, including `data` and `data-agent-workspaces-v1`, was removed. A was opened on a new, empty root. Nothing was written outside the root (`~/Library` Preferences, Application Support and Caches were checked) | Identical to the database-loss reopen | pass |

**Local drafts and Refresh.** In the shipped composition (B), every
Refresh detached the review editor's reply draft (`editor_draft: none`).
This happened with no external change too (`07-refresh-no-change`). The
in-place reply field kept showing the typed text until the thread itself
left. After a quit and restart on the same root, no reply draft came back,
whether or not a Refresh came first. Selecting the file, line 1 or line 2
showed none, and no draft card appeared. The server's truth always won and
nothing was ever published. But the rule BPR-0138 states, that a held edit
is reported as conflicted rather than discarded quietly, is not what the
shipped application does with an unsent reply. [[BPR-1374]] owns this.
The shipped composition does not edit published comments. Its
reconciliation is called with no local edits, so the "unpublished edit to a
published comment" shape that BPR-0138 tests by fixture has no live
counterpart to exercise.

## Remaining, and why the card stays Parked

1. **[[BPR-1374]]** (agent work). An unsent reply draft must survive a
   Refresh and a restart. When its thread leaves the server, the draft must
   be reported as conflicted rather than dropped. Then re-run the
   editable-local and draft rows above through the same probe.
2. **In-app resolve and reopen round trip** (needs the owner). The shipped
   composition refuses thread resolution under the GitHub CLI profile ("Thread
   resolution is unavailable through the GitHub CLI profile."). Only the
   native GitHub.com sign-in offers it, and that sign-in is a device-code
   flow the owner completes in his browser. Once a disposable profile is
   signed in that way, the agent can press Resolve and Reopen in the app and
   read GitHub's `isResolved` and `resolvedBy` back through `gh`.
3. **Not a second-identity gap.** `MikeMarcinTest` has pull access only, so
   GitHub refuses it `resolveReviewThread` and `unresolveReviewThread`. Those
   two actions ran as `MikeMarcin` through `gh`, a second client of the same
   account. Granting the test account write access would change BPR-Test's
   collaborators, which no agent may do. This needs the owner only if a
   resolve by a different identity is wanted.

GitHub's viewed state reaches the accepted snapshot in both directions, but
no surface shows it. Whether the workbench should present it is a product
question, not part of this card's evidence.

## BPR-1374 landed in code, 2026-10-09

[[BPR-1374]] fixes the unsent-reply rows above in the product. An unsent
reply is held again after Refresh and presented after a restart. When its
thread is deleted elsewhere, it is kept as a flagged note instead of being
dropped. Its fixture courses fail with the old rule and pass with the fix.
The live re-run of this card's two draft rows through the probe was not
done; the coordinator deferred it. Item 1 of "Remaining" is now only that
re-run.
