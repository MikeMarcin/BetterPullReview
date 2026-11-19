---
schema: "implement-rfc.card.v1"
id: "BPR-2269"
title: "An Approve whose line comment was written before a reread that kept the head stops at the pending-comment hold"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2269--BPR-2269--724289100633.json","found_by":"BPR-2265's implementer, 2026-11-18, building its reread fixture course","related_cards":["BPR-2265","BPR-2263","BPR-1345"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "66b412893", "evidence": {"kind": "command", "detail": "application_review_submission_composition_public_path: a line comment put in the review before two head-keeping rereads, Finish your review open and the drafts' restore held (access::hold_draft_restore); the Approve reaches the server once, from one Submit, carrying the comment; nothing refused. With the keep disabled the same course stops at 'Not submitted: a pending comment changed or could not be held for this review.'"}}, {"criterion": "AC2", "outcome": "pass", "revision": "66b412893", "evidence": {"kind": "command", "detail": "BPR-2265's reread course writes its line comment before the rereads and checks the published Approve body, event and comment; it runs on its own fixture server and data root"}}, {"criterion": "AC3", "outcome": "pass", "revision": "66b412893", "evidence": {"kind": "command", "detail": "Passed on macOS dev preset, offscreen: application_review_submission_composition_public_path (and scale_125, scale_200), application_reply_draft_contract, application_private_draft_restore, application_conversation_read_public_path, application_inline_publish_cli_consent, application_line_draft_contract, application_draft_delete_contract, application_draft_state_contract, application_refresh_pick_contract, local_draft, architecture_boundaries; application_start_review_contract passes, with an intermittent phase-17 'Review recovery is unavailable' miss reproduced without this change; format_check and clang-tidy clean on the changed files; verify_documentation DOCUMENTATION_OK. application_review_round_trip_composition_contract passes its resealed digest and then fails at SEMANTIC_COMMENT_PROBE_FAILED phase=4, which it also does without this change"}}]
---

# BPR-2269: An Approve whose line comment was written before a reread that kept the head stops at the pending-comment hold

## Observed

In BPR-2265's fixture course, an Approve carrying a line comment written
before two rereads of the same pull request (same head and account) stopped
at "Not submitted: a pending comment changed or could not be held for this
review." The log shows `review_submit_wait submit_subject=pending_comments`
and then the refusal, before any request. It may be part of the owner's real
Submit failures on 2026-11-18.

## Acceptance

- AC1. After rereads that keep the head, an Approve carrying a line comment
  written before them publishes with the comment, from one Submit.
- AC2. BPR-2265's reread course carries the line comment again.
- AC3. Focused tests, format and tidy on the changed files, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Cause

Every accepted handoff runs `compose_review_ledger`
(`src/app/src/github_application/window_review_ledger.cpp`), including a
reread that keeps the head. That retired the private review:
`PrivateReview->invalidate()` dropped its store context and binding and
bumped its generation, and `PrivateDraftsReady` went false. The private
review then came back in several asynchronous steps. The ledger reopened,
then `open_private_review`. `observe_private_review` bound it only once the
reread's comparison was accepted again, and only then were the drafts
restored.

During that window, `change_review_drafts`
(`window_review_drafts.cpp`) saw `PrivateDraftsReady` false and took its
in-memory path. The shared draft owner refuses a `begin_publish` there, so
`hold_review_comments` answered "not held". Submit then said "Not
submitted: a pending comment changed or could not be held for this
review." No request was sent. The draft revisions, the anchor, the body and
the path all still matched. Nothing about the comment had changed; only its
store had been taken away. How long the window lasts depends on how quickly
the comparison is read again, which is why the stop came and went.

## Change

- `compose_review_ledger` keeps the private review open, bound and restored
  when all of these hold:
  - its drafts are restored;
  - its open store is the handoff's (`private_review_workflow::serves`: the
    same profile, host, account, repository, pull request and storage root);
  - its binding has the handoff's head, host authority and access mode.

  `retire_review_ledger(bool)` retires the rest of the ledger as before. A
  kept private review is not reopened when the ledger answers. A changed
  head, account or pull request retires it as before, so the stale-head
  rules are unchanged.
- `ensure_shared_drafts` sets `PrivateDraftsReady` false when it replaces
  the draft owner. A new owner has read no drafts, even where the private
  review was kept.
- Testing: `access::hold_draft_restore` holds the drafts' restore, as a slow
  comparison or store leaves it.

## Result

- `application_review_submission_composition_public_path`, BPR-2265's reread
  course:
  - It now writes a line comment and puts it in the review before the
    rereads, opens Finish your review, holds the drafts' restore and runs two
    head-keeping rereads.
  - The Approve then publishes once, from one Submit, carrying the comment.
    The course has its own fixture server, so its published thread touches
    no later course.
  - With the keep disabled it stops at exactly the reported sentence. Before
    the restore hold, the same stop showed only when Submit raced the
    reread.
- `application_reply_draft_contract`'s deleted course held the private store
  over a head-keeping Refresh and required the private review to be torn
  down. It now requires the private review to be kept bound, with its
  store's lane held. Discard is still kept until the store resumes.
- `application_review_round_trip_composition_contract`: the production app
  digest is resealed (this card plus BPR-2265's unsealed changes). The course
  then fails at its stale-head probe (`SEMANTIC_COMMENT_PROBE_FAILED
  phase=4`). It fails the same way with this card's keep disabled, so the
  failure predates this card. It is not fixed here.
