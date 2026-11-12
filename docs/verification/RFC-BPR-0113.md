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
