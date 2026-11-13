---
schema: "implement-rfc.card.v1"
id: "BPR-2159"
title: "Threads' state filter, Overview's Local chip, My review and Finish your review"
status: "Done"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2158"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":["AC07","AC08","AC09"],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
acceptance: [{"criterion":"AC07","outcome":"pass","revision":"b0ea75ce8","evidence":{"kind":"command","detail":"qt_conversation_finder_scale_{100,125,200} test_my_drafts (Local rows in their files' groups; four choices show exactly their rows with matching counts; Only new and Hide resolved hide no Local under Local; reopening starts on All); application_local_comments_found_public_path AC07"}},{"criterion":"AC08","outcome":"pass","revision":"b0ea75ce8","evidence":{"kind":"command","detail":"qt_pull_request_overview_page test_local_chip_and_my_review_by_state; application_local_comments_found_public_path AC08 (Local chip beside drafts chip, folder sum, activation lands on Threads on Local at the file's first, no Note column, no Private notes row)"}},{"criterion":"AC09","outcome":"pass","revision":"b0ea75ce8","evidence":{"kind":"command","detail":"qt_review_submission_editor_scale_{100,125,200}; application_local_comments_found_public_path AC09 (no Local pending or not in the review; Keep Local removes the row; the Local line's count, Show, absent at zero)"}}]
---

# BPR-2159: Threads' state filter, Overview's Local chip, My review and Finish your review

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `finding-and-counting` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first. Threads: My drafts becomes the All, Published, Draft, Local state filter with counts (not remembered; Draft carries the amber dot); Local rows in their files' groups; Find a thread searches Local. Overview: the Local chip beside the drafts chip, a folder's sum, activation opens Threads on Local at the file's first; the Note column and Private notes row go; My review counts comments by state. Finish your review: Local never pending or not in the review; Keep Local on a not-in-review row; one quiet "N local comments stay on this computer" line with Show, absent at zero. Insights' Private notes tile is renamed Local comments (Q13).

## Acceptance

- **AC07:** Threads lists Local comments in their files' groups and its state filter's four choices show exactly the rows of that state with matching counts; the filter is not restored on reopening; under Local, Hide resolved and Only new hide no Local comment.
- **AC08:** A file's Comments cell on Overview shows a Local chip with its count beside the drafts chip, a folder shows the sum, the chip's activation opens Threads on Local at that file's first, and Overview has no Note column or Private notes row.
- **AC09:** Finish your review never lists a Local comment as pending or as not in the review; Keep Local on a not-in-review row removes that row; the Local line shows the count and is absent at zero.

## Notes

Ends in an owner look (Accept or Reject) on a frozen build whose fixture holds Local, Draft and Published comments.

## Progress (2026-11-13)

Frontend-design was loaded first. Lane commits `2fa8c82fa` (the parked WIP, cherry-picked), `9543551e2`, `406e9934f`, `b0ea75ce8`.

Built:
- **Threads' state filter.** My drafts became one chip that opens four exclusive choices: All, Published, Draft and Local, each with its count. The chip names its choice and count ("Local 2") and keeps the width of its widest choice, so nothing beside it moves when the choice changes. Draft carries the amber dot in the menu while a draft is not in the review. Counts come from the rows each choice shows. Under Draft or Local no other filter hides one of mine. The filter is not remembered: a pull request opened again starts on All.
- **Local rows.** My Local comments on a file are rows in their file's group, in line order, marked with the computer and "Local" or "Local reply". A pull request Local comment is not a Threads row. Find a thread searches them. A Local row's pick opens its card in Review, at its file and line. A Local row says "your local comment, only on this computer" to a screen reader.
- **Row menu.** My own rows add Make Draft (on a Local row) or Keep Local (on a saved draft). A published thread offers neither. Make Draft from a list first opens the comment at its line, then decides against that file's comparison.
- **Overview.** The Comments cell shows a Local chip (the computer, dashed as the drafts chip is, and the count) just left of the drafts chip. A folder adds up its files. A click or Enter opens Threads on Local at that row's first. The Note column is gone; no column fills its place, as a native list leaves the space after its last column, so File keeps a dragged width.
- **My review** counts my comments by state: In review, Not in review, Local comments. The Private notes and Pending comments rows are gone. The application projection's per-row note join is removed.
- **Finish your review.** A Local comment is never listed as pending or as not in the review. A not-in-review row's menu offers Keep Local (tooltip "Only on this computer"). One quiet line, "N local comments stay on this computer", with Show, opens Threads on Local; it is absent at zero.
- **Keep Local from a list** keeps a draft whose comparison is not shown: its file is the roster's path, and it keeps its comparison (outdated elsewhere, `unmatched`, until that comparison shows it).
- **Insights' tile** is Local comments ("No local comments yet"), counting the same history (Q13).
- **Found on the way:** Threads listed the draft open in the drawer twice, as its saved row and as an unsaved row; the window never passed the open draft's identity. It does now, and the Threads draft row follows the roster.

Evidence (dev, Debug, macOS):
- `qt_conversation_finder_scale_{100,125,200}` `test_my_drafts`: Local rows in their file's group in line order; each of the four choices shows exactly its rows with its count; the chip keeps its width; under Draft and Local, Only new and Hide resolved hide none; search finds a Local comment; Make Draft and Keep Local in my rows' menus, neither on a thread; `show_state` at a folder's first; reopening starts on All.
- `qt_pull_request_overview_page` `test_local_chip_and_my_review_by_state`: the Local chip beside the drafts chip, a folder's sum, activation names the row and opens no file, a tooltip of 60 characters at most, no Note column, My review by state, no Private notes row.
- `qt_review_submission_editor_scale_{100,125,200}`: Keep Local in a not-in-review row's menu names its draft; the list leaves it; the Local line's count, Show, and absence at zero.
- `application_local_comments_found_public_path` (new, composed): AC07 Threads' rows and the four choices' counts; Keep Local from a Threads row and Make Draft back from its Local row; AC08 Overview's Local chip, its folder, its activation landing on the file's first Local row, no Note column, My review; AC09 Finish your review without the line at zero, with "3 local comments stay on this computer", Keep Local removing the row, and Show. Nothing sent.
- `application_local_comments_composition_public_path`: AC13's Threads counter counts threads, not Local rows.
- Also passing: `application_pull_request_overview`, `qt_drawer_surfaces_fit_scale_{100,125,200}`, `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`, `application_review_submission_composition_public_path`, `application_progressive_open_public_path`, `application_profile_multi_window_public_path`, `application_insights_public_path`, `application_review_round_trip_composition_contract` (pin resealed), `application_composition_source_contract`, `architecture_boundaries`. Format gate passes.
