---
id: BPR-0191
title: The workbench shell becomes the application's window
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0190, BPR-0195]
---

# The workbench shell becomes the application's window

The largest slice on the board, and the one that makes RFC-BPR-0005 real.

Its preparation was split into BPR-0195 after measuring the size: ~2700 lines
of window implementation and ~4600 lines of tests across three projection-port
contracts. BPR-0195 extracts the session chrome into a composable panel while
preserving behaviour and leaving every existing test untouched, so that this
card changes exactly one thing — which window the application shows.

`github_window` and `workbench_window` both compose `diff_workbench` and
`epoch_comparison_control`. The application shows `github_window`, so the
shell RFC-BPR-0005 specified — toolbar, compact overflow, drawers, detail pane,
declared focus order — has never been on screen. BPR-0188 decided that
`workbench_window` becomes the application's window and recorded why the
cheaper alternative was refused.

## What moves

RFC-BPR-0002's session chrome becomes a composed region inside the shell rather
than a window of its own: host profile selector, owner/repository/pull-request
locator, connect/open/refresh actions, and the status banner. Its threaded
conversation view moves into the conversations drawer region.

RFC-BPR-0002 keeps sole ownership of session identity, credentials, capability,
and conversation content. It stops owning the window. Nothing about what a
conversation *is* changes here.

## The surface must be composed once

Two owners of one review surface is how this started. When this lands,
`diff_workbench` and `epoch_comparison_control` have exactly one composing
parent, and a test says so rather than a reviewer checking.

## Evidence that moves with the code, not evidence that is rewritten

`github_window`'s existing assertions were each forced by a real shipped
defect. BPR-0097 found three live accessible-name collisions — two focusable
widgets answering to `Previous change`, two to `Files drawer`, two to `Review
navigation` — and an ambiguous `Alt+1`/`Alt+2` binding that made Qt trigger
*neither* shortcut, a defect whose only symptom is a key that silently does
nothing.

Every one of those tests applies to the composed window and must pass against
it. In particular the rule that no duplicate key sequence exists anywhere in
the assembled window is now checked over a strictly larger widget set, and it
is the assertion most likely to fail when these two windows merge. That is the
point of moving it rather than rewriting it.

## Measured, 2026-08-08 — the conversation cannot move as written

This card says RFC-BPR-0002's "threaded conversation view moves into the
conversations drawer region". RFC-BPR-0005 agrees: *"Comments remain in the
right drawer."*

**The drawer as built cannot hold it.** `workbench_drawer_panel` is `final` and
composes a `QListView` over `workbench_drawer_model`, which derives from
`QAbstractListModel`. The conversation is a `QTreeView` over
`github_conversation_model`, and it is threaded — that is the whole point of it.
A list model cannot represent a thread.

So the sentence in this card is not a move, it is a design decision that has not
been made. Two routes, and they are not close in cost:

1. **Widen the drawer** to host a tree — `workbench_drawer_model` becomes a tree
   model, or the panel gains a second content mode. Every RFC-BPR-0005 drawer
   assertion is written against a list: bounded row counts, the width
   persistence, the virtualization figures. Each has to be re-established
   against whatever replaces it, and the 68/68/68 bounded-widget figure this
   card already flags for re-measurement is measured on the list.
2. **The conversations drawer hosts the existing tree instead of its list.** The
   drawer keeps its collapse, width and toggle behaviour and swaps its content.
   Smaller, but it means the files drawer and the conversations drawer stop
   being the same kind of thing, and `workbench_drawer_panel` stops being one
   type serving two regions.

Route 2 looks cheaper and is probably right — the two drawers genuinely show
different kinds of content, and pretending otherwise is what produced a list
model for a threaded view. But it is a product-shaped decision about what a
drawer *is*, and it changes assertions RFC-BPR-0005 already banked, so it is
not mine to take silently.

### Decided, 2026-08-08: route 2, with custom drawing

The product owner took route 2 and asked for a threaded conversations drawer
drawn in a git-graph / DAGVIZ style, with filtering by lane.

**The conversation is a forest of trees, not a general DAG**, and that makes
this considerably cheaper than the framing suggests.
`github_conversation_projection` carries `std::optional<std::string> ParentId` —
exactly one optional parent per node. No node has two parents, so there are no
merges, so there are no edge crossings to resolve. Lane assignment in a git
graph is mostly merge topology; without merges it is a depth-first walk with a
lane counter.

A tree *is* a DAG, so the owner's reading is not wrong — it is strictly wider
than what the model can express, and the narrower fact is the useful one. Do not
build merge handling for a shape that cannot have merges.

