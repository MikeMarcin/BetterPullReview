---
id: "RFC-BPR-0090"
title: "Suggest change edits the selected head lines in place in pull request review"
status: "In Progress"
area: "BPR"
depends_on: ["RFC-BPR-0022", "RFC-BPR-0023", "RFC-BPR-0055", "RFC-BPR-0089"]
metadata: {"origin": {"date": "2026-10-21", "channel": "owner chat, relayed by the coordinator"}, "card": "BPR-1723", "revision": "r2 2026-10-21, owner answers Q1-Q5 recorded", "draft": "docs/rfcs/assets/pr-in-pane-suggestions-v1/RFC-DRAFT-0025-in-pane-suggest-change-for-pull-requests.md", "import": "docs/rfcs/assets/pr-in-pane-suggestions-v1.LIVE_IMPORT.md", "authoring": "author-rfc; frontend-design:frontend-design loaded for the design frames", "design_frames": ["docs/design/BPR-1723-in-pane-suggest-v1/index.html", "docs/design/BPR-1723-in-pane-suggest-v2/index.html"], "owner_questions": "Q1-Q5 answered 2026-10-21 in attended session e5387830d18edd8d541b96e47ecbd9cb (closed)"}
---

# Suggest change edits the selected head lines in place in pull request review

In Progress, revision r2. Adopted from
[RFC-DRAFT-0025](assets/pr-in-pane-suggestions-v1/RFC-DRAFT-0025-in-pane-suggest-change-for-pull-requests.md)
([import record](assets/pr-in-pane-suggestions-v1.LIVE_IMPORT.md)) after the
owner answered its five questions. Card [[BPR-1723]] holds the phase 1
finding; the milestone cards are listed under [Milestones](#milestones).
M1 to M3 are implemented; Carry and Copy change at a newer head are
BPR-1733. M4's live course on BPR-Test #123 published the core's
planned suggestions, which GitHub took at their lines and returned
unchanged; GitHub's Commit suggestion on a CRLF file and the owner's look
are parked on the owner ([checkpoint](../issues/_checkpoints/RFC-BPR-0090-M4/handoff.md)).

## Problem and grounded current behavior

The owner reported, verbatim (2026-10-21):

> It seems like the suggest change logic from the local project where we can
> type our change into the main code pain doesnt exist for the pr review ux

**Local projects (Review Cycles).** Suggest change makes the captured file
editable in the main pane from the caret
([RFC-BPR-0055, owner amendment of 2026-10-01](RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md#owner-amendment-2026-10-01-suggest-change-edits-the-file-in-place);
[[BPR-1156]]). The pieces are `core::suggestion_text_editor`
(`src/core/include/bpr/core/suggestion_text_editing.hpp`: exact lines with
their own endings, several carets, box selection with virtual space, undo
and Enter indentation), `qt::suggestion_editor` (the cycle page's reading
view, which turns editable without changing anything on screen) and
`review_cycle_view::begin_suggestion` and `end_suggestion`.

**Pull requests.** Suggest change (Shift+Return, the code's right-click, or
the "Suggest change on lines a–b" item from [[BPR-1721]]) starts a drawer
draft whose body is a ```` ```suggestion ```` block holding the selected head
lines exactly (`core::suggestion_fence_body`, `suggestion_source_lines`;
`workbench_window::begin_new_comment`). The reviewer edits the block as text
in the drawer field, and "What it changes" shows removed and added lines
(`core::suggested_change_of`). The panes are read-only `QPlainTextEdit`s. The
right pane is the whole head document with no synthetic rows
([alignment-space contract](../ux/diff-link-map-and-alignment-space-scrolling.md#1-layout)).

The surrounding rules this RFC keeps:

- **Ranges.** `core::decide_comment_range` requires several lines to be on one
  side and inside one hunk of `github_review_hunks`. A single right line
  outside every hunk is still taken as a comment, and GitHub may refuse it
  ([[BPR-1715]]; [[BPR-1716]] is open).
- **Suggestion applicability.** `core::decide_suggestion_applicability`
  ([[BPR-1084]]) makes a suggestion native only on a current head line. A
  suggestion is never moved to another line and never sent as a file comment
  (`incompatible_body`). BPR applies nothing; GitHub applies it.
- **Labels.** A label header is text at the start of the body. Edits replace
  only the header's bytes (R12, `core_feedback_suggestion_preservation`).
- **Filtered diffs** ([RFC-BPR-0089](RFC-BPR-0089-commit-scoped-review-and-generated-files.md#comments-and-line-anchoring)).
  The right side can be an earlier commit than head. A line unchanged at head
  is published at head. Otherwise the comment goes to that commit, where
  GitHub shows it outdated, or becomes a file comment with a permalink
  ([[BPR-1706]]). A range is published only where the right side is head.
- **Heads.** Drafts are durable from their first edit and bound to their
  comparison. A draft on an earlier comparison stays read-only and names its
  head ([review round trip](../architecture/review-round-trip.md#durable-workbench-drafts-rfc-bpr-0023-m8)).

GitHub's own rule fixes the delivery shape: one review comment on lines
`start_line`..`line` of the RIGHT side carries one ```` ```suggestion ````
block, and that block replaces exactly those lines.

## Owner decisions (2026-10-21)

The five questions were asked once each in attended-review session
`e5387830d18edd8d541b96e47ecbd9cb` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261021-in-pane-suggest-v001`,
candidates `BPR-PENDING-AF-q1-extent-v001` to `BPR-PENDING-AF-q5-drawer-block-v001`
at revision `653e2ff2d`, decisions at event sequences 19 to 52). The session
was handed back with nothing pending and its host stopped on 2026-10-21.

- **Q1, extent: Reject.** The owner first accepted (sequence 17) and then
  rejected (sequence 19) the whole-file edit. The note on the rejection did
  not save; the owner told the coordinator in chat: "Only the selected lines
  are editable in place." Suggest change makes only the selected line range
  editable in the head pane, and each Suggest change is one suggestion.
- **Q2, words with several changes: A**, each change has its own words. With
  Q1 rejected an edit holds one change, so every suggestion has its own
  words by construction.
- **Q3, a change GitHub cannot take as a suggestion: B, with a note.** Note,
  verbatim: "Can we do a mix of both? Mark it and give the option to convert
  it to a file comment say on the finalize review page or on the comment
  itself?" The change is marked, and Convert to file comment is offered on
  the comment and on the Finish your review page. Nothing converts on its own.
- **Q4, one pane: A.** While editing, the one pane shows the head file.
  Note, verbatim: "I kind of want to say 2 pane with top over bottom but I
  think that's too big of a change to land here." A stacked two-pane layout
  is recorded as a separate backlog card (BPR-1731).
- **Q5, the drawer block: Accept.** Suggest change always edits in the code,
  and the drawer never prefills a suggestion block. A fence typed or pasted
  into a comment still works as text, as today ([[BPR-1084]]).

## Proposed behavior and boundaries

### Entry: which lines

Suggest change, from any entry that has it today, on a selection or on the
caret's line alone, opens an edit of exactly those head lines. The range must
satisfy the existing rules, decided by the core
(`core::decide_suggestion_range`, built on `decide_comment_range`):

- **Head side only.** A base line keeps BPR-1721's "Suggest change: head
  side only".
- **One changed region.** Several lines must lie in one hunk of
  `github_review_hunks`, as for a comment. A single line must also lie in a
  hunk: GitHub places no line comment outside the diff ([[BPR-1715]]), and a
  suggestion has no file-comment form. Add comment on such a line keeps
  today's behaviour, which [[BPR-1716]] owns.
- **Filtered diffs.** On a diff whose right side is not head, a range is
  refused, as `place_filtered_comment` refuses it for every comment. One line
  is taken, and its placement is decided as below.

A refused entry says why in the status line, in the words the menu item's
reason row already uses, and changes nothing.

### The editable range

The head pane becomes the shared editor of RC55-07 over the whole head file,
at the same geometry: text, scroll position, line numbers, syntax colours and
the head's added-row fills do not change, and the caret is at the end of the
last selected line, so typing adds to the lines rather than replacing them.
The editor follows the comparison's scroll, so a wheel over it scrolls both
panes, and moving the caret out of view scrolls the comparison to it. A thin
write-blue edge runs down the gutter beside the range
and the range's rows take the light write-blue wash, the drawer's "amber is
where you read, blue is where you write". The rest of the file stays
readable around it, so the reviewer sees the context of the change, but is
set back: its lines and the space below the last line sit on the code
gutter's ground, and in the light and dark variants their ink fades a
quarter of the way toward that ground, never below the text floor
(`core::read_only_code_ink`). The high-contrast variants keep their ink and
take only the ground. The owner asked for this on the drawer-link look
(2026-10-22): "I wonder if we can make it more obvious which parts of the
panes are read only. The rail is nice but maybe we could dim the background
the rest of the code or something."

- **Only the range is editable.** The editable span runs from the start of
  the range's first line to the end of its last line's text. Everything
  before it, the last line's ending, and everything after it are fixed. The
  caret can go anywhere, and selecting, copying and Find work outside the
  range, but an edit that would change a fixed byte is refused whole by the
  core and changes nothing. A selection that runs into fixed text, such as a
  triple-clicked last line with its line break, is clipped to the span before
  it is replaced. A caret wholly outside the span refuses. Backspace at the
  span's start and Delete at its end are refused, because they would join a
  fixed line.
- **Saying so.** A refused edit leaves the text alone, flashes the range's
  edge once, and the status line says "Only lines a–b are in this suggestion.
  Esc ends the edit." The pane never moves the caret into the range on its
  own.
- **Growing and shrinking.** Enter, a pasted line break or several pasted
  lines inside the span add rows to the range; the range grows by those rows
  and the rows below move down. Deleting lines inside the span shrinks it.
  New rows have no line number, because they have none in the head file; the
  rows below keep their head numbers.
- **Deleting the lines.** Emptying the span leaves one empty row in the
  range, which means "delete these lines" (an empty suggestion block), and
  the row says "Deletes lines a–b" in the gutter's ink. A range that was one
  empty line and is left empty is unchanged. Replacing lines with one blank
  line is not expressible; it reads as a deletion.
- **The anchor never moves.** However many rows the edit holds, the comment
  is on the selected lines, `start_line`..`line`, side RIGHT, and its block
  is the span's lines without their endings. The block's fence is
  `suggestion_fence_body`'s (one backtick longer than any run in the code).
- **The base pane stays read-only**, and its rows and the link map keep their
  alignment above the range. Below a range that grew or shrank, the head
  rows are offset by the difference while the range is in view, so ribbons
  that touch the range or lie below it are drawn dimmed until the edit ends.
  The edit scrolls without a jump: rows above the range meet their head rows,
  the range's rows pass at the rate its length differs, and rows below meet
  theirs once the range has gone above the top.
- **One pane (Q4).** In the one-pane comparison the edit shows the head file
  alone, scrolled so the range sits where its rows were, with the head's
  added rows still green. The interleaved comparison returns when the edit
  ends.
- **Bounds.** These are reused unchanged: 1 MiB to open (a larger file is
  refused before the edit opens), and 64 KiB for the change. The status line
  says why instead of cutting anything.

### One suggestion per edit

Each Suggest change is one draft and one GitHub suggestion comment on its
lines. Suggest change on other lines while an edit is open ends this edit
(its draft is kept) and opens the new one. There are no multi-run plans,
merges or "Part 2 of 3".

### The plan the core makes

`core::plan_pull_request_suggestion` turns the range and the edited span into
what GitHub receives, from values the host already holds:

- **Changed or not.** The block is compared with the original lines by
  content. An edit that changes only line endings, or one typed back to the
  original, is unchanged and plans no suggestion.
- **Anchor.** `start_line` (none for one line) and `line`, zero-based in the
  draft and one-based on the wire, side RIGHT.
- **Block and body.** The block lines, and the body the draft keeps: the
  prose bytes exactly, then a blank line, then the fence; the fence alone
  when there is no prose. `core::split_suggestion_body` reads a body back
  into its prose and block, exactly; R12 holds because the prose and its
  label header are never rewritten.
- **What it changes.** The removed and added lines, from
  `suggested_change_of`.
- **Placement.** The plan is **placeable** when the suggestion is native at
  its lines: the range passed entry and, on a filtered diff, the line is
  unchanged at head (`place_filtered_comment` places it at head). Otherwise
  it is **unplaceable** with one reason: changed after the filtered commit
  (GitHub would show it outdated, where a suggestion cannot be applied), or
  resolvable on no commit (BPR-1706's file comment).

### The drawer, linked to the edit (Q5)

The drawer shows the draft as today: header, label chip, prose field, foot.
The header says "Suggested change, lines a–b". While the code is edited:

- **The block lives in the code.** The drawer field holds the prose (and its
  label header) only, and never a block. Under it, "What it changes" shows
  the removed and added rows in the diff fills. The draft's body, which is
  what persists and what GitHub receives, is the core's composition of the
  prose and the block.
- **A typed fence is text.** A ```` ```suggestion ```` block typed or pasted
  into the prose stays text, as today; the linked edit's block is still the
  one GitHub applies, and the drawer notes that the words hold a block of
  their own.
- **Selecting.** Selecting the draft's card scrolls the pane to the range;
  moving the caret into the range keeps its card selected.
- **The actions are the drawer's own:** Add single comment, Start a review,
  Add review comment, and Command+Return for the main action, through
  today's publication path. Nothing new reaches GitHub.

### A change GitHub cannot take as a suggestion (Q3)

- **Marked.** An unplaceable suggestion keeps its block and its edit. The
  range's wash and edge turn amber, the reading colour, and the card says
  why: "Line 6 changed after commit 9b2e41c. GitHub shows a comment here as
  outdated, so it cannot apply a suggestion." Add single comment and Start a
  review give way to Convert, and their keys send nothing while it is held.
- **Held from the review.** Finish your review lists a held suggestion with
  its reason under its row and Convert beside Delete, and Comment, Approve,
  Request changes and Submit wait until no held suggestion is listed. The
  window holds what it has planned: a suggestion it has opened in the code
  this session. A draft written in an earlier session and never opened since
  is planned, and held, when it is next opened.
- **Convert to file comment**, on the comment itself and on the Finish your
  review page beside the held comment. It rewrites the draft as text
  (`core::convert_suggestion_to_text`): the prose exactly, then "Suggested
  change to lines a–b:" and the change as a ```` ```diff ```` block of its
  removed and added lines. The draft keeps its lines; it is never moved to
  another line. Where a text comment there goes is the existing rule: a
  filtered line GitHub resolves on no commit is BPR-1706's file comment with
  a permalink to the line. A line GitHub resolves on the filtered commit is
  published there as a text comment that GitHub shows outdated, as RFC-BPR-0089
  Q1 decided for comments; making that one a file comment needs a durable
  file-comment subject for a draft, which is [[BPR-1716]]'s AC2 and is
  recorded as follow-up BPR-1732. The button's words name the
  destination the core decides: "Convert to file comment" or "Convert to a
  comment on `<short commit>`". Converted, the draft is an ordinary text
  comment: its whole text shows in the drawer, and it is sent as one.

### Lifecycle

- **Durability.** The draft is written through the existing durable draft
  path from the first settled edit, its body carrying the block. A file
  switch, closing the window or a killed process keeps it as a kept draft.
- **Leaving the edit.** Esc with no selection or extra carets, publishing
  the draft, deleting it, switching file, or Suggest change elsewhere ends
  the edit. With no change and no prose, nothing is kept. Otherwise the
  draft is kept and the pane returns to reading with the draft's lines
  marked as today.
- **Typed back to the original.** With prose, the draft is a plain comment
  on its lines, its body the words alone, and "What it changes" says there is
  no change yet. With no prose it is not kept.
- **Reopening.** Opening a kept draft whose body ends in one suggestion block
  (Suggest change on its lines, its card, Return or Comment) opens the edit
  again, with its words in the drawer; Suggest change puts the keyboard in
  the code and the others leave it with the words. The core rebuilds the span by giving the block's lines the original
  last line's ending (`core::rebuild_suggestion_span`); this is exact for
  every block the edit wrote. It is refused with a reason, and the draft
  stays a text draft, when the block does not parse, the prose holds a
  second block, or the draft's comparison is not the current one.
- **Undo.** The editor's ordinary undo applies (256 steps), and the range's
  extent is part of each step. Delete on the draft deletes it as the drawer
  does today, restores the lines in the pane and ends the edit.

### A new head while editing

The edit is bound to its head blob and comparison. When a newer head arrives,
the edit ends and its draft stays on its comparison, read-only, naming its
head, as the established rule says. Carry and Copy change, below, are card
BPR-1733: they need the host's old-to-new head correspondence on the
window's side, and the core's decision is built (M1). **Carry to the new head** is offered on
that draft only when the core finds every original line of its range
unchanged at the new head (exact, from the old-to-new head correspondence,
never fuzzy, `core::decide_suggestion_carry`). Carrying opens the same edit
on the new head's lines with the same block, and the old draft is deleted
once the new one is kept. Otherwise the draft says the file changed where it
was edited, and Copy change copies the block's text.

### Ownership

- **Core** decides the range at entry, enforces the editable span inside the
  editing model, plans the anchor, block, body and placement, splits and
  composes bodies, rebuilds a span from a kept draft, converts to text, and
  decides Carry. All of these are pure decisions over values the host holds.
- **Host** persists drafts, supplies the head and new-head documents and
  their correspondence, and publishes through today's path.
- **Qt** puts the shared editor over the head pane at the same geometry,
  paints the gutter, fills, range edge and dimmed ribbons, links the card to
  the range and routes the Convert gestures. It decides nothing about
  ranges, anchors or placement.

### Non-goals

- Editing the base side, or lines outside the selected range.
- Several changes from one edit (Q1).
- Applying a suggestion locally or on GitHub from BPR.
- Suggestions on replies, which GitHub anchors to their thread.
- Review Cycles' own editor. It is reused; it gains the editable span, an
  external scroll offset, a number gutter and row fills, and keeps its
  behaviour when none is set.
- A stacked two-pane layout (Q4's note; BPR-1731).

### Alternatives considered

- **The whole head file editable** (draft r1, Q1). Rejected by the owner.
- **Make the diff pane's `QPlainTextEdit` editable.** Rejected: it would fork
  RC55-07's editing behaviour, and the pane's measurement, Find and marks
  assume read-only text.
- **An editor holding only the range, embedded over its rows.** Rejected: the
  rows below cannot move down as the range grows without changing the pane's
  document, which would shift every line-keyed mark, span and Find result.
- **Refuse typing outside the range by moving the caret into it.** Rejected:
  the caret would jump away from where the reviewer is reading.

## Acceptance criteria

- **AC01:** From Shift+Return, the right-click item and the review bar, on a
  selection or the caret alone, the head pane becomes editable with
  unchanged text, scroll, numbers and colours, and the caret at the end of
  the last selected line. A base
  line, a range across changed regions, a single line outside every changed
  region and a range on a filtered diff whose right side is not head are
  refused at entry with their reason.
- **AC02:** Inside the range, RC55-07's gestures (multi-caret, box with
  virtual space, IME, undo, clipboard, Enter indentation) work in the pane on
  the composed application. An edit that would change a fixed byte is
  refused whole with the status line's words; Enter grows the range and the
  rows below move; emptying the span is a deletion.
- **AC03:** For fixture edits that replace, delete, grow and shrink the lines
  and hold backticks, the published comment's `start_line`, `line`, `side`
  and body match the core's plan byte for byte, in the fixture service and on
  BPR-Test. GitHub's "Commit suggestion" then gives exactly the edited file,
  for an LF file and a CRLF file (the line-ending measurement).
- **AC04:** An unplaceable suggestion is marked with its reason on the comment
  and on the Finish your review page, cannot be published as a suggestion,
  and Convert gives the core's text body at the same lines, published where
  the core places a text comment. No suggestion is moved to another line.
- **AC05:** The drawer shows the prose and "What it changes" and never a
  prefilled block. A label chip edit changes only the header's bytes of the
  composed body (`core_feedback_suggestion_preservation` extended to composed
  bodies).
- **AC06:** A kept suggestion survives a file switch, closing the window and a
  killed process. Reopening rebuilds the same span exactly, and an
  unparsable block, a second block or another comparison is refused with its
  reason.
- **AC07:** Undo, Delete, leaving the edit and typing back to the original
  behave as "Lifecycle" says.
- **AC08:** With a new head arriving mid-edit, the edit ends and the draft
  stays on its comparison. Carry is offered exactly when every original line
  of its range is unchanged at the new head, and Copy change otherwise.
- **AC09:** On a filtered diff whose right side is not head, a line unchanged
  at head is placeable, and a changed one is unplaceable with its reason.
- **AC10:** The owner accepts the built in-pane edit on a frozen build, one
  question per look criterion: entry and continuity, the fixed lines'
  refusal, growth and deletion, the drawer link, the unplaceable mark and
  Convert, and one pane.

## Milestones

| Milestone | Card | Scope | Acceptance |
|---|---|---|---|
| M1 Core | BPR-1726 | `decide_suggestion_range`, the editable span in `suggestion_text_editor`, `plan_pull_request_suggestion`, `split_suggestion_body`, `rebuild_suggestion_span`, `convert_suggestion_to_text`, `decide_suggestion_carry`, with core tests | AC03 and AC05 (core share), AC06 (rebuild), AC08 (decision), AC09 |
| M2 Qt pane | BPR-1727 | The shared editor over the head pane at the same geometry: external scroll, number gutter, fills, range edge, dimmed ribbons, the entries and the one-pane head view | AC01, AC02 |
| M3 Drawer and lifecycle | BPR-1728 | The linked draft, the composed body through the durable path, the mark and Convert on the comment and on Finish your review, leaving, reopening, deleting, typed back, the edit ending at a new head, and the composed courses | AC04 to AC07, AC08 (the edit ends), AC09 |
| M3 Carry | BPR-1733 | Carry to the new head and Copy change on a kept suggestion whose head moved, from the host's old-to-new head correspondence | AC08 (Carry and Copy change) |
| M4 Live and attended | BPR-1729 | A BPR-Test course (publish the plan's bodies, then Commit suggestion), the CRLF measurement, a frozen build and the owner's look | AC03 (live), AC10 |
| Closeout | BPR-1730 | Verify every criterion and close the RFC | All |

Follow-ups recorded in the backlog: BPR-1731 (a stacked two-pane
layout, Q4's note) and BPR-1732 (a durable file-comment subject, so a
converted suggestion on a line GitHub resolves on a filtered commit can be a
file comment too).
