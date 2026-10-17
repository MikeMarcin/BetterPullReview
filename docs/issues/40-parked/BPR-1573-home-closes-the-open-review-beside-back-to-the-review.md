---
schema: "implement-rfc.card.v1"
id: "BPR-1573"
title: "Home closes the open review beside Back to the review"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-17","owner_note":"Home should have a close review next to back to review.","governing_rfc":"RFC-BPR-0032","related_rfcs":["RFC-BPR-0028","RFC-BPR-0076"],"related_cards":["BPR-0775","BPR-1381","BPR-1535","BPR-1539","BPR-1567"],"reservation":"docs/issues/_reservations/BPR-1573--BPR-1573--f2af07a882df.json"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"58ee21f56610a72ff0d97fd29ce891bbcbc22315","evidence":{"kind":"command","detail":"qt_composed_github_scale_100/125/200 and application_close_review: Close review follows Back to the review and its key on the bar's line, named, described, bordered, no key; Tab reaches it; Space activates it."}},{"criterion":"AC2","outcome":"pass","revision":"58ee21f56610a72ff0d97fd29ce891bbcbc22315","evidence":{"kind":"command","detail":"core_github_window_read, application_github_window_read_host and application_close_review: the core forgets the pull request and keeps the session, a read in flight is cancelled and its late answer is stale, the epoch authority is invalidated once, the reading destination, tabs and drafts ledger are released, Home has no Back row, the bar has focus, Refresh is unavailable; a second lane keeps its pull request."}},{"criterion":"AC3","outcome":"pass","revision":"58ee21f56610a72ff0d97fd29ce891bbcbc22315","evidence":{"kind":"command","detail":"application_close_review: the comment being written is saved, Close review asks nothing, and it is there after reopening the same pull request."}},{"criterion":"AC4","outcome":"pending","revision":"58ee21f56610a72ff0d97fd29ce891bbcbc22315","evidence":{"kind":"command","detail":"Negative control fails without the host's epoch invalidation. Focused set passes; qt_workbench_window_scale_200 and application_profile_multi_window_public_path timed out under load average ~250 and are owed on the merged tree with the full dev ctest. clang-format 22, clang-tidy 22 clean on changed lines, architecture_boundaries, DOCUMENTATION_OK on a clean merge with main."}},{"criterion":"AC5","outcome":"pending","revision":"58ee21f56610a72ff0d97fd29ce891bbcbc22315","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending: session 71540c0b93725af935e64bedcdc66c9a, candidate BPR-1573-close-review-v001, fingerprint e365e4eb, cursor 3, frozen root bpr1573-close-review-v001.1nv8u3tq."}}]
---

# BPR-1573: Home closes the open review beside Back to the review

The owner asked on 2026-10-17: "Home should have a close review next to back
to review." Over an open review Home offers only the way back
([[BPR-0775]], beside the bar since [[BPR-1381]], ⌘⇧H since [[BPR-1535]]).
Nothing releases the review short of opening another pull request, signing
out or closing the window.

Governing design: Home's return row from
[RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md),
recorded in the [workbench specification](../../ux/workbench-specification.md);
the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md); and the
document tabs that start again for another pull request ([[BPR-1539]],
[RFC-BPR-0076](../../rfcs/RFC-BPR-0076-file-change-navigation-enters-review.md)).

## Acceptance

- AC1: Over an open review Home shows **Close review** beside **Back to the
  review**, the two grouped as one control on the bar's line. Close review is
  reachable by Tab after Back to the review and activates with Space; it has
  an accessible name and a description that names the review and says what
  happens to its drafts. No new shortcut is bound.
- AC2: Close review releases the review in this window, decided by the core:
  the window's read lane forgets its pull request (a read in flight is
  cancelled and its late answer is stale; with nothing open the gesture is
  refused) and the composition releases it by the path opening another pull
  request and signing out already take, with its document tabs started
  again. Home afterwards has no Back to the review, the bar has focus, and
  Refresh is unavailable. Another window showing the same pull request keeps
  it.
- AC3: Drafts are kept as that release path keeps them: a comment being
  written when the review is closed is still there when the same pull
  request is opened again, and closing asks nothing.
- AC4: Qt and composed courses cover AC1 to AC3, with a negative control
  that fails without the release. Format, tidy, architecture and
  documentation gates pass.
- AC5 (human): The owner accepts the frozen candidate, Light and Dark:
  "Close review sits beside Back to the review and returns Home with nothing
  open".

## Implementation

Core decides: `close_pull_request_intent` on the window read lane. The host
performs: the window read host cancels the read and revokes the review's
authorities; the composition releases the review and its tabs. Qt presents:
the landing's Close review button and the window's dispatch. Never drive or
capture the owner's desktop; the owner checks the frozen build themselves.

## Checkpoint — candidate v001, 2026-10-17

Product candidate `58ee21f56610a72ff0d97fd29ce891bbcbc22315` on branch
`bpr-1573-close-review` (worktree
`/Users/jaeger/.codex/worktrees/bpr1573-close-review/BetterPullReview`). Not
merged to main until the owner accepts. Paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1573/handoff.json).

Design. Nothing closed a review before: opening another pull request and
signing out release a window's review through the read host's epoch
invalidation, which runs the composition's `invalidate` (reading
destination, drafts ledger, review mutation and submission, workbench and
epoch). Close review reuses that release. The core decides with
`close_pull_request_intent` on the window's read lane: it clears the pull
request scope, keeps the session, ends a read in flight (its late answer is
stale) and refuses when nothing is open. The read host stops the read and
invalidates the epoch authority; the composition starts the document tabs
again ([[BPR-1539]]). Qt presents: Back to the review (filled, default), its
key, then Close review (bordered, no key), on the bar's line; Tab reaches
Close review after Back to the review. After closing, Home has nothing to
return to, the bar takes focus and the status line says "Closed the review
of owner/name#N." Drafts follow the release path: no prompt, and a comment
being written is there again when the same pull request is opened.

| Acceptance | Evidence at `58ee21f56` | Result |
|---|---|---|
| AC1 | `qt_composed_github_scale_100/125/200`: Close review follows the way back and its key on the bar's line, named, described, bordered, no key; Tab reaches it; Space asks once. `application_close_review`: same through window-system keys in the composed app | Pass |
| AC2 | `core_github_window_read`: refusal with nothing open, release keeps the session, read in flight cancelled and its late answer stale, second window keeps its pull request. `application_github_window_read_host`: one epoch invalidation, read cancelled, late answer unpublished. `application_close_review`: read lane, reading destination, tabs and ledger released; no Back row; bar focused; Refresh unavailable | Pass |
| AC3 | `application_close_review`: the comment being written is saved, Close review asks nothing, and the comment is there after reopening the same pull request | Pass |
| AC4 | Negative control: without the host's epoch invalidation the course fails "the review's reading and its tabs are released". Focused set passes; `qt_workbench_window_scale_200` and `application_profile_multi_window_public_path` timed out at load average ~250 and are owed on the merged tree. clang-format 22; clang-tidy 22 clean on changed lines; `architecture_boundaries`; DOCUMENTATION_OK on a clean merge with main | Pass, two reruns owed |
| AC5 human | Session `71540c0b93725af935e64bedcdc66c9a`, candidate `BPR-1573-close-review-v001`, fingerprint `e365e4eb…`, ready and protected at cursor 3; frozen root `bpr1573-close-review-v001.1nv8u3tq` | Pending |

Next: on hand-back after cursor 3, an Accept merges main into the branch,
reruns the focused set and the two timed-out rows, runs the full dev ctest
once on the merged tree, lands and finishes the card; a Reject's note
becomes v002.