Roots are the threads: a node with no `ParentId` is a top-level comment or a
thread anchored to a file and line, which `Path` and `Line` carry. Filtering by
lane therefore means filtering by thread, which is a real reviewer gesture
rather than a visualisation gimmick.

A secondary Treeverse-style overview was considered and deliberately deferred —
[[BPR-0211]].

### Refined, 2026-08-08: anchored to the current file, not the whole review

The owner narrowed what the drawer is for:

> A dedicated whole PR conversation view might be nice, but what I typically
> want is a SmartBear Collaborator style conversation anchored to a line of code
> for the currently viewed file

So the drawer's content is **the current file's threads, anchored to lines** —
not the review's entire conversation forest. The whole-review view becomes the
secondary thing, which is roughly [[BPR-0211]]'s territory rather than this
card's.

**This is expressible today with no new plumbing**, which is the useful part:

- `github_conversation_projection` already carries `Path` and an optional
  `Line`, which is the anchor.
- `core::workbench_interaction` already holds a selected
  `std::optional<workbench_file_id> File`, and `select_workbench_file_intent`
  already changes it.
- The two are joined by `core::file_identity_of(path)`, which
  `workbench_snapshot_observation` documents as *the only way to name a file*.
  Path to identity is a pure function, so filtering the forest to the current
  file is a comparison and not a lookup table.

The extraction in `github_conversation_panel` is unaffected — it presents
whatever forest it is handed. What changes is who decides which subset that is,
and that decision belongs above the panel rather than inside it.

### Wired, with one gap that the merge itself closes

`github_conversation_panel::set_anchor_file` is implemented and asserted, and
`github_window` sets the anchor from `diff_workbench`'s current context so the
drawer shows the file being read.

**It follows a conversation projection, not a bare file change.** `diff_workbench`
exposes no sink for a context change — the application publishes into it as a
`diff_projection_port` and nothing observes the result — so a reviewer who opens
a different file without a new conversation projection keeps the previous file's
threads.

Deliberately not fixed by adding a context sink to `diff_workbench`. That would
widen RFC-BPR-0004's surface for a presentation concern belonging to this card,
and it is unnecessary: the shell composes *both* the comparison surface and the
conversation, so once it is the window it can set the anchor when it publishes a
diff. The gap is an artefact of the two windows and closes when they become one.

The panel-level behaviour is already correct and tested — changing the anchor
re-filters from what was retained without waiting for a projection. It is only
the *caller* that is currently under-informed.

### One caveat that lands on [[BPR-0212]]

A comment's `Line` is an anchor *in some revision*. "Anchored to a line of the
currently viewed file" is therefore only stable while the revision is fixed, and
the moment a reviewer changes the epoch pair the anchors refer to code that may
have moved. The product cannot say which revision a comment's line belongs to —
that is exactly the missing datum BPR-0212 records.

So this design works inside one revision today and needs BPR-0212 to work across
them. Worth knowing before the line anchoring is built, not after.

### What is unblocked either way

Both routes need the pull-request metadata and the conversation to be
composable outside `github_window`, exactly as BPR-0195 made the session chrome
composable. That extraction is the enabler and is not affected by the choice.

**Nothing has been extracted yet.** This finding arrived before that work
started, which is the useful order: three hours of moving layout in service of
route 1 would have been three hours spent on the route not taken.

## Required direct evidence

- `nm` on the shipped binary reports `workbench_window` symbols, and the
  application's window is the shell.
- Exactly one widget composes `diff_workbench`, and exactly one composes
  `epoch_comparison_control`, asserted structurally.
- Every accessible name in the assembled window is unique, and every key
  sequence is unique, over the merged widget set.
- The declared focus order names the interactive stops the composed shell
  offers, and the comparison surface still contributes exactly one stop —
  ordering inside it stays RFC-BPR-0004's decision.
- Connect, open, refresh, and the pull-request locator still work, still emit
  intents, and still take their enablement and disabled reasons from command
  projections rather than from local widget state.
- The compact overflow threshold is still derived from style and font only,
  never from the current toolbar mode.
- RFC-BPR-0005's virtualization figures are re-measured on the composed window
  at 60, 6,000, and 60,000 lines. The recorded 68/68/68 figure was measured on
  `qt_workbench_window`; if the merged window's bounded widget count differs,
  the claim is restated with the new number rather than quietly kept.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What must not be done instead

Keeping both windows and showing the shell only in tests. That is the current
state with extra steps.

Relaxing an accessible-name or key-sequence assertion because the merge made it
fail. A collision after the merge is a real collision — the merged window is
what a user gets, and a name or shortcut that is ambiguous there is ambiguous
for them.
