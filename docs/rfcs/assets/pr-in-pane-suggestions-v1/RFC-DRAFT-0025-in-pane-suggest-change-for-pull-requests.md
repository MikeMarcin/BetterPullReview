---
id: "RFC-DRAFT-0025"
title: "Suggest change edits the head file in place in pull request review"
status: "Proposed"
metadata: {"origin": {"date": "2026-10-21", "channel": "owner chat, relayed by the coordinator"}, "card": "BPR-PENDING-AF", "revision": "r1 2026-10-21", "authoring": "author-rfc; frontend-design:frontend-design loaded for the design frames", "design_frames": "docs/design/BPR-PENDING-AF-in-pane-suggest-v1/index.html", "owner_questions": "Q1-Q5 open; asked as a fresh attended-review session (see the card)"}
---

# Suggest change edits the head file in place in pull request review

## Problem and grounded current behavior

The owner reported, verbatim (2026-10-21):

> It seems like the suggest change logic from the local project where we can
> type our change into the main code pain doesnt exist for the pr review ux

**Local projects (Review Cycles).** Suggest change makes the whole captured
file editable in the main pane from the caret
([RFC-BPR-0055, owner amendment of 2026-10-01](../../RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md#owner-amendment-2026-10-01-suggest-change-edits-the-file-in-place);
[[BPR-1156]]). The pieces are:

- `core::suggestion_text_editor` (`src/core/include/bpr/core/suggestion_text_editing.hpp`):
  exact lines with their own endings, several carets, box selection with
  virtual space, undo, and Enter indentation (`indentation_for`).
- `core::derive_edit_hunks`, `edit_hunks_problem` and `preview_edit_hunks`
  (`review_cycle_suggestion.hpp`). Each separate changed run of whole lines
  becomes one exact suggestion in the original file's coordinates. Runs are
  separated by at least one unchanged line. The bounds are 1 MiB per file,
  64 changes and 64 KiB per change.
- `qt::suggestion_editor`, the cycle page's reading view, which turns
  editable without changing anything on screen. `qt::suggestion_composer`
  is the tray beside it: location, rationale, exact or illustrative, and a
  before/after preview in the diff fills.
- `review_cycle_view::begin_suggestion` and `end_suggestion`
  (`review_cycle_view.cpp:1690-1730`). The session is held in memory, with
  Save suggestion and Cancel (Esc). Nothing is written to the checkout.

[[BPR-1212]]'s Q2 kept this editor for local projects ("B: whole-file
editor"). Pull requests kept a block in the drawer.

**Pull requests.** Suggest change (Shift+Return, the code's right-click, or
the "Suggest change on lines a–b" item from [[BPR-1721]]) starts a drawer
draft whose body is a ```` ```suggestion ```` block holding the selected head
lines exactly (`core::suggestion_fence_body`, `suggestion_source_lines`;
`workbench_window::begin_new_comment`, `workbench_window.cpp:9918-9965`). The
reviewer edits the block as text in the drawer field, and "What it changes"
shows removed and added lines (`core::suggested_change_of`). The panes are
read-only `QPlainTextEdit`s. The right pane is the whole head document with no
synthetic rows ([alignment-space contract](../../../ux/diff-link-map-and-alignment-space-scrolling.md#1-layout)),
so structurally it is the same whole-file view the cycle page edits.

The surrounding rules this proposal must keep:

- **Ranges.** `core::decide_comment_range` requires several lines to be on one
  side and inside one hunk of `github_review_hunks` (three lines of context,
  inter-hunk context 1). A single right line outside every hunk is still sent,
  and GitHub may refuse it: GitHub.com answers 422, and GHES 3.20.8 refused a
  whole review over one such comment ([[BPR-1715]]; [[BPR-1716]] is open).
- **Suggestion applicability.** `core::decide_suggestion_applicability`
  ([[BPR-1084]]; the [delivery contract's "Suggested changes"](../../../ux/conventional-comments-feedback.md))
  makes a suggestion native only on a current head line. A base line, a file
  comment, an outdated or unavailable thread, or a refused inline line make it
  text only. A suggestion is never moved to another line and never sent as a
  file comment (`incompatible_body`). BPR applies nothing; GitHub applies it.
- **Labels.** A label header is text at the start of the body. Edits replace
  only the header's bytes, and every byte of the fence stays as it was (R12,
  `core_feedback_suggestion_preservation`).
- **Filtered diffs** ([RFC-BPR-0089](../../RFC-BPR-0089-commit-scoped-review-and-generated-files.md#comments-and-line-anchoring)).
  The right side can be an earlier commit than head. A line unchanged at head
  is published at head. Otherwise the comment goes to that commit, where
  GitHub may show it as outdated, or becomes a file comment with a permalink
  ([[BPR-1706]]).
- **Heads.** Drafts are durable from their first edit and bound to their
  comparison. A draft on an earlier comparison stays read-only and names its
  head. Using it on current code takes an explicit copy
  ([review round trip](../../../architecture/review-round-trip.md#durable-workbench-drafts-rfc-bpr-0023-m8)).

GitHub's own rule fixes the delivery shape. One review comment on lines
`start_line`..`line` of the RIGHT side carries at most one useful
```` ```suggestion ```` block, and that block replaces exactly those lines.
So an edit with several separate changes is several comments.

## Proposed behavior and boundaries

### Entry and where the edit happens

Suggest change, from any entry that has it today, makes the **right (head)
pane editable in place**. The text, scroll position, line numbers and syntax
colours do not change, and the selection is selected again in the editor. The
caret is enough; no selection is needed (the RFC-BPR-0055 amendment's entry
rule). The pane is the shared multi-caret editor of RC55-07, given the diff
pane's row pitch, number gutter and the head's added and changed row fills.
No second editor is written. A thin write-blue edge marks the pane as being
edited, and a blue gutter bracket marks each changed run. This is the
drawer's "amber is where you read, blue is where you write".

- **The left (base) pane stays read-only.** Suggest change on a base line
  stays unavailable, with BPR-1721's "Suggest change: head side only".
  GitHub applies suggestions to the head only.
- **The link map and the left pane keep their alignment** through the edit's
  line mapping, which the core derives from the runs. Ribbons that touch an
  edited run are drawn dimmed until the edit ends. The drawer, Line details
  and Find keep working on the original head.
- **One pane** cannot hold an edit among interleaved removed rows. What it
  shows while editing is Q4.

### Extent: what one edit produces

*Q1 decides this section.* As proposed, the whole head file is editable, as in
a local project. Each separate changed run of whole lines
(`derive_edit_hunks` against the head blob's exact bytes) becomes **one
GitHub suggestion comment** on that run's lines. A new core decision,
`plan_pull_request_suggestions`, turns each run into its GitHub shape:

- **Anchor.** A replaced or deleted run anchors to its own original lines:
  `start_line` and `line`, `side: RIGHT`, with one line using `line` alone.
  A deletion is an empty block. A pure insertion has no original line. It
  anchors to the unchanged line above it, and its block repeats that line
  followed by the new lines. At the top of the file it anchors to line 1, and
  the block ends with line 1. Of the two neighbours, it uses the one inside
  the same hunk.
- **Block.** The block holds the run's replacement lines without their
  endings, fenced by `suggestion_fence_body` (a longer fence when the code
  holds backticks).
- **Placement.** Each run is decided with the existing rules: one hunk of
  `github_review_hunks` (`decide_comment_range`), a current head line
  (`decide_suggestion_applicability`), and for filtered diffs the head mapping
  of `place_filtered_comment`. A run is **placeable** only if all three agree.
  Otherwise it is marked with one reason: outside the changed regions, across
  two changed regions, changed again after the filtered commit, a change of
  line endings only, or not UTF-8. What happens to an unplaceable run is Q3.
- **Bounds.** These are reused unchanged: 1 MiB to open, 64 changes and
  64 KiB per change to save. The composer says why instead of cutting
  anything.

### The drawer, linked to the edit

The drawer shows the draft as today: header, label chip, prose field, foot.
The changed run's lines appear in the header ("Suggested change, lines
a–b"). While the code is being edited:

- **The block lives in the code.** The drawer field holds the prose (and
  label header) only. Under it, "What it changes" shows each run's removed
  and added rows in the diff fills, from `preview_edit_hunks`. The body
  GitHub receives is composed by the core: the prose bytes exactly, then the
  block. R12 stays satisfied because the header and prose are never
  rewritten. Whether the drawer may still prefill and edit a block as text is
  Q5.
- **Several runs.** Each run is its own draft card at its own lines, with its
  own blue bracket. Whether each one has its own prose is Q2.
- **Selecting.** Selecting a draft card scrolls the pane to its run, and
  moving the caret into a run selects its card.
- **The actions are the drawer's own:** Add single comment, Start a review,
  Add review comment, and Command+Return for the main action. They act on
  every placeable draft of the edit as one set, in file order. A set holding
  an unplaceable run follows Q3. Publishing goes through today's
  publication path. Nothing new reaches GitHub.

### Lifecycle

- **Durability.** No change is lost once it has settled. The edit's drafts
  are written through the existing durable draft path, the first settled
  edit allocating them, and their bodies carry the blocks. A file switch,
  closing the window or a crash keeps them as kept drafts.
- **Leaving the edit.** Esc, Close, or a click into the base pane with no
  selection or extra carets left ends the edit. With no change made, nothing
  is kept. Otherwise the drafts are kept, as the drawer's rule says, and the
  pane returns to reading with each kept suggestion bracketed at its lines.
- **Reopening.** Reopening a kept suggestion draft, or choosing Edit in code
  on it, puts every kept suggestion draft of that file on the same comparison
  back into the pane. The core rebuilds the edited text by splicing each
  block over its lines. This is exact, and it is refused with a reason when a
  block no longer parses or two drafts' lines overlap. The draft then stays a
  text draft.
- **Undo.** The editor's ordinary undo applies (256 steps). Deleting one
  run's draft from the drawer reverts that run's lines in the pane as one
  undoable step. **Delete** on the edit asks first when it holds more than one
  change ("Delete 3 suggested changes?"). It deletes them all and leaves the
  pane reading.
- **Runs that change shape.** When typing merges two runs, their drafts merge
  and the prose is joined by a blank line, earlier first. When a run is typed
  back to the original, its draft keeps any prose as a plain comment on those
  lines and says so. With no prose it disappears.

### A new head while editing

The edit is bound to the head blob and comparison it started on, and it never
moves. When a newer head arrives, the drawer says "New commits arrived. This
change is against `<short OID>`." The pane keeps editing the old text.
Following the established rule, the drafts stay on their comparison. **Carry
to the new head** is the explicit copy gesture. It is offered only when the
core finds every run's original lines unchanged at the new head (exact, from
the old-to-new head correspondence, never fuzzy). It makes new drafts there,
and the old ones are released. Otherwise the drawer says the file changed
where you edited, and offers Copy change.

### Ownership

- **Core** derives the runs (reused), plans each run's GitHub anchor and
  block, decides placeability and its reason, composes bodies from prose and
  blocks, maps lines through the edit for the panes, rebuilds an edit from
  kept drafts, and decides Carry. All of these are pure decisions over values
  the host already holds.
- **Host** persists the drafts, supplies the head and new-head documents and
  their correspondence, and publishes through today's path.
- **Qt** swaps the right pane's reading view for the shared editor at the
  same geometry, paints brackets, fills and the dimmed ribbons, and links
  cards to runs. It decides nothing about anchors or placement.

### Non-goals

- Editing the base side.
- Applying a suggestion locally or on GitHub from BPR.
- Suggestions on replies, which GitHub anchors to their thread.
- Review Cycles' own editor. It is reused, not changed, apart from the gutter
  and the fills it gains.
- File comments that carry a ```` ```suggestion ```` block. Q3's B sends a
  ```` ```diff ```` block, which is text.

### Alternatives considered

- **Make the diff pane's `QPlainTextEdit` editable.** This was rejected. It
  would fork RC55-07's editing behaviour into a second editor, and the pane's
  measurement, Find and marks assume read-only text.
- **Edit only the selected lines** (GitHub's own model). This is Q1's
  alternative. It is simpler, but it is the limit the owner rejected for
  local projects ("I don't want to be limited to changing the lines I
  initially selected").
- **One comment holding several blocks.** This was rejected. GitHub's blocks
  all replace the same commented lines, so the extra blocks could not apply.

## Owner questions

Each question is asked once, in a fresh short attended-review session (the
card records the session). Until a question is answered, the scope it affects
is not ready.

- **Q1 (Accept or Reject). Extent.** Suggest change makes the whole head file
  editable, and each separate change becomes its own suggestion comment on its
  lines. If rejected, the edit is limited to the selected lines, and typing
  outside them is refused.
- **Q2 (A or B). Prose with several changes.** A: each change is its own
  draft with its own words. B: one set of words goes on the first change, and
  the others carry their block with "Part 2 of 3" and a link to the first.
- **Q3 (A or B). A change GitHub cannot take as a suggestion** (outside the
  changed regions, or changed after a filtered commit). A: the pane marks it
  and the drafts cannot be sent until it is undone. B: it is sent as text: a
  file comment (RFC-BPR-0022, as [[BPR-1706]] does for filtered lines) with
  a permalink to its lines and the change as a ```` ```diff ```` block. The
  mark says that GitHub cannot apply it. It is never moved to another line.
- **Q4 (A or B). One pane.** A: while editing, the one pane shows the head
  file alone, with its added rows still green, and the interleaved comparison
  returns when the edit ends. B: Suggest change switches to two panes for the
  edit and back afterwards.
- **Q5 (Accept or Reject). Replace the drawer block.** Suggest change always
  edits in the code, and the drawer never prefills a suggestion block. A
  fence typed or pasted into a comment still works as text, as today
  ([[BPR-1084]]). If rejected, the drawer also offers "Edit as text", which
  turns the linked edit into today's block field.

## Acceptance

- **AC01:** From Shift+Return, the right-click item and the review bar, on a
  selection or the caret alone, the right pane becomes editable with
  unchanged text, scroll, numbers and colours, and the selection kept. The
  base pane and base-line Suggest change stay as BPR-1721 left them.
- **AC02:** RC55-07's gestures (multi-caret, box with virtual space, IME,
  undo, clipboard, Enter indentation) work in the pane on the composed
  application. They are checked with the existing editor courses run against
  the pane.
- **AC03:** For a fixture edit with a replacement, a deletion, an insertion
  at the top, an insertion in the middle and a block containing backticks,
  the published comments' `start_line`, `line`, `side` and bodies match the
  core's plan byte for byte, in the fixture service and on BPR-Test. GitHub's
  "Commit suggestion" then gives exactly the edited file.
- **AC04:** Unplaceable runs are marked with their reason and handled as Q3
  decides. No run is ever moved to a line other than its own, or its
  insertion neighbour.
- **AC05:** The drawer shows the prose and "What it changes" for each run as
  the "drawer, linked to the edit" section says. A label chip edit changes
  only the header's bytes of the composed body
  (`core_feedback_suggestion_preservation` extended to composed bodies).
- **AC06:** Settled changes survive a file switch, closing the window and a
  killed process. Reopening rebuilds the same edited text exactly, and an
  overlap or unparsable block is refused with its reason.
- **AC07:** Undo, deleting one run's draft, Delete on the whole edit and
  leaving the edit behave as "Lifecycle" says, including merges and runs
  typed back to the original.
- **AC08:** With a new head arriving mid-edit, the edit does not move. Carry
  is offered exactly when every run's original lines are unchanged at the
  new head, and Copy change is offered otherwise.
- **AC09:** On a filtered diff whose right side is not head, runs unchanged
  at head are placeable, and others take Q3's handling with the "changed
  after" reason.
- **AC10:** The owner accepts the built in-pane edit on a frozen build, one
  question per look criterion: entry and pane continuity, the drawer link,
  several changes, and one pane.

## Milestones (proposed)

1. **M1 Core.** `plan_pull_request_suggestions` (anchors, insertion rule,
   placement reasons), body composition with the prose preserved, the edit's
   line mapping, rebuilding from drafts, and the Carry decision, with tests.
   AC03 and AC05 (core share), AC06 (rebuild), AC08 (decision) and AC09.
2. **M2 Qt pane.** The shared editor over the right pane at the same
   geometry: the gutter, fills, brackets, the dimmed ribbons, left-pane
   alignment, the keys and entries, and one pane per Q4. AC01 and AC02.
3. **M3 Drawer and lifecycle.** Linked draft cards, prose per Q2, the
   unplaceable handling per Q3, durability, reopening, deletion, a new head,
   and the composed course on `conventional-comments-v1`. AC04 to AC09.
4. **M4 Live and attended.** A BPR-Test course (publish, then apply on GitHub
   through `gh`), a frozen build and the owner's look. AC03 (live) and AC10.

The work is about the size of [[BPR-1156]] plus BPR-1212's slices: five or
six cards, with M2 and M3 the bulk. Expect two or three attended look rounds,
judging by BPR-1156's history.
