---
schema: "implement-rfc.card.v1"
id: "BPR-1290"
title: "Redesign the Conversations drawer around reading the conversation"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"origin":"Owner chat, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1290--BPR-1290--127a0d0a37cd.json","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028","RFC-BPR-0050","RFC-BPR-0052"],"related_cards":["BPR-1083","BPR-1212","BPR-1288","BPR-0418","BPR-0456","BPR-0648","BPR-0649"],"owner_references":["docs/design/owner-refs/BPR-1290-owner-ref-conversations-drawer.webp","docs/design/owner-refs/BPR-1290-owner-ref-split-tabs.webp"],"requested_skill":"frontend-design","acceptance":["BPR-1290:inventory","BPR-1290:direction","BPR-1290:reading","BPR-1290:navigation","BPR-1290:depth","BPR-1290:kept","BPR-1290:themes","BPR-1290:owner-look"]}
---

# BPR-1290: Redesign the Conversations drawer around reading the conversation

## Origin

The owner, in chat on 2026-10-06, verbatim:

> I don't understand the conversations drawer at all.. the useful part is this
> tiny little section at the bottom i can barely see and the rest I dont get.

Their screenshot, [`BPR-1290-owner-ref-conversations-drawer.webp`](../../design/owner-refs/BPR-1290-owner-ref-conversations-drawer.webp),
shows Read on the `conventional-comments-v1` fixture (`src/engine/renderer.cpp`,
fixture data only). The Comments tab of the drawer stacks four parts:

1. Thread cards: "Inline thread · @tbutler / 4 posts · 3 unread / Reply Mark
   read", a dashed "Your unpublished draft" card with Publish, Edit and Delete,
   and "Inline thread · @jiayin / 2 posts · 1 unread".
2. "4 comments · depth 1" over a graph of name pills (`renderer.cpp:6`, then
   `@tbutler`, then `@fixture-author`, `@jiayin`, `@tbutler`).
3. "Path to this reply: renderer.cpp:6 › @tbutler · 3 replies below it".
4. At the bottom, a few lines tall: "Review thread / src/engine/renderer.cpp:6
   · Open / Reply by @tbutler". This is the only part the owner finds useful.

## Outcome

