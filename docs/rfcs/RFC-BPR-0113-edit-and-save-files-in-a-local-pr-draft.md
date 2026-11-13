---
id: "RFC-BPR-0113"
title: "Edit and save files in a local PR draft"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the local-draft-authoring-v1 pack, where it was RFC-DRAFT-0001","inspected_revision":"5380330fb1e1f15f5a3da476e0e8260ba4a730a3","milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"baseline":"b0d54697a6db6b17b459a751515de60aff14d65d"}
depends_on: ["RFC-BPR-0108"]
---

# Edit and save files in a local PR draft

## Problem and grounded current behavior

A local PR draft shows the author what they propose to merge, but a one-line
correction found while reading it means leaving BetterPullReview.
[RFC-BPR-0108](RFC-BPR-0108-local-pr-drafts.md) treats every change to
the worktree as something done elsewhere: the author can "return to an
editor", and BPR captures the result as a successor candidate.

The owner asked for editing in place
([notes 1 to 3](assets/local-draft-authoring-v1/SOURCE_NOTES.md)): the local draft "should allow us to edit
the files", using the editing interface the application already has, for
"minor self edits".

The inspected code has the editing half and none of the writing half:

- The editing model is in core
  ([suggestion_text_editing.hpp](../../src/core/include/bpr/core/suggestion_text_editing.hpp)):
  exact text, several carets, box selections, virtual space and undo. Text is
  UTF-8 kept as lines with their own endings, so CRLF, LF, tabs and a missing
  final newline survive every edit. It is bounded at 1 MiB of text, 1024
  carets and 256 undo steps, and a larger file "is refused before it is
  opened, never truncated". Its header calls it "a deliberately small
  code-proposal editor, not an IDE".
- The Qt editor over that model
  ([suggestion_editor.hpp](../../src/adapters/qt/include/bpr/qt/suggestion_editor.hpp))
  already shows a whole file with its syntax colours on the Review Cycles
  page and edits the lines of Suggest change. Neither use writes a file.
- The workspace adapter reads a chosen local root "without following links,
  writing or running Git" ([root guide](../../AGENTS.md)), and a
  widget or Qt model may perform no filesystem effect.

## Proposed behavior and boundaries

### Edit opens a file of the draft

In a local draft's Changes area, a file the draft lists offers **Edit**: a
file changed on the branch, or a listed working edit, selected or not. Edit
opens the whole file as it is on disk in the existing code editor, with every
gesture that editor has today. Nothing else in the application gains Edit: a
pull request review keeps Suggest change, and Review Cycles is unchanged.

When the bytes on disk are not the bytes the current candidate holds for that
file, the editor says so before the first edit: the file has changes on disk
that are not in the draft, and saving will offer the whole file to it.

A file is not editable, and Edit says which of these is why, when it is:

- deleted, a directory, a symbolic link, a submodule, or outside the draft's
  worktree root;
- not valid UTF-8, or holding a NUL byte;
- larger than the editing model's bound of 1 MiB;
- captured with a filter, LFS or capture-quality qualification under the
  material contracts RFC-BPR-0108 preserves, so that the bytes on disk are not
  the bytes Git stores.

A refused file is never opened truncated or converted.
Creating, deleting and renaming files, and editing a file the draft does not
list, are outside this RFC.

### Save

**Save** writes the editor's exact text to that one file and changes nothing
else. Line endings, tabs and the presence or absence of a final newline are
written as edited. The file's mode is kept. Nothing is staged or committed.

Save writes only when the file on disk still holds the bytes the editor read
when it opened, or wrote at its last save. When it does not, nothing is
written; the editor keeps the author's text and offers to reload from disk or
to keep editing. A save that fails or is interrupted leaves a complete file,
the old one or the new one.

Undo continues across a save. Leaving the editor, the draft or the
application with unsaved edits asks first: Save, Discard or Cancel. Unsaved
edits are not kept across a crash or a forced quit; this is stated in the
editor's unsaved-changes wording, not discovered.

### A save is a worktree change like any other

A saved file is an ordinary change to the worktree. The draft offers the
refresh RFC-BPR-0108 defines, at once, with the saved file already selected;
several saves before a refresh are one offer. Accepting makes the successor
candidate and retains the previous one. Until then the current candidate and
everything attached to it are unchanged, and the file's row is marked as
edited since the candidate. Notes, evidence and tour stops on changed lines
follow the stale rules those RFCs already define.

### Architecture

