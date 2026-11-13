# RFC-BPR-0113 verification

Pulled forward with RFC-BPR-0114 by the owner's redesign of 2026-11-12
([accepted design](../design/local-draft-pr-window-v1/README.md); amendment in
the [RFC](../rfcs/RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md)).
Cards [[BPR-2062]], [[BPR-2063]], closeout [[BPR-2064]].

## What was built

Edit is the third view of the review bar's view picker in author state
(`view_picker`, its glyph a frame with a text caret). It shows the worktree's
file in the existing code editor (`suggestion_editor`) with no diff; the file
header has the dot, Not saved and Save (Reload when the file changed on disk);
the gutter marks added and changed lines and a wedge where lines went, dashed
while unsaved (`suggestion_editor::set_gutter_marks`, marks from
`core::line_marks`). Core `decide_editable` (deleted, not a file, link,
submodule, outside, over 1 MiB, not UTF-8, NUL, filtered) and `decide_save`
(write, changed on disk, unchanged). The write is `worktree_file_port`
(see RFC-BPR-0114's ledger). A refused file dims Edit with the reason as its
tooltip.

## Criteria (as amended)

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `local_changes`: Save writes CRLF and no final newline byte for byte; desktop course: Edit shows the file, an edit enables Save, Save writes it, nothing is staged | Pass for bytes; carets, box selection and undo are the existing editor's, not re-verified |
| AC02 | `local_changes`: a file changed on disk is not overwritten and the decision is `changed_on_disk` (Reload offered); the write is temporary file plus rename | Pass; an interrupted save has no course |
| AC03 | `core_local_changes`: each refusal | Pass in core; not every refusal is driven through the window |
| AC04 | A save is a worktree change; the candidate menu's Refresh records a successor | Built; no course saves then refreshes |
| AC05 | Leaving with unsaved edits does not ask yet | Not built |
| AC06 | Opening reads only; no Qt widget writes (the write is the host's through the port); a review not in author state has two segments and no Edit (`qt_workbench_window_*`) | Pass |
| AC07 | Owner look filed | Pending owner |

## Owner verdicts (attended session 6d00826a4d9c2ee08fe91321c2c1d521, handed back 2026-11-13T03:52:51Z)

Owner words verbatim. The frozen root `/Users/jaeger/local-draft-pr-window-v002.lyqc133b`
(from `be709f8e6c6a`) and host 75022 were removed after hand-back.

- `local-draft-pr-window`: **Reject** (decision `8fe04b44`, 2026-11-13T03:50:35Z, screenshot attached in the session)
> This whole local draft view just looks kind of amateur compared with the PR view, I thought we chose to stick with the PR view and just augment it. This seems like we tried to rebuild it and didn't do a great job.
- `home-1`: **Accept** (decision `753d927e`, 2026-11-13T03:41:14Z, screenshot attached in the session)
> Looks good except there's something weird with selection. I can select both local draft and recently viewed and escape doesnt clear selection of either.
- `view-picker-and-edit`: **Reject** (decision `a4b84680`, 2026-11-13T03:52:47Z, screenshot attached in the session)
> Seems like we have no syntax highlighting. This useless files tab doesn't even update the file in the edit pane.
- `local-changes-commit-push`: **Reject** (decision `2c74ac41`, 2026-11-13T03:49:58Z, screenshot attached in the session)
> I don't know why Local Changes tab doesn't show up untless I go to a drop down and say show local changes. That drop down option seems unnecessrry just have the tab always. I also don't know why we don't have the search tab for find in files.
>
> I actually don't know what this files tab is for, seems just strictly worse than the files tab we already have for PRs. And strictly worse than the Local Changes tab.
> The average user won't need htis information, can hide it behind a disclosure.
- `agent-commit-message`: **Reject** (decision `016d90c7`, 2026-11-13T03:39:45Z, screenshot attached in the session)
> Nib isn't using agent accent color. And I think it belongs next to commit not subject.
>
> This tooltip box feels extra long. Actually severaal tooltip boxes feel super long.

What follows: the separate local-draft page is replaced by the pull request
window itself, with author-state additions only (round "local draft in the PR
window", 2026-11-12). Home 1 is accepted except the selection fault, which the
same round fixes.

## Local draft in the pull request window (2026-11-13)

The Edit view, Local Changes, staging, commit and push are now the pull request window's additions; see the [RFC-BPR-0108 ledger](RFC-BPR-0108.md#local-draft-in-the-pull-request-window-2026-11-13). Edit has syntax colours and follows the file chosen in Files, Local Changes or Search; the agent's nib is beside Commit in the agent accent (`qt_local_draft_window` holds it, with an accent inventory of Local Changes and the one-sentence tooltips).

## Owner verdicts (attended session 767b92339e278ea303b3836f21426fd3, handed back 2026-11-13T06:41:17Z)

All five looks accepted, two with notes; verbatim in the
[RFC-BPR-0108 ledger](RFC-BPR-0108.md#owner-verdicts-attended-session-767b92339e278ea303b3836f21426fd3-handed-back-2026-11-13t064117z).
The notes are follow-up [[BPR-2156]].