The selected conversation is the drawer's main surface, at full height: every
post inline with its avatar, its label mark under the avatar and its rail
(BPR-1083's accepted v4 layout), complete bodies, and a reply written in
place. Choosing a thread becomes a compact control (a list or a switcher). The
reply-tree graph and the path caption are removed, or folded into something
that explains itself. Text is kept to a minimum, with icons and tooltips, as
the owner prefers.

Use the `frontend-design` skill. Design as static frames first, ask the owner
to choose between named candidates, then build the chosen one in the Qt
adapter.

## What the drawer does today, and why

Nothing here may be lost by accident. Every row is either kept by the
candidates or listed as a decision for the owner.

| Part | What it does | Why it exists |
|---|---|---|
| Tabs: Comments (count), Notes, Tour | Comments carries the count of threads with new comments; Notes holds private notes and AI tools; each tab keeps its own scroll | [Workbench specification, Conversation drawer](../../ux/workbench-specification.md#conversation-drawer); RFC-BPR-0052's right tray; [[BPR-1204]]; RFC-BPR-0039 (Tour) |
| "Conversations drawer" caption above the tabs | The panel's name (`workbench_drawer.cpp` `panel_name`) drawn as a title | The specification says the drawer's header **is** its tabs; the candidates drop the caption and keep the name as the accessible name |
| Scope: This file / All PR conversations | Cards for the compared file at their lines, or every thread in file order | [[BPR-0648]]; RFC-BPR-0028 AC-05 |
| Filters: Unread, Unresolved, Clear; Discuss's file choice | Narrow the threads shown | [[BPR-0649]]; specification's filter list (New, Unresolved, Defects, Needs Verification, Pending Drafts, Outdated, All) |
| Thread cards aligned to their lines | Each card sits at its line's height beside the code, pushed down when lines are close; unplaceable threads follow | [[BPR-0648]]; RFC-BPR-0028 AC-05 ("Read shows this file's threads aligned to their lines with unread state and drafts"); the owner's SmartBear Collaborator reference ([[BPR-0211]]) |
| Card actions: Reply, Resolve/Unresolve, Mark read | Reply makes the thread current and focuses the draft editor; Resolve through the mutation port; Mark read through the conversation receipt | [[BPR-0657]], [[BPR-0656]] |
| Unread dots, counts and dimming; "Next new comment"; gutter marks (amber thread, blue new) | Show and walk what is new | [[BPR-0656]], [[BPR-0648]] |
| Draft cards (dashed): Publish, Edit, Delete | My unpublished note at its line; nothing publishes without Publish | [[BPR-0648]]; RFC-BPR-0037; Compose is [[BPR-1212]] |
| Defect badge, Reclassify, Previous/Next defect, Defects N/M | Private thread classification on its card | [[BPR-1208]]; RFC-BPR-0012 |
| Label marks and rails on posts | 16 px mark under the 18 px avatar, rail in the label's tint, 4 px when blocking | [[BPR-1083]], owner's v4 Accept ([`BPR-1079-feedback-ux-selected.html`](../../design/BPR-1079-feedback-ux-selected.html)); [delivery contract](../../ux/conventional-comments-feedback.md#thread-cards-and-detail-bpr-1083-rfc-bpr-0050) |
| Thread map ("4 comments · depth 1" and the pill graph) | One thread's reply structure, one row per depth; selecting a node highlights its whole branch | [[BPR-0418]]: the owner chose the Treeverse-style map on 2026-08-21 ("it still needs to show the full branch that you clicked on"); [specification, The thread map](../../ux/workbench-specification.md#the-thread-map); RFC-BPR-0005 |
| "Path to this reply: … · n replies below it" | Caption naming the branch from the root to the selected post | [[BPR-0418]]'s "show the full branch" floor, as text |
| Post list ("Review thread / … · Open / Reply by @tbutler"), capped at 7 lines | The selected thread's posts as a flat feed; a reply still says "Reply by" | [[BPR-0456]]: the owner asked for "a straight linear reply feed … we have the treverse view if things get complicated" |
| Markdown body under the post list | The selected post's complete body; View source and Copy source keep the label prefix | [[BPR-0456]]; RFC-BPR-0050 R32 |
| Support details | Explains a file-level compatibility comment | RFC-BPR-0022 |
| Line synchronization | Selecting a thread reveals its exact line; selecting a line filters to its threads; the first automatic selection moves no code | [Specification](../../ux/workbench-specification.md) "Conversation synchronization is exact"; [[BPR-0985]] |
| Discuss arrangement | The same widgets, widened: list and map at the left, feed at the right, at least 640 px | [[BPR-0649]]; RFC-BPR-0028 AC-05 and AC-08 |

Width: the drawer opens at 375 px (`conversation_drawer_default_width`) and can
be dragged to 160 px ([[BPR-0728]], [[BPR-0662]]); Discuss grants at least 640.

### Why it reads badly

Read from the code and the screenshot, not from the owner:

- The selected conversation, the part people read, has the least room: the
  post list is capped at seven lines (`setMaximumHeight(lineSpacing * 7)`) and
  the body sits under it, below the cards and the map on one vertical
  splitter.
- The map was accepted as the answer to deep threads, but GitHub's own reply
  edges point at the thread's first comment, so most real threads are "depth 1"
  and the map draws a root with a row of names. In the fixture it explains
  nothing the posts don't.
- The cards repeat what the post list shows, and the space above the first card
  is the line alignment, which reads as empty space.

## Relation to other cards

- **[[BPR-1083]]** (Parked, human review). Its v4 thread marks under the
  avatar and the label rail are accepted for thread cards; this redesign draws
  every post that way and does not change the marks, tints, glyphs or the
  blocking cue. BPR-1083's open round v002 (`BPR-1083-thread-marks-v002`)
  judges the marks as built today, and that verdict carries over.
- **[[BPR-1212]]** (Backlog). Owns the Compose rethink. The candidates here
  place a reply field in the conversation; its inside (label chip, draft
  keeping, Publish) stays whatever BPR-1212 settles, and this card adopts it.
  Whichever lands second adopts the other's composer.
- **[[BPR-1288]]** (Backlog). The same "frontend-design love" request for
  Settings; the two share the palette and type scale, and neither depends on
  the other.

## Acceptance

- **BPR-1290:inventory.** The table above is complete before design starts.
- **BPR-1290:direction.** The owner chooses one of the frame candidates (or
  rejects all) in an attended session; the choice and note are recorded here.
- **BPR-1290:reading.** In Read, the selected conversation fills the drawer's
  height under the tabs and its navigation: every post shows its author,
  avatar, mark and rail per BPR-1083 v4, unread state and complete body, with
  no nested scroll area; Reply opens the editor in place, in the conversation.
- **BPR-1290:navigation.** Choosing a thread is one compact control that keeps
  this-file and whole-PR scope, the filters, unread and draft state, keyboard
  access (Up/Down, Return, Shift+F10) and the exact line reveal both ways.
- **BPR-1290:depth.** The reply-tree graph and the "Path to this reply"
  caption are gone, or folded into a control that names what it does; a reply
  to anything but the post above it says whom it answers; the branching
  fixture (`branching-thread-v1`, RFC-BPR-0028 AC-08) reads correctly.
- **BPR-1290:kept.** Every inventory row still works or is removed by the
  owner's choice, with the governing text amended (RFC-BPR-0028 AC-05 if line
  alignment changes; the specification's thread-map section; RFC-BPR-0005 is
  satisfied while comments stay in the drawer). Object and accessible names
  used by tests and screen readers stay or migrate with their tests. Discuss
  is redrawn in the same language.
- **BPR-1290:themes.** Light, dark and both high-contrast variants, larger
  text, 375 px, 280 px and Discuss widths render without clipping; colours
  come from the palette.
- **BPR-1290:owner-look.** The owner accepts the built drawer in an attended
  session. Required, not self-certified.

## Boundaries

Presentation in the Qt adapter (`github_conversation_panel`,
`conversation_cards_view`, `github_conversation_delegate`,
`github_thread_map`, `markdown_body_view`). No new authority: reading,
replying, resolving, marking read and publishing keep their existing core and
host paths. The frames are design evidence, not the product.

## Design round v1, 2026-10-06 (superseded, unanswered)

[Round v1](../../design/BPR-1290-conversations-drawer-v1.html) with
[frames](../../design/BPR-1290-conversations-drawer-frames-v1.html), drawn
with `frontend-design` on the product palette. The drawer is shown at 375 px
and 280 px, in light and dark, with the fixture's renderer.cpp threads, the
whole pull request and `branching-thread-v1`. All three candidates share the
conversation: one header row (location, show in code, Resolve, Mark read,
More), the posts per BPR-1083 v4 with complete bodies, a "New" rule, a
"↩ name" chip on a reply that answers something other than the post above it,
a ⑂ count that folds branches, and a Reply field at the foot. They differ in
how a thread is chosen:

- **A. Switcher.** A row of chips (root mark, line, new count; drafts dashed)
  and a list popover for the whole pull request.
- **B. Stack.** One-line rows for the other threads above and below the open
  one, which expands in place.
- **C. Line rail.** A 24 px strip whose ticks sit at their lines' height, so
  it keeps AC-05's alignment, with previous and next arrows and the list.

The draft's body in the frames is illustrative (the screenshot does not show
it). Choosing A or B means amending RFC-BPR-0028 AC-05's "aligned to their
lines".

## Round v1 session, unanswered

Round v1's session (`77c7a0ba376d5e801cc4975de994ba70`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-drawer-v001`,
candidate `BPR-1290-drawer-direction-v001`) was not answered: the owner
stopped it. It records no decision. Its candidate was released
(`resume-development`, sequence 4) and its frozen root
`/Users/jaeger/bpr1290-drawer-review-v001.Hi6CDf` deleted. A, B and C are not
chosen or rejected; round v2 replaces the question.

