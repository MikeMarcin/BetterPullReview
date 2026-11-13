---
schema: "implement-rfc.card.v1"
id: "BPR-2158"
title: "Comments absorbs Notes: Local cards, the state control, N and one gutter mark"
status: "In Progress"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2157"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":["AC01","AC02","AC04","AC05","AC06","AC14"],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
---

# BPR-2158: Comments absorbs Notes: Local cards, the state control, N and one gutter mark

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `comments-absorb-notes` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first; the accepted design is [docs/design/comments-absorb-notes-v1](../../design/comments-absorb-notes-v1/README.md). The Notes tab goes (right drawer and Overview drawer); Local comments are cards in Comments at their lines, replies inside their threads and edge chips for the pull request, whole files and other files; the composer's Local/Draft (Local/Reply) state control in the foot row; N opens it on Local; the computer icon with the tooltip "Only on this computer" (owner, Q1) marks Local and a draft stops wearing the lock; order in a card and a line's group; one gutter mark a line with accessible text naming everything on it; Make draft on line N for an anchor the comparison does not place.

## Acceptance

- **AC01:** The right drawer's tabs are Comments, Tour and Agent, and the Overview drawer has no Notes tab; N on a selected line opens the composer on Local in Comments.
- **AC02:** On a line that has a published thread, a Local reply and a draft reply of mine, one card shows all three in the order defined, each with its state's mark, and the line has one gutter mark whose accessible text names all three.
- **AC04:** Keep local on a draft in the review lowers the review's pending count by one, removes it from every "not in this review" list, and keeps its text, line or range and reply target; Make draft on it returns a draft of the same kind at the same place, not in the review.
- **AC05:** Keep local on a draft held by a submitted review or being published, and Make draft where my draft already stands, are refused with their reasons and change nothing; no action offers a state change on a published comment.
- **AC06:** A Local comment on the pull request, on a whole file, on another file, and on a line the shown comparison does not place each appears where "Where a Local comment can be" says, the last with its reason; none is shown at a line it was not written on, and Make draft on it is offered only as Make draft on line N with a line selected.
- **AC14:** Each state is announced in words to assistive technology ("Local comment, only on this computer", "Draft, not in your review", "Draft, in your review"), and the state control, Make draft and Keep local are reachable and operable by keyboard alone.

## Notes

Ends in an owner look (Accept or Reject) on a frozen build whose fixture holds Local, Draft and Published comments on one line.

## Progress (2026-11-13, not claimed)

Frontend-design was loaded first; the accepted design is followed. The card is not claimed: it depends on BPR-2157, which stays In Progress until its AC03 and AC13 have composed evidence. Lane commits `9e408149a`, `e8553b931`, `007d48bdc`, `beede2fc3`.

Built:
- **Store move.** Keep local and Make draft are each one SQLite transaction (`private_review_store_port::keep_local`, `private_store_write::MakeDraftFrom`). Keep local ends the draft as discarded a revision on, so every window's shared drafts adopt its end from the store, and stores the Local comment. The comment takes the draft's identity when no Local comment holds it, else the next free one. Make draft removes the Local comment at its revision and adds the new draft in the same write. Both run on the private review workflow's lane and fan the durable commit out; the ledger host then reloads its Local comments.
- **Comments drawer.** Local comments are cards at their lines, after the published threads and before my draft. A Local reply is a post inside its thread's card, marked "Local reply" with the computer. A comment the comparison does not place is drawn after the placed cards with its reason. The card offers Make draft (or "Make draft on line N", or disabled with why), Copy text and Delete. A saved draft's card offers Keep local.
- **Computer icon.** `conversation_glyph::computer`, a line glyph in the drawer's own family, with tooltip and accessible text "Only on this computer". `QStyle::SP_ComputerIcon` was not used: at 12 px the macOS style's icon is a full-colour image unlike every other glyph here. This is a design choice for the owner's look. Drafts wear the pencil; no state wears a lock.
- **Composer.** The two-segment state control (Local, Draft) stands where "Private until you publish" stood. With Local chosen, the foot is Delete, Save (⌘↩) and "Only on this computer. Never sent." In a narrow drawer the segments keep their glyphs only.
- **N.** `localCommentAction` opens the composer on Local in Comments; Comment… and the gutter open it on Draft.
- **One gutter mark a line.** It names everything on the line ("Line 6: 1 thread, 2 local comments, 1 draft reply").
- **Notes tab.** Hidden in the tray and in Overview's drawer. Go to note opens the Local comment's card in Comments. The retired panel claims no key.

Evidence (dev, Debug, macOS):
- `sqlite_private_review_storage` `keep_local_and_make_draft`: identity kept or moved to the next free one, a stale revision or a published draft moves nothing, Make draft removes the comment in the same write.
- `qt_conversation_cards_view`, two new tests:
  - `test_local_comments_share_a_line` (AC02, AC06 outdated, AC14): order in the card, one mark naming everything, the state words, Make draft and Delete dispatch, outdated Make draft disabled with why.
  - `test_composer_state_control` (AC01 preset, AC14): Local's foot, ⌘↩ saves Local, Draft restores the row.
- `qt_workbench_window_scale_{100,125,200}`: AC01 tray tabs Comments, Tour and Agent; the state control is keyboard-reachable; N is the window's.
- Composed look, `--attended-local-comments-look` on `conventional-comments-v1`: N and Save write a Local comment through the composer, two Local replies are seeded, and the second is made a draft reply through Make draft. Output `ATTENDED_LOCAL_COMMENTS_MARK:Line 6: 1 thread, 2 local comments, 1 draft reply`.

