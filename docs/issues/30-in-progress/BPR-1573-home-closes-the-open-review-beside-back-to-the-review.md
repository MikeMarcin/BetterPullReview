---
schema: "implement-rfc.card.v1"
id: "BPR-1573"
title: "Home closes the open review beside Back to the review"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-17","owner_note":"Home should have a close review next to back to review.","governing_rfc":"RFC-BPR-0032","related_rfcs":["RFC-BPR-0028","RFC-BPR-0076"],"related_cards":["BPR-0775","BPR-1381","BPR-1535","BPR-1539","BPR-1567"],"reservation":"docs/issues/_reservations/BPR-1573--BPR-1573--f2af07a882df.json"}
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
