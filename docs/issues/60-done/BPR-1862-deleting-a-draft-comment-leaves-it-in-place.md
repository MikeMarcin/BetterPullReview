---
schema: "implement-rfc.card.v1"
id: "BPR-1862"
title: "Deleting a draft comment leaves it in place"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","found_by":"Owner report, 2026-11-04, relayed by the coordinator: deleting a draft comment does not delete it, including off-hunk and agent-drafted comments; an agent turn kept saying Go to draft","reservation":"docs/issues/_reservations/BPR-1862--BPR-1862--839b7876778e.json","related_cards":["BPR-1345","BPR-1735","BPR-1773","BPR-1861"]}
acceptance: [{"criterion": "AC1 A confirmed Delete comment removes the draft from the drawer, the notes and the store, on a line inside GitHub's diff and on one outside every hunk, and it stays gone with its line chosen again", "outcome": "pass", "revision": "cd15acd32", "evidence": {"kind": "command", "command": "ctest --preset dev -R application_draft_delete_contract --repeat until-fail:3", "detail": "line and off-hunk courses on conventional-comments-v1, src/engine/renderer.cpp: DRAFT_DELETE_EVIDENCE:11111 each. Before the fix both ended failed=not-deleted with state=discarded stored=0 and card draft:unpublished[<words>] still drawn."}}, {"criterion": "AC2 An agent turn's Draft comment can be deleted; the turn lets the deleted draft go and Draft comment pressed again makes a new draft; a turn's draft taken into a review and deleted from Finish your review is let go too, while a draft nobody deleted is kept", "outcome": "pass", "revision": "cd15acd32", "evidence": {"kind": "command", "command": "ctest --preset dev -R application_draft_delete_contract --repeat until-fail:3", "detail": "agent course: DRAFT_DELETE_EVIDENCE:1111111. Before the fix the first Draft comment was refused with 'resolve the draft open at that line first' although it had saved the draft (the callback compared the placeholder ID), and the delete ended failed=not-deleted."}}, {"criterion": "AC3 After a restart over the same data root none of the deleted drafts comes back, and the kept one is still there", "outcome": "pass", "revision": "cd15acd32", "evidence": {"kind": "command", "command": "ctest --preset dev -R application_draft_delete_contract --repeat until-fail:3", "detail": "reopened course: DRAFT_DELETE_EVIDENCE:11111; the agent turn's second, undeleted draft is still stored."}}, {"criterion": "AC4 A delete that cannot go ahead never ends in silence", "outcome": "pass", "revision": "cd15acd32", "evidence": {"kind": "freeform", "detail": "Source inspection at cd15acd32: dispatch_review_draft's discard refusals go through refuseDelete ('Not deleted: ...') instead of the Publish-worded messages; the drawer's confirm handler says 'Not deleted: the comment changed while you were asked' or names Discard's disabled reason through workbench_window::present_draft_delete_refusal (status bar and editor status). Not exercised by a course."}}]
---

# BPR-1862: Deleting a draft comment leaves it in place

## Request

The owner reported on 2026-11-04 that deleting a draft comment does not
delete it: the comment stays on its line. It happened for comments outside
every hunk and for comments an agent drafted, and the agent turn went on
offering Go to draft.

## What was there

- **The store deleted it, and the window did not.** The private store
  accepted the discard and the draft left the store, so it was also gone after
  the pull request was reopened. But `finish_review_draft_command` and
  `change_review_drafts` passed the discarded record to `accept_shared_draft`,
  which made it `ReviewDraft` again and showed it to the editor with state
  `discarded` and its words intact. The notes filter in
  `conversation_cards_view.cpp` excluded only published drafts, so the
  discarded draft was still drawn as the line's `draft:unpublished` card,
  "Your note, not published". Comment stayed tied to it until another line
  was chosen.
- **Off-hunk and agent drafts had the same cause.** The same failure
  happened on a line inside the diff, on an off-hunk line, and for an agent
  draft.
- **The agent path also had an ID mismatch.** A line's first draft has a
  placeholder ID until its first save gives it the store's ID.
  `promote_private_item` compared against the old placeholder ID, so Draft
  comment reported "Not drafted: resolve the draft open at that line first"
  even though it had saved the draft. The turn recorded no draft for itself,
  so nothing could release it.
- **Refusals were silent.** Every refusal message in `dispatch_review_draft`
  was written for Publish only. The drawer's confirm handler did nothing,
  with no message, when the shown draft had changed or Discard was disabled.

## Outcome

- Once a discard is accepted, `leave_deleted_draft()` re-reads the line's
  draft with `adopt_review_selection`. Nothing shows at the line, and a new
  comment there is a new draft. It runs after a discard in
  `finish_review_draft_command`, and from the `present` lambda in
  `change_review_drafts`, which covers the Finish your review delete and
  Discard review.
- `refuseDelete` words every discard refusal ("Not deleted: …"), and
  `workbench_window::present_draft_delete_refusal` makes the confirm handler
  say why when it cannot delete.
- `promote_private_item` accepts the saved draft under its new ID when it
  started from a placeholder.
- `agent_answer_draft::Drafted` records the draft the turn wrote.
  `present_saved_drafts`, which every store change goes through, clears it
  once that draft is gone or discarded, and calls
  `agent_tab_view::release_answer_draft`. The turn goes from Go to draft back
  to Draft comment, enabled.

## Verification

| Check | Result |
|---|---|
| application_draft_delete_contract (line, off-hunk, agent, reopened) | pass, then three more in a row at cd15acd32 |
| Same courses before the fix | line, off-hunk and agent each `failed=not-deleted`; agent also `agent-refused=resolve the draft open at that line first` |
| application_start_review_contract | fails at `finish-page-not-as-drawn`; see Not covered |
| clang-format 22 on every touched file | formatted |

## Not covered

- `application_start_review_contract` fails in its `finish` course at
  `finish-page-not-as-drawn`. Its `calm()` check requires Approve to be
  enabled, but the fixture viewer `fixture-author` is the pull request's
  author, and BPR-1853 (already on main) disables Approve for the author.
  This was not confirmed by a build of main. Only the app target was built,
  to keep the shared disk free.
- `application_private_draft_restore` was not run: its helper target was not
  built.
- No Qt unit test for `agent_tab_view::release_answer_draft`. The agent course
  checks the turn's record (`Drafted`), not the button's words.
- For a thread reply drafted from an agent turn, `Drafted` is recorded only if
  `ReviewDraft` is already that reply when the callback runs.
- The refusal wording (AC4) was inspected, not driven by a course.
- macOS offscreen only. Windows and Linux were not run. No live GitHub course:
  drafts are private and never reach GitHub until published.
- `scripts/verify_documentation.py` reports only `NEXT_ID: BPR-1863 rewinds
  historical high water BPR-1866`: other branches in the shared repository
  have allocated up to BPR-1866, and main's counter is behind them, not this
  card's.