Not built yet (this milestone):
- Edge chips for Local comments on the pull request, a whole file or another file (AC06).
- The reply composer's Local/Reply control.
- Editing a saved Local comment in place.
- Delete of a Local comment does not ask first.
- Composed courses for Keep local in the review (AC04) and its refusals (AC05).
- The draft reply's text shows only when the reply field is opened.
- Hidden tabs remain in the accessibility tree.
- `private_notes_panel` still exists as a hidden backend; its code retirement is pending.

The migrated note: the look does not hold one. A migrated note is an ordinary Local comment (same table, same card), so it looks the same. AC11's migration evidence is `sqlite_review_ledger_storage`.

## Owner look v001: Accept (session 12534e15, handed back 2026-11-13T17:44:00Z)

Verified in `session.json`: `comment-states-on-a-line` **Accept**, `composer-state-and-computer-icon` **Accept** (the line-drawn computer glyph included), `notes-tab-retired` **Accept**, each with no note. The look criterion is met for the frozen build from `beede2fc33b5`; the [RFC-BPR-0127 ledger](../../verification/RFC-BPR-0127.md#owner-verdicts-first-look-attended-session-12534e154303dd1913382442a83b8c8f-handed-back-2026-11-13t174400z) records it. Host 68507/68517 stopped; the root removed.

## Progress (2026-11-13, second round)

Frontend-design was loaded first. Lane commits `98069b5c5`, the Qt evidence commit after it, and `64476cd4d`.

Built:
- **Edge chips (AC06).** The pull request link counts my Local comments on the pull request beside the computer ("Pull request conversation: 21 comments, 21 new, 1 local comment"), and Overview's Comments tab draws them after the conversation, "On the pull request. Only on this computer." Two quiet chips beside the link, each shown only with comments: **This file** (whole-file comments, drawn after the placed cards, "On this whole file, not on a line."; the chip opens the first) and **Other files** (a menu of path:line and words; choosing one goes to the file, then the line, where it opens). None is drawn at a line.
- **The reply field's Local/Reply control.** Local puts Save (⌘↩) in Send's place and saves a Local reply inside the thread.
- **A Local reply in a published thread** has its own row: Make draft (or why not), Copy text, Delete.
- **Edit in place.** A Local comment or reply opens the comment editor in place ("Only on this computer. Never sent."), saved at the revision editing began from; a stale revision is refused with the words kept.
- **Delete asks first** (`deleteLocalConfirmation`, "Delete this local comment?" or "…local reply?").
- **Fixes on the composed path.** Save as Local on a draft the store holds always moves it (its identity kept); a composer draft the store never held is written as a new Local comment and the empty draft goes. After Keep local the kept draft no longer holds its line (as after Delete, [[BPR-1862]]), so N and Comment there open a new one. N on a line where my draft with words stands opens that draft on Draft and says so; Local and Save on it is Keep local. Make draft's offer is decided again whenever the drafts change, and the open card redraws a changed offer.

Evidence (dev, Debug, macOS):
- `qt_conversation_cards_view` `test_local_comments_edges_and_editing` (AC06, AC14): the chips' words and counts, keyboard focus, the file chip opening its comment, the Other files menu dispatching `open_local_elsewhere`, edit in place dispatching identity, revision and words and ending on the stored words, the Local reply's actions by its identity, the reply field's Local/Reply control and ⌘↩ Save, and Overview's Comments tab drawing the pull request's comment. Output `LOCAL_EDGE_CHIPS This file: 1 local comment, only on this computer | Other files: 2 local comments, only on this computer | Pull request conversation: 1 comments, 1 local comment`.
- `application_local_comments_composition_public_path` (new; composed window on `conversations_v1`, review submission fixture server), passing twice:
  - N and Save on a thread's line; a Local reply through the reply field's Local and Save; a whole-file and a pull request comment (stored through a test seam, as a migrated note leaves them). The file chip and the pull request link count them; the pull request's is not drawn beside the code (AC06).
  - Edit in place and Delete, which asks first.
  - AC05: the published thread offers Keep local and Make draft on nothing but my Local reply; Make draft where my draft stands is disabled with "Your draft already stands there. Nothing is merged." and nothing changes; Keep local on a draft a submitted review holds (terminals held) is refused, "Kept as it is: your submitted review holds this draft until GitHub answers.", and the draft stays held.
  - AC04: Start a review, then Keep local (Local and Save in the composer): the roster's in-review count falls by one, no draft list or "not in this review" list holds it, and it is a Local comment with its words at its line. Make draft returns a draft not in the review at the same side and line with the same words.

Not built yet (this milestone):
- The hidden Notes tabs are still tabs (hidden with `setTabVisible`, so Qt reports them invisible), and `private_notes_panel` is still the hidden backend for the old Notes surface and the courses that drive it. Retiring it moves `review_submission_composition_public_path`, `review_tour_composition_public_path`, `explain_file_public_path`, `hide_drawers_native_menu_key_tests` and five Qt test files off the panel.
- A draft held by a submitted review still offers the Local segment, which is then refused with its reason; disabling it there is a possible refinement.

## Owner look v002 filed (2026-11-13)

Session `3fefa3666ff9e2c2fc0943c4672f8b1e` (state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v002-2026-11-13/state`, cursor 12), frozen root `/Users/jaeger/comments-absorb-notes-v002._jfvvf9h` from `6ffcf4cce274`, launcher check `ATTENDED_LOCAL_COMMENTS_SHOWN:6`. One Accept/Reject each: `local-edge-chips`, `reply-local-state`, `local-reply-actions`, `local-edit-and-delete`. Not waited on.