## Owner notes after round v1, 2026-10-06

In BPR-1083's v002 review (session `c2a2442c5ce16c3fbb77598fc87360bb`,
sequence 25, the verdict on `BPR-1083-thread-marks-v002`), before answering
v1, verbatim as recorded:

> I also think I've identified why comments drawer feels so clunky, that one
> tab is trying to serve 2 masters. I think we need to split the inline
> comments and the comment tree into separate ttabs in the drawer.

The screenshot attached to that verdict, [`BPR-1290-owner-ref-split-tabs.webp`](../../design/owner-refs/BPR-1290-owner-ref-split-tabs.webp),
shows Read on `conventional-comments-v1` (`renderer.cpp:12`, fixture data
only): the Comments tab with two thread cards, the map ("2 comments · depth
1"), the path line and the post list. The verdict's first line ("Marks don't
show on inline comments") is BPR-1083's to act on.

On round v1, relayed by the coordinator, verbatim:

> It's hard to tell the value of the inline comments without a long file, you
> have no long files in your mockups

## Design round v2, 2026-10-06

[Round v2](../../design/BPR-1290-conversations-drawer-v2.html) with
[frames](../../design/BPR-1290-conversations-drawer-frames-v2.html), drawn with
`frontend-design` on the product palette, light and dark, at 375 px and
280 px. The Comments tab becomes two tabs; Notes and Tour are unchanged.

- **Inline.** This file's threads as cards beside their lines: the first post
  in full with its mark and rail (BPR-1083 v4), the people, the reply count,
  what is new. A card opens in place to the whole conversation and a Reply
  field. Threads above or below the visible code are chips on the tab's top
  and bottom edges (nearest first); clicking one scrolls the code there. With
  no thread on the visible lines, the nearest one each way peeks in as a
  closed card. The code's scroll bar keeps a tick per thread. Drawn against a
  long file at three scroll positions.
- **Conversation.** One conversation in full under a header that picks the
  thread (the whole pull request, by file), steps to the previous or next,
  shows the line, resolves and marks read; Reply at the foot. It follows the
  thread chosen in Inline.

Two choices are drawn both ways:

- **Tree.** A. Indented: a reply sits under the post it answers, a run where
  each answers the one above stays level, and a line with a fold knob joins a
  post to its several replies. B. ↩ chip: posts in order one step in; a reply
  that answers anything but the post above starts with "↩ name: first words".
  GitHub review threads, where every reply answers the first comment, look the
  same both ways.
- **Picking a card in Inline.** A. It opens in place (↗ opens it in
  Conversation). B. It opens the Conversation tab (⌄ opens it in place).

Content: the long file is this repository's
`src/adapters/qt/src/conversation_cards_view.cpp` (1518 lines, code verbatim
at `7513758b0`); no fixture has threads on a long file, so its seven threads
are made up, in the voices of the `conventional-comments-v1` people. The short
threads and the pull-request list are that fixture's; the deep thread is
`branching-thread-v1`, whose unread state is illustrative. Thread map, path
line, post list and body pane are gone; defect actions, Support details and
View/Copy source move under More. Discuss is redrawn after the choice.

## Pending owner decisions

One attended session asks three questions on frames frozen from `552615a22`
in `/Users/jaeger/bpr1290-drawer-review-v002.OWoDW6` (the two v2 HTML files and
both owner screenshots; `open /Users/jaeger/bpr1290-drawer-review-v002.OWoDW6/source/docs/design/BPR-1290-conversations-drawer-v2.html`).
Session `c6d804e30f0b4f6b02c7ef6d13335165`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-drawer-v002`,
cursor 9:

1. `BPR-1290-split-tabs-v002`: Accept or Reject the split into Inline and
   Conversation as drawn (BPR-1290:direction).
2. `BPR-1290-tree-style-v002`: A. Indented or B. ↩ chip (BPR-1290:depth).
3. `BPR-1290-inline-pick-v002`: A. Opens in place or B. Opens Conversation
   (BPR-1290:navigation).

Next: record each verdict and note here, revise the frames if a note asks,
then plan the Qt build. BPR-1083's sessions are separate and untouched.