Core owns the editing model, the save decision (the identity comparison, the
refusal reasons and the typed outcome) and the editable-file policy. The
application performs the write through a new, narrow port that writes one
named regular file beneath a local draft's worktree root, follows no link and
refuses any other path. The existing workspace adapter stays read-only. Qt
presents and performs no file effect. `architecture_boundaries` keeps the
write port to the save here and the Revert of
[the change-run RFC](RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md).

Proposals in this RFC that the owner has not separately confirmed are listed
in the [decision record](assets/local-draft-authoring-v1/DECISIONS.md#decisions) as P05 and P06.

### Owner amendment, 2026-11-12: Edit is a view, in author state

The owner, choosing the pull request window for local drafts
([accepted design](../design/local-draft-pr-window-v1/README.md)):

> Local draft is more of a state than a mode since we're talking about letting
> this same mode be used for addressing PR feedback. Maybe that takes the form
> of a no-diff view option (like we have 2 pane and single pane) maybe instead
> of the 1 button toggle this current is it becomes a 2-icon pick 1 normally
> and a 3 icon-pick one if you're in author/prepare mode.

Changed requirements; everything not named here stands.

- **Edit is the third view of the review bar's view picker**, not a button in
  a Changes area. The picker is a segmented pick of two icons (two panes, one
  pane) in a review and three in author state: a window on the author's own
  worktree, which is a local draft or a pull request opened by its author.
  "Nothing else in the application gains Edit: a pull request review keeps
  Suggest change" becomes: a review of someone else's pull request, or of any
  pull request not opened on the author's worktree, has no Edit.
- **What Edit shows.** The worktree's file, as on disk, in the existing code
  editor, with no diff. The file header carries a dot by the name, Not saved,
  and Save. The gutter keeps the comparison: marks for added, changed and
  removed lines against the comparison the window shows, dashed until saved.
- **Changed on disk.** When the file changed on disk since it was read, Reload
  takes Save's place and nothing is written (AC02 unchanged in substance).
- **A file that cannot be edited** dims the third icon, and its tooltip gives
  this RFC's reason (AC03 unchanged).
- **After a save** the file's row and the Local Changes tab's counts follow
  the worktree; the successor-candidate offer of AC04 is the candidate menu's
  Refresh, which a local draft keeps. In a pull request opened by its author
  there is no candidate, and a save is simply a worktree change.

AC01 reads "In author state, Edit …"; AC06's clause "a pull request review
offers no Edit" reads "a review not in author state offers no Edit"; AC07
reads: on a frozen build serving a local draft, the owner accepts the view
picker, the Edit view, its not-editable reasons and its changed-on-disk state.

Delivery: pulled forward from the local-draft-authoring plan into
`v0-5-0-prerelease-targets-v1` on lane
`claude/v0-5-0-prerelease-targets-52265d` (BPR-2062, BPR-2063, BPR-2064), so
the authoring pack does not build it twice.

## Acceptance criteria

- **AC01:** In a local draft, Edit on a listed file opens the whole file in the existing code editor with several carets, box selection, virtual space and undo, and a saved edit is on disk byte for byte as edited, including CRLF and LF endings, tabs and a missing final newline, with the file's mode unchanged and nothing staged or committed.
- **AC02:** Save writes only when the file on disk is what the editor last read or wrote. A file changed on disk since is not overwritten, the author's text is kept, and reload and keep-editing are offered. An interrupted save leaves a complete old or new file.
- **AC03:** A deleted file, a link, a submodule, a path outside the root, a non-UTF-8 or NUL-bearing file, a file over 1 MiB and a filter- or LFS-qualified file each refuse Edit with that specific reason, and none is opened truncated or converted.
- **AC04:** After a save the draft offers one refresh with the saved files selected. Accepting creates a successor candidate holding the saved bytes and retains the previous candidate; before that, the current candidate, its notes, evidence and tour are unchanged.
- **AC05:** Leaving the editor, the draft or the application with unsaved edits asks first, and Discard writes nothing.
- **AC06:** Opening, reading and resuming a local draft without pressing Save writes nothing to the worktree; a pull request review offers no Edit; and no Qt widget or model performs the write.
- **AC07:** On a frozen build serving a local draft, the owner accepts the editor in Changes, its not-editable reasons and its changed-on-disk state.

## Proposed delivery

[Save policy and write port](../issues/60-done/BPR-2062-save-policy-and-worktree-write-port.md),
[the editor in Changes](../issues/10-backlog/BPR-2063-the-editor-in-changes.md) and
[closeout](../issues/10-backlog/BPR-2064-close-rfc-0113.md) form the first
[milestone](plans/local-draft-authoring-v1/milestone-plan.json). They need the local draft's durable
candidates from RFC-BPR-0108 to exist first.
