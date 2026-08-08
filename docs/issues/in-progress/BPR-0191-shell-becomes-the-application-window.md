---
id: BPR-0191
title: The workbench shell becomes the application's window
status: In Progress
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

This finding arrived before that work started, which is the useful order: three
hours of moving layout in service of route 1 would have been three hours spent
on the route not taken.

### Extracted, 2026-08-08: both composable regions exist

`github_conversation_panel` and `github_pull_request_panel` now own what
`github_window` used to hold inline. `github_window.cpp` went from 932 lines to
736 and composes three panels — session, pull request, conversation.

Object names, accessible names, and every reviewer-facing string moved
unchanged, so the window tests that reach these widgets through `findChild` were
not edited at all. That is deliberate and it is the only evidence available that
the move preserved behaviour: a test rewritten alongside the code it covers
cannot distinguish a faithful extraction from a plausible one.

Shown non-vacuous rather than assumed: neutering
`github_pull_request_panel`'s disclosure toggle — with a clean build, zero
`error:` lines — fails `qt_github_window_scale_100` on *"technical disclosure
opens in place without moving focus"*. The untouched window assertions do reach
into the panel.

This slice also repairs the previous one: `github_conversation_panel.cpp` was
committed with a `readability-identifier-naming` failure — a lambda named
`rooted_in_file` where the project spells locals in camel case — so the
`coding-tidy` gate was red from that commit until this one. It was missed
because the gate's output had been captured through `tail`, which both discards
the diagnostic and replaces the build's exit status with the pipe's. Capture
long gates whole.

Two members stay on the window as borrowed pointers, `TechnicalDetailsButton`
and `BodyEditor`, because the tab order and the description splitter are
properties of the assembled window rather than of either panel. The description
editor is built by the panel, since it is pull-request content, and placed by
whoever assembles the window.

### Route 2 built, 2026-08-08: the conversations drawer hosts the tree

`workbench_drawer_panel::set_content_widget` swaps what a drawer shows while it
keeps everything that makes it a drawer — collapse, width reporting, the toggle,
the heading. The shell composes `github_conversation_panel` into the
conversations drawer through it. The files drawer is untouched: a file list is a
list.

The list and its model are **destroyed rather than hidden**. A hidden list would
still answer `contents()`, and a caller reading rows off it would believe it was
reading the drawer's content while the drawer showed something else. Both
`contents()` and `contents_view()` now answer `nullptr` for that drawer, which
is the honest report.

Because of that, the drawer stopped being able to answer "what is your one
interactive stop" through `contents_view()`. `focus_stop()` answers it instead —
the list for a list drawer, the tree for a hosted one — and `focus_order()` asks
that rather than reaching past the drawer for a view that may not exist. Without
it the composed window would have contributed a null focus stop, which is the
silent-failure shape BPR-0097 was about.

One existing assertion was **restated, not relaxed**: "each drawer owns its own
independent contents" could not survive a drawer with no list. It now reads that
a hosting drawer reports no list at all, plus a separate check that the files
drawer still has one. Both new assertions were shown to bite with a clean build:
neutering `focus_stop()` fails *"the drawer's one interactive stop is the
conversation tree"*, and hiding the list instead of destroying it fails *"a
drawer that hosts a tree reports no list…"*.

### Measured, 2026-08-08: the last two slices cannot be separated

The obvious next step looked like "compose the remaining two panels, then wire
the port". It does not decompose that way. `github_session_panel`'s constructor
takes the profile list, the intent dispatch, and the connect/open/refresh
`QAction`s — so the shell cannot construct it before it is a
`github_projection_port` with a dispatch of its own. `github_pull_request_panel`
needs nothing at construction, but composing it alone would put an inert
metadata region on screen with no source of data.

So the remainder is one coherent chunk, not three:

1. `workbench_window` implements `application::github_projection_port`, gaining
   the intent dispatch, the three actions, and the profile list, and composes
   `github_session_panel` and `github_pull_request_panel` into a header above
   the central splitter. The panel builds the description editor but does not lay
   it out — `github_window` reparents it into a splitter — so the shell must
   place `body_editor()` explicitly rather than rely on Qt's reparent-removal
   behaviour.
2. The application constructs the shell instead of `github_window`, and
   `github_window` is deleted.
3. Virtualization re-measured at 60/6,000/60,000 lines on the composed window,
   restating 68/68/68 if the merged bounded widget count differs.

The uniqueness evidence this card calls for needs no new test: the shell's
existing assertions already walk `findChildren<QWidget*>()` for duplicate
accessible names among reachable focusable widgets, and `findChildren<QAction*>()`
for duplicate key sequences. Composing the GitHub panels puts them under those
rules automatically, which is why the conversation panel landing changed nothing
there and why the session chrome is the one to watch.

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
