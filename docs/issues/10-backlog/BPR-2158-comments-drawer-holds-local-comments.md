---
schema: "implement-rfc.card.v1"
id: "BPR-2158"
title: "Comments absorbs Notes: Local cards, the state control, N and one gutter mark"
status: "Backlog"
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
