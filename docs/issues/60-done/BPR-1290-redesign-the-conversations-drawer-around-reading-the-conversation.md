---
schema: "implement-rfc.card.v1"
id: "BPR-1290"
title: "Redesign the Conversations drawer around reading the conversation"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1290--BPR-1290--127a0d0a37cd.json","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028","RFC-BPR-0050","RFC-BPR-0052"],"related_cards":["BPR-1083","BPR-1212","BPR-1288","BPR-1293","BPR-1294","BPR-0418","BPR-0456","BPR-0648","BPR-0649"],"owner_references":["docs/design/owner-refs/BPR-1290-owner-ref-conversations-drawer.webp","docs/design/owner-refs/BPR-1290-owner-ref-split-tabs.webp","docs/design/owner-refs/BPR-1290-owner-ref-built-drawer-v002.webp","docs/design/owner-refs/BPR-1290-owner-ref-built-drawer-v003.webp"],"requested_skill":"frontend-design","acceptance":["BPR-1290:inventory","BPR-1290:direction","BPR-1290:reading","BPR-1290:navigation","BPR-1290:depth","BPR-1290:kept","BPR-1290:themes","BPR-1290:owner-look"]}
acceptance: [{"criterion":"BPR-1290:inventory","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"observation","detail":"The inventory table under 'What the drawer does today, and why' was complete before design round v1 and is the basis of :kept."}},{"criterion":"BPR-1290:direction","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"observation","detail":"Owner chose the inline drawer prototype in chat, 2026-10-06: 'I like it' (recorded under 'Owner verdict on the inline drawer prototype')."}},{"criterion":"BPR-1290:reading","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"command","detail":"macOS v005: qt_composed_inline_drawer_scale_{100,125,200} open a thread in place pinned at its line and as tall as its posts, every post with author, BPR-1083 mark and rail, unread state and complete body rendered through markdown_body_view in place with no vertical scroll of its own; Reply opens in place. Linux and Windows passed the v001 rows at 1fecfaef5."}},{"criterion":"BPR-1290:navigation","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"command","detail":"Inline rows cover margin notes, gutter markers, out-of-view chips, Shift-click on scroll-bar ticks (Windows rerun 3/3 at 3036a08af), Only new and Hide resolved, Up/Down, Return, Esc, R, N, Command+Return and Shift+F10, and the line reveal both ways; negative controls fail as expected."}},{"criterion":"BPR-1290:depth","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"command","detail":"Thread map, path caption, post list and body pane are absent from the drawer (negative controls in qt_composed_inline_drawer); a deep thread reads in posting order with 'answers' naming; branching-thread-v1 reads correctly."}},{"criterion":"BPR-1290:kept","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"observation","detail":"Kept rows (unread/Mark read, Resolve, Reply, draft Publish/Edit/Delete, defect badges, Shift+F10 menu, Support details, line sync, gutter marks, Notes, Tour, Discuss) exercised by the 41 Debug rows; governing text amended in the specification's Conversation drawer and thread-map sections and RFC-BPR-0028; posts render per the BPR-1299 correction; DOCUMENTATION_OK."}},{"criterion":"BPR-1290:themes","outcome":"pass","revision":"0a7f72068","evidence":{"kind":"command","detail":"Inline rows render light, dark and high-contrast larger text at 100/125/200 scale, 280/375 px and Discuss widths without clipping, colours from the palette; drawer drags 280-760 px. macOS v005; Linux and Windows at v001."}},{"criterion":"BPR-1290:owner-look","outcome":"pass","revision":"frozen-7fb9997b1-v005","evidence":{"kind":"observation","detail":"Owner Accept, no note: attended session f76a3bf839eafd23b24baa2da0e15979, candidate BPR-1290-built-drawer-v005 (fingerprint 08e46c30552e...), decision 53123a21-bb9d-4c71-906f-e0566a6d58f5, event 5; published as 0a7f72068. Linux/Windows runs of v005's rows are owed station commands on the card, not blockers."}}]
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
  Amended 2026-10-06 by the owner's format choice after round v3 (below): the
  owner chooses one of the whole-window concepts (or rejects all) in chat, and
  the chosen one is then prototyped; the choice and note are recorded here.
  The owner kept the inline drawer prototype on 2026-10-06 ("I like it",
  below).
- **BPR-1290:reading.** In Read, the selected conversation fills the drawer's
  height under the tabs and its navigation (Read is Review since [[BPR-1295]]): every post shows its author,
  avatar, mark and rail per BPR-1083 v4, unread state and complete body, with
  no nested scroll area; Reply opens the editor in place, in the conversation.
  Amended 2026-10-06 by the owner's verdict on the inline drawer prototype
  ("I like it", below): the open thread does not fill the drawer. It opens in
  place, pinned at its line and as tall as its posts, and the drawer moves with
  the code; the rest of the criterion stands.
- **BPR-1290:navigation.** Choosing a thread is one compact control that keeps
  this-file and whole-PR scope, the filters, unread and draft state, keyboard
  access (Up/Down, Return, Shift+F10) and the exact line reveal both ways.
  Amended 2026-10-06 by the owner's verdict on the inline drawer prototype
  ("I like it", below): the drawer shows this file's threads only. A thread
  is chosen by its margin note, its marker in the code's gutter, the
  out-of-view chips at the drawer's top and bottom, or its tick on the code's
  scroll bar. Amended 2026-10-06 by the owner's v001 Reject (below): a thread
  is chosen from the scroll bar by Shift-click on its tick, which scrolls the
  code to its line and opens it; a plain press on the bar stays the bar's own.
  Amended 2026-10-08 by the owner's choice on [[BPR-1335]] (session
  3c9017b5, option 1): the gutter has no marker column; "its marker in the
  code's gutter" is now its tinted line number, pressed anywhere on its cell,
  and the code's context menu offers Open thread for the selected line.
  The filters are Only new and Hide resolved. The keys are
  Up/Down between threads, Return to open, Esc to close, R to reply, N for
  the next new thread, Command+Return (Ctrl+Return) to publish, and
  Shift+F10 for the thread's actions. Opening a thread reveals its line, and
  its marker opens it. Finding a conversation anywhere in the pull request
  moves to [[BPR-1293]].
- **BPR-1290:depth.** The reply-tree graph and the "Path to this reply"
  caption are gone, or folded into a control that names what it does; a reply
  to anything but the post above it says whom it answers; the branching
  fixture (`branching-thread-v1`, RFC-BPR-0028 AC-08) reads correctly.
  Amended 2026-10-06 by the owner's verdict on the inline drawer prototype
  ("I like it", below): the thread map, the path caption, the post list and
  the body pane leave the drawer, and nothing replaces them there. A deep
  thread is read in place: every post in posting order with its complete body,
  and a reply that answers anything but the thread's first post names whom it
  answers. `branching-thread-v1` reads correctly that way. Navigating deep or
  PR-wide structure is [[BPR-1293]]'s.
- **BPR-1290:kept.** Every inventory row still works or is removed by the
  owner's choice, with the governing text amended (RFC-BPR-0028 AC-05 if line
  alignment changes; the specification's thread-map section; RFC-BPR-0005 is
  satisfied while comments stay in the drawer). Object and accessible names
  used by tests and screen readers stay or migrate with their tests. Discuss
  is redrawn in the same language.
  Amended 2026-10-06 by the owner's verdict on the inline drawer prototype
  ("I like it", below). Removed by that choice: the drawer's whole-PR scope
  (to [[BPR-1293]]), the thread map and the path caption (the map's widget is
  kept for BPR-1293 to reuse), the post list and the Markdown body pane (each
  post is read in place, as written), and the "Conversations drawer" caption.
  Comments that are not on a file are [[BPR-1294]]'s. Kept: unread state and
  Mark read, Resolve, Reply, my draft with Publish, Edit and Delete, defect
  badges and the thread menu (Shift+F10), Support details (a line in the open
  thread), the exact line synchronization both ways, the gutter marks, Notes
  and Tour. Discuss keeps its scope, file and filter controls, and lists the
  same notes in file order, each opening in place. The governing text is
  amended in the specification's Conversation drawer and thread-map sections
  and in RFC-BPR-0028's Read and Discuss paragraphs, AC-05 and AC-08.
  RFC-BPR-0005 is satisfied: comments stay in the drawer.
- **BPR-1290:themes.** Light, dark and both high-contrast variants, larger
  text, 375 px, 280 px and Discuss widths render without clipping; colours
  come from the palette. The drawer's width drags from 280 to 760 px (the
  prototype's range).
- **BPR-1290:owner-look.** The owner accepts the built drawer in an attended
  session. Required, not self-certified.

## Boundaries

Presentation in the Qt adapter (`github_conversation_panel`,
`conversation_cards_view`, `github_conversation_delegate`,
`github_thread_map`, `markdown_body_view`, and for the gutter markers and
scroll-bar ticks `diff_workbench`). No new authority: reading,
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

## Design round v2, 2026-10-06 (superseded in part, unanswered)

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

## Round v2 session, stopped unanswered

Round v2's session (`c6d804e30f0b4f6b02c7ef6d13335165`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-drawer-v002`,
cursor 9) was stopped by the coordinator before the owner answered it. It
records no decision on any of its three candidates (`BPR-1290-split-tabs-v002`,
`BPR-1290-tree-style-v002`, `BPR-1290-inline-pick-v002`). Its candidates were
released and its frozen root `/Users/jaeger/bpr1290-drawer-review-v002.OWoDW6`
was deleted (see the v3 session below).

`BPR-1290-tree-style-v002` (A. Indented or B. ↩ chip) is **superseded**, not
answered. The owner's direction below makes the map the way to read a
conversation's structure, so the reader shows one branch at a time and the
question of drawing a whole tree inside the reader no longer arises. The split
and Inline pick questions are asked again in v3 on the same terms.

## Owner direction, 2026-10-06

In chat, verbatim:

> I feel like the treemap should be the primary interface to navigate the
> conversation view., unless you have a good reason it shouldnt be.

"Treemap" means the Treeverse thread map ([[BPR-0418]]), of which the owner
said on 2026-08-21 (the [[BPR-0211]] verdict), verbatim:

> I prefer this view, it still needs to show the full branch that you clicked
> on, or at least up to the noce you selected.

The coordinator replied with one caveat: GitHub review threads are usually
flat (every reply's parent is the first comment), so a map of one thread
degenerates to a root and a row of names. The owner then corrected the scope,
verbatim (relayed by the coordinator):

> Not to move around a single conversation, I mean to find the right
> conversation.

So the map is the way to **find** a conversation across the whole pull
request, not to move inside one. It draws every conversation as a small tree,
grouped by file; picking a conversation or a post in it opens the branch from
the first comment to that post in the reader. Inline remains this file's
line-anchored threads.

## Design round v3, 2026-10-06 (superseded, unanswered)

[Round v3](../../design/BPR-1290-conversations-drawer-v3.html) with
[frames](../../design/BPR-1290-conversations-drawer-frames-v3.html), drawn with
`frontend-design` on the product palette, light and dark, at 375, 280 and
760 px. Inline is v2's, unchanged; its frames are reused from
[v2's frames](../../design/BPR-1290-conversations-drawer-frames-v2.html).

- **The map.** Every conversation in the pull request, grouped by file. A row
  is one conversation: line, the author's avatar and the root's label mark,
  then one node per reply, oldest first; on the right the new count, ✓ for
  resolved, or a lock for my draft. A node is the post's label mark
  (BPR-1083) or the author's initial, ringed blue when new, faded when read.
- **The flat-thread caveat, answered by the layout rule.** Replies sit on one
  line in posting order. A reply that has replies of its own starts a lane
  underneath, joined to the post it answers. A GitHub thread is therefore one
  line and the map branches only where a conversation does. `branching-thread-v1`
  draws as three lanes off the first comment plus the aside's lane off C's
  third reply. A line too long for the drawer folds its oldest read posts into
  a count; the picked post and new ones never fold.
- **Picking.** The picked post has an amber ring in the map and an amber edge in
  the reader (amber follows the accepted Treeverse mockup's "in focus"). The path from
  the first comment is drawn heavy and what follows it light. The reader shows
  that branch, root to picked post and onward, as a linear feed ([[BPR-0456]]),
  with ⑂ n where other branches leave it, and Reply at the foot.
- **Toolbar.** This file or whole pull request; only new; only unresolved;
  highlight a label (fades the rest in place, does not hide it); next new.
- **Layout, three ways.** A. Map above the reader with a divider. B. Map beside
  the reader; the drawer widens to 760 px while Conversation is open (the
  Discuss arrangement), and falls back to A below 640 px. C. The map fills the
  tab; picking folds it to the picked conversation's row above the reader, and
  ‹ (Esc) returns.

Content: the pull request is made up for the frames, 21 conversations in 10
files: `conventional-comments-v1`'s six threads, v2's seven illustrative
threads on `conversation_cards_view.cpp`, and eight illustrative ones in the
same people's voices, including a 17-post flat thread on
`github_thread_map.cpp:214`. The deep thread is `branching-thread-v1`
(constructed; unread state illustrative). Code lines are verbatim from this
repository at `62198a6a8`.

Governing text to amend if accepted: the specification's
[thread map](../../ux/workbench-specification.md#the-thread-map) section (the map
covers the whole pull request, uses the rule above, and shows per-post new
state, which the ledger has carried since [[BPR-0656]]; the section's "no read
or unread state on the map" predates that). RFC-BPR-0028 AC-05's line alignment
stays with Inline.

## Round v3 session, stopped unanswered

Round v3's session (`8ecbfc2039c5ffacfcdd3ea0fff14f2c`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-drawer-v003`)
was stopped by the coordinator at cursor 12 with no decision on any of its four
candidates (`BPR-1290-map-primary-v003`, `BPR-1290-map-layout-v003`,
`BPR-1290-inline-pick-v003`, `BPR-1290-split-tabs-v003`). Round v3 is
**superseded**: none of its questions is answered, and none is asked again.
Its candidates were released (`resume-development`, sequence 13) and its frozen
root `/Users/jaeger/bpr1290-drawer-review-v003.PfgyIp` deleted.

## Owner verdict on round v3 and the format, 2026-10-06

In chat, relayed by the coordinator, verbatim:

> the treeverse review i'm finding it hard to choose any of those they all look
> bad and that review page format isn't ideal for mockup judging on a big
> redesign like this.

Asked how to proceed, the owner chose **fresh concepts first**: step back with
three very different whole-window concepts (not drawer tweaks) as large stills,
then prototype the one they like. No attended session for this round; the
owner answers in chat. The card is parked on `owner-design-discussion`.

Direction carried from earlier rounds: reading the conversation is the useful
part and the drawer has too many parts and is cramped; inline comments
(line-anchored to the file) are split from the conversation tree; the Treeverse
map is the primary way to find the right conversation across the pull request;
the code stays in view while commenting; a long file; minimal text, with icons,
tooltips and colour; label marks and rails as accepted in [[BPR-1083]].

## Whole-window concepts, round 1, 2026-10-06 (superseded, unanswered)

[The concepts page](../../design/BPR-1290-concepts-v1/index.html) shows three
whole-window concepts as large stills, light and dark, at 1440 by 900 and 1920
by 1080, each with two detail stills, a one-line pitch and no form. Drawn with
`frontend-design`; every still comes from
[`frames.html`](../../design/BPR-1290-concepts-v1/frames.html) (`c`, `f`,
`theme`) through headless Chrome at 2x. Shared by all three: amber is the
branch being read (the accepted Treeverse mockup's "in focus"), blue rings mark
new replies, label marks and rails are BPR-1083 v4's with BPR-1080's glyphs and
BPR-1079's tints, and a reply without a label is a dot in its author's colour.

- **A. Atlas.** The map is a navigator column at the left (every conversation,
  grouped by file, one row each, branching where the conversation does); the
  long file stays in the middle with a thread lane at its right edge; the
  picked branch reads in a column at the right with Reply at its foot. Details:
  hovering a post in the map previews it; replying with lines selected in the
  code.
- **B. Desk.** The conversation is the workspace (Discuss): every conversation
  hangs from its file as a vertical tree across the top of the window, the
  branch reads in a wide centre column, and the code peeks in at the right with
  a whole-file minimap. Details: highlighting one label across the pull
  request; quoting lines from the peek into the reply.
- **C. Margin.** The code is the page: threads are notes in a margin beside
  their lines, and the picked one opens right under its line with its own small
  map, the branch and Reply, between the code above and below. The map is a
  finder laid over the window (⌘J) with search by words, person or label and a
  preview. Details: the finder searching for "page"; hovering a margin note.

Content: the long file is this repository's
`src/adapters/qt/src/conversation_cards_view.cpp` (1550 lines, code verbatim at
`1d839518c`). The pull request is made up: 21 conversations in 10 files,
extended from round v3's set. The deep conversation at line 667 has
`branching-thread-v1`'s shape (24 posts, three branches off the first comment,
an aside forking off the third reply of the third branch); its words and unread
state are illustrative.

## Owner direction after concepts round 1, 2026-10-06

The owner did not choose A, B or C. In chat, verbatim:

> what's the point of the conversation view if the inline comments view shows
> the whole thread like the first mockup before I said split it? Seems like
> they show the same thing except for navigation.

> I guess there's comments that are on the PR and not tied to source files to
> consider. I'm not sure how we deal with those at all right now.

> I kind of feel like all of these tree view mockups are worse than what we had
> before the redesign. At least they all confuse me and most encroach on the
> screen real-estate heavily. I'd almost rather just have the tree view be a
> tab in the main pane or a window you can summon like the home screen.

> Honestly I'm not too attached to the modalities right now, i'm not sure how
> long I want to retain them. (beyond overview and review of course)

The concepts round is **superseded**: none of A, B or C is chosen or asked
again. The agreed plan:

- **(a) This card: the drawer is inline only.** It shows this file's
  line-anchored threads, and each opens in place to the whole conversation with
  a reply. There are no tabs for a conversation tree and no map in the drawer.
  Notes and Tour stay.
- **(b) [[BPR-1293]]: the PR-wide finder moves out of the drawer** into a
  main-pane tab or a window you summon like Home (open question), and is not
  tied to the Read or Discuss modalities. Not designed here. The owner then
  narrowed it to navigation only, verbatim:

  > Well I dont necessarily want to summon non-file comments like that. Just
  > the navigation I was thinking. We might need a different better solution
  > for PR wide comments that arent on a file.

- **[[BPR-1294]]: pull request comments and review summaries that are not on a
  file** are a separate open problem. The leading direction is Overview, after
  the owner's "I know in collaborator those comments are effectively on their
  version of the overview page." Not designed here.

Criteria to revisit with the owner once (a) is settled: BPR-1290:navigation
(the whole-PR scope leaves the drawer for BPR-1293), :depth (deep threads are
BPR-1293's question) and :kept (RFC-BPR-0028 AC-05's "All PR conversations"
and Discuss).

## Inline drawer prototype v1, 2026-10-06

[The prototype](../../design/BPR-1290-inline-drawer-v1/index.html) is one
clickable full window: Read on this repository's
`src/adapters/qt/src/conversation_cards_view.cpp` (all 1557 lines, verbatim at
`fbbabb3ef`), the drawer at its 375 px default (drag 280 to 760), light and
dark from the title bar. Drawn with `frontend-design`.

- Eight threads and one draft of mine hang from their lines (38, 430, 656, 667,
  927, 1080, 1142, 1176, 1510) as flat margin notes: author, root mark and rail,
  two lines of the body as written, reply count and people, new count, ✓ for
  resolved. The drawer moves with the code.
- Clicking a thread, or its marker in the code's gutter, opens it in place: the
  whole conversation with a New rule, Show line, Resolve, Mark read, More and
  Reply. The open thread is pinned exactly at its line (the line washed amber,
  its marker ringed, the card's amber edge and notch), and the others make room
  above and below it. One thread is open at a time.
- Threads above or below what the drawer shows are chips on its top and bottom
  edges, nearest first; the code's scroll bar keeps a tick per thread.
- Marks, rails and decorations are BPR-1083's as accepted: three weights and
  the if-minor corner, every body shown as written with its decorations
  (`suggestion (non-blocking, if-minor):`, custom `note (perf):`, a plain post
  with a grey rail). A reply typed with a label prefix gets its mark when
  published.
- Keyboard: Up/Down between threads, Return opens, Esc closes, R replies, N
  next new, ⌘Return publishes.
- Only new and Hide resolved filter the drawer. The finder button and the status
  bar's "3 comments not on a file" name BPR-1293 and BPR-1294; neither is
  drawn.

Content: the threads are made up in the `conventional-comments-v1` people's
voices; the changed-line ranges are illustrative. Kept as today and not drawn:
More's items, defect actions, the composer's inside ([[BPR-1212]]),
Notes and Tour.

## Owner verdict on the inline drawer prototype, 2026-10-06

In chat, relayed by the coordinator, verbatim:

> I like it

The owner keeps the inline drawer as prototyped, which settles
BPR-1290:direction as amended. :reading, :navigation, :depth and :kept are
amended above to the inline-only drawer, citing this verdict. The governing
text follows in the same change: the specification's
[Conversation drawer](../../ux/workbench-specification.md#conversation-drawer)
and [thread map](../../ux/workbench-specification.md#the-thread-map) sections,
and [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
Read and Discuss paragraphs, AC-05 and AC-08.

Next: build the drawer in the Qt adapter, faithful to the prototype, then one
short attended session on a frozen build for BPR-1290:owner-look.

## Built drawer v001, 2026-10-06

Candidate `ffea22995` ("BPR-1290: the conversations drawer is inline only"),
on `origin/main` at `070d612c2`; pushed as `201c96f21` on `fa057ca50`, the
same change over a base that adds only unrelated Git, workspace and export
work (the pin, the drawer rows and `architecture_boundaries` pass again on
it). Built in the Qt adapter, faithful to
[`BPR-1290-inline-drawer-v1`](../../design/BPR-1290-inline-drawer-v1/index.html):

- **Notes.** The Comments tab shows this file's threads as notes beside the
  code (`conversation_cards_view`): avatar, BPR-1083 mark under it, rail,
  author, the first post's text with its decoration text, a count, new and
  draft state. Notes keep to their lines and push down when they would
  overlap; an outdated thread's note follows the placed ones.
- **Open in place** (`conversation_open_card`). A thread opens where its note
  was, pinned at its line and as tall as its posts: every post in posting
  order with its complete body as written, a reply that answers anything but
  the first post naming whom it answers, a New rule before the new posts,
  Show line, Resolve or Unresolve, Mark read, Reply written in place with
  Command+Return (Ctrl+Return) to publish, my draft with Publish, Edit and
  Delete, Support details, and the thread menu (Shift+F10). The code line is
  washed, its gutter marker ringed, and the card has the amber edge.
- **Finding a thread.** Out-of-view chips at the drawer's top and bottom count
  the threads above and below and jump to the nearest; the code's scroll bar
  has a tick per thread (`comparison_scroll_controller`'s axis, so ticks fall
  where the thumb shows the line); the gutter marker opens its thread.
- **Filters and keys.** Only new and Hide resolved, as two icons in the tab
  row. Up/Down, Return, Esc, R, N, Command+Return, Shift+F10.
- **Width.** The drawer opens at 375 and drags from 280 to 760
  (`workbench_drawer`); the tab row wraps rather than clipping at 280 with
  larger text.
- **Removed from the drawer.** The thread map, the path caption, the post list
  and the body pane; `github_conversation_delegate` is deleted. The
  selection authority is still the conversation model's current index, held
  by a tree that is never shown.
- **Kept for [[BPR-1293]].** `github_thread_map` (the Treeverse widget) and its
  rows stay in the tree, unused by the drawer, so BPR-1293's finder can reuse
  it; nothing in this card depends on it.
- **Discuss** lists the same notes in file order with its scope, file and
  filter row; each opens in place. Notes and Tour are unchanged. Nothing here
  depends on the Read/Review split ([[BPR-1295]]).

Boundaries: presentation only. Reading, replying, resolving, marking read and
publishing keep their core and host paths; no new authority, no QSS, colours
from the palette.

Fixture: `inline-drawer-v1` (`scripts/build_inline_drawer_fixture.py`):
conventional-comments-v1's files and six threads plus a 1557-line file with
the prototype's eight threads at its lines. Constructed, nothing fetched.

### Verification on the candidate

- Qt rows: `qt_conversation_cards_view` (rewritten),
  `qt_github_conversation_panel`, `qt_github_thread_map`,
  `qt_feedback_label_threads`, `qt_composed_markdown`, `qt_composed_github`,
  `qt_workbench_window`, and the new `qt_composed_inline_drawer_scale_{100,125,200}`
  (notes at their lines, open in place pinned at its line, wash, ring and
  amber tick, chips, ticks, keys, filters, drag limits, a deep thread in
  posting order, dark and high-contrast larger text; negative controls: the
  drawer holds no thread map, path line, post list or body pane, a note does
  not match the line below its own, a line with no thread has no marker, a
  thread in view has no chip, no filter shows every note again). Passed in
  Debug; the inline rows and the cards view also in Release, where their
  100 ms open and scroll-frame checks are enforced.
- Large-review rows in an optimized build (Release, budgets enforced):
  `application_large_review_*`, `application_first_tab_comparison`,
  `application_unavailable_diff_public_path`,
  `application_approximate_diff_public_path`: passed.
  `paged_review_and_git_load_input_gap` 37.6 ms in Release, 92.6 ms in Debug
  (limit 100; closed notes are measured by advance, not laid out).
  `application_large_review_cache_bytes` missed its footprint budget once
  (198.7 MB against 192.9 MB) and then passed twice; `origin/main` at
  `070d612c2` misses the same budget on its own (195.1 MB), so the row sits
  at its limit independently of this card.
- `application_composition_contract` and
  `application_review_round_trip_composition_contract`: passed after the
  BPR-1290 reseal (presentation and fixture only; mutation pin unchanged).
- `architecture_boundaries`, fixture and documentation rows, format check,
  clang-tidy on the changed Qt files: clean. `DOCUMENTATION_OK`.
- `live_mutation_isolation` timed out once under a full parallel run and
  passed alone (276 s).

### Owner look, session v001

Pending: BPR-1290:owner-look is the owner's, not self-certified. One
attended session, one question, Accept or Reject: "The built drawer matches
the prototype."

- Session `b3a08bc827a529abbf0bc7ecede12823`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-built-v001`,
  candidate `BPR-1290-built-drawer-v001` (revision `frozen-ffea22995-v001`,
  fingerprint `a866088590f1…`), ready at event cursor 2.
- Frozen root `/Users/jaeger/bpr1290-inline-drawer-review-v001.HWZ3wf`: the
  `git archive` of `ffea22995` built there with the dev preset (tests on);
  executable SHA-256 `af5db4f5f51a…`. Offscreen on it, `--attended-open-file`
  showed both `src/ui/conversation_cards_view.cpp` and
  `src/engine/renderer.cpp`, and the inline rows passed from its build tree.
- Launch: `python3 /Users/jaeger/bpr1290-inline-drawer-review-v001.HWZ3wf/launch.py`
  (the long file; `--renderer` for renderer.cpp; `--prototype` opens the kept
  prototype for comparison). It checks the build and fixture tree first and
  reports an early exit within seconds.

Next: read the session's decision. Accept satisfies :owner-look and the card
finishes; Reject comes back with what differs in its note, for a v002
candidate.

**Note: Linux verification at `1fecfaef5` (station `bpr-main-linux`, WSL2, GCC
15.2.0, offscreen).**

- **Dev tree** (rebuilt whole, no warnings): 21 of 21 pass.
  - `qt_composed_inline_drawer_scale_{100,125,200}` (0.9, 1.1 and 1.9 s).
  - `qt_conversation_cards_view` and `qt_github_conversation_panel`.
  - `qt_github_thread_map_scale_{100,125,200}` and
    `qt_feedback_label_threads`.
  - `qt_composed_markdown_{1_0,1_25,2_0}` and
    `qt_composed_github_scale_{100,125,200}`.
  - `qt_workbench_window_scale_{100,125,200}`, which now pass with
    [[BPR-1255]]'s floor.
  - `application_composition_contract` (792.4 s),
    `application_review_round_trip_composition_contract` and
    `architecture_boundaries`.
- **Optimized tests-on tree** (`app-release`, `BUILD_TESTING=ON`, GCC
  `-O3`, `-j1`, idle): 21 of 21 pass in 100.1 s. That is the 18
  large-review rows and the three `qt_composed_inline_drawer` scales. Every
  report has `BudgetsEnforced` true, `KnownInputMisses` empty and no input
  failure. Largest gaps in ms: `large_file_selection` 18.9–34.2,
  `large_source_revisit_render` 17.8 and 21.0, `approximate_file_selection`
  36.4, `first_tab_file` 22.6. The largest gated gap is 49.0
  (`paged_review_and_git_load`).

`build/app-release-tests` was removed. The card stays parked for the owner's
look.

## Owner verdict on built drawer v001, 2026-10-06

Session `b3a08bc827a529abbf0bc7ecede12823` handed back at sequence 4 (cursor
2 to 4 read). `BPR-1290-built-drawer-v001` (revision `frozen-ffea22995-v001`):
**Reject** (decision `1cece685`, 10:31:15Z). Note, verbatim:

> I want to shift click on a comment marker on the scrollbar to scroll to it.

Read as a requirement, not a defect: v001's ticks opened their thread on a
plain press, which also took the press away from the scroll bar. The
coordinator relayed the shape of the fix: Shift-click on a thread's tick
scrolls the code so that its line is in view and opens the thread, as
pressing its note does; a plain click keeps the scroll bar's own behaviour;
crowded ticks are hit-tested to the nearest within a few pixels; a tick has a
"Shift-click to go to …" tooltip and a pointing cursor while Shift is held
over it. BPR-1290:navigation is amended above. Nothing else in v001 was
rejected, and BPR-1290:owner-look is asked again on v002.

## Built drawer v002, 2026-10-06

Candidate `b109295f4` ("Shift-click a scroll-bar tick to go to its thread; a
plain press stays the bar's own"), the change the v001 Reject asked for and
nothing else. Presentation in the Qt adapter only:

- **Shift-click on a tick** (`diff_workbench`'s tick bar,
  `set_thread_tick_dispatch`) goes to its thread: the window's
  `go_to_thread_at` shows the drawer on Comments, opens the thread in place
  and reveals its exact line through Show line's path, so the line comes into
  view even when the thread was already current.
- **A plain press** on the bar is the bar's own (pages or drags). The gutter
  marker still opens or closes its thread.
- **Crowded ticks.** The pointer finds the tick whose centre is nearest,
  within 4 px of its edge.
- **Tooltip and cursor.** A tick's anchored tooltip (BPR-1083's) says
  "Shift-click to go to line 250: 1 post, 1 new". With Shift held over the bar
  the pointer is a hand and the tick a press would choose is ringed in the
  window's ink; pressing or letting go of Shift while the pointer rests there
  updates both. The bar's accessible description names the gesture.

The specification's [Conversation drawer](../../ux/workbench-specification.md#conversation-drawer)
section is amended with the gesture.

### Verification on the candidate

- `qt_composed_inline_drawer_scale_{100,125,200}`, extended through the
  composed window: line 250's thread current but closed and out of view, a
  plain press on its tick moves the code and opens nothing (negative control);
  Shift-click opens it and its line is in view and selected; Shift-click 3 px
  below line 300's tick goes to it; Shift-click on a stretch with no tick
  opens nothing and stays the bar's (negative control); lines 60 and 64's
  ticks are 8 px apart and one point reaches both: Shift-click a pixel nearer
  either goes to that one; without Shift no ring and no hand (negative
  control), Shift pressed and let go with the pointer resting adds and removes
  them; the tooltip's text, the nearer tick's between crowded ones, and none
  away from every tick (negative control). Passed in Debug and Release on
  macOS; Shift-click answers in 5 to 7 ms (limit 100, enforced in Release),
  opening in place 10 to 13 ms, a scroll frame 3 to 9 ms.
- Mutations, each failing the row as expected: a plain press going to the
  thread; no explicit reveal; first tick in reach instead of nearest; no reach;
  no Shift watch while the pointer rests.
- 45 rows (`qt_composed*`, `qt_conversation*`, `qt_github_conversation*`,
  `qt_feedback_label*`, `qt_workbench_window*`, `qt_diff*`,
  `architecture_boundaries` and every `*composition_contract`, including the
  round trip): passed in Debug. `src/app` is unchanged, so the round-trip pin
  is unchanged. `qt_conversation_cards_view` also in Release. Format check and
  clang-tidy on the changed units: clean. `DOCUMENTATION_OK`.

### Owner look, session v002

Pending: BPR-1290:owner-look is the owner's, not self-certified. One question,
Accept or Reject: "The built drawer matches the prototype, with Shift-click on
scroll-bar markers."

- Session `ef04b01ef8a5c04f9d6ff1b0f0dbbe87`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-built-v002`,
  candidate `BPR-1290-built-drawer-v002` (revision `frozen-b109295f4-v002`,
  fingerprint `0190c2ea2162…`), ready at event cursor 2.
- Frozen root `/Users/jaeger/bpr1290-inline-drawer-review-v002.GSNSiu`: the
  `git archive` of `b109295f4` built there with the dev preset (tests on),
  source read-only; executable SHA-256 `676fd7790bd8…`. Offscreen on it,
  `--attended-open-file` showed both `src/ui/conversation_cards_view.cpp` and
  `src/engine/renderer.cpp`, and the inline rows passed from its build tree.
- Launch: `python3 /Users/jaeger/bpr1290-inline-drawer-review-v002.GSNSiu/launch.py`
  (the long file; `--renderer` for renderer.cpp; `--prototype` for the kept
  prototype). It checks the build and fixture tree first and reports an early
  exit within seconds.
- v001's session was released (`resume-development --all`, sequence 5), its
  app instance stopped and its frozen root
  `/Users/jaeger/bpr1290-inline-drawer-review-v001.HWZ3wf` deleted.

Next: read the session's decision. Accept satisfies :owner-look and the card
finishes; Reject comes back with what differs in its note, for a v003
candidate. Linux and Windows have not run v002's rows.

**Note: Windows run at `1fecfaef5` (station `bpr-main-win`, MSVC 14.51, Qt
6.11.2; no remote mutation).**

- **`windows-dev` Debug, whole tree.** It built with no `warning C` or
  `warning LNK` line, and the named selection passes 21 of 21 under `-j4`:
  - `qt_composed_inline_drawer_scale_{100,125,200}`;
  - `qt_conversation_cards_view`, `qt_github_conversation_panel` and
    `qt_github_thread_map_scale_{100,125,200}`;
  - `qt_feedback_label_threads`, `qt_composed_markdown_{1_0,1_25,2_0}` and
    `qt_composed_github_scale_{100,125,200}`;
  - `qt_workbench_window_scale_{100,125,200}` (171.8–187.7 s);
  - `application_composition_contract` (996.8 s),
    `application_review_round_trip_composition_contract` and
    `architecture_boundaries`.
- **Optimized tests-on tree** (`build/bpr0967-release`, MSVC `/O2`). The 18
  large-review rows pass 18 of 18 with `-j1` in 586.1 s. All 14 reports have
  `BudgetsEnforced` true, an empty `KnownInputMisses` and no limit miss. The
  largest gap in a gated course is 62.8 ms (`paged_review_and_git_load`), and
  `files_drawer_open` takes 66.1 ms as a whole phase.

**Note: Windows Shift-click rerun at `3036a08af`** (holds `b109295f4`, which
lets a Shift-click on a scroll-bar tick go to its thread; station
`bpr-main-win`, MSVC 14.51). Both trees were rebuilt with no `warning C`
line. `qt_composed_inline_drawer_scale_{100,125,200}` pass 3 of 3 in each
with `-j1`:

- `windows-dev` Debug: 7.1, 7.9 and 11.2 s;
- the optimized tests-on tree (`build/bpr0967-release`, MSVC `/O2`), where
  the 100 ms open and scroll checks apply: 3.7, 1.3 and 2.2 s.

## Owner verdict on built drawer v002, 2026-10-06

Session `ef04b01ef8a5c04f9d6ff1b0f0dbbe87` handed back at sequence 4 (cursor
2 to 4 read). `BPR-1290-built-drawer-v002` (revision `frozen-b109295f4-v002`):
**Reject** (decision `08cb35c3`, 15:40:10Z). Note, verbatim:

> Tooltip can show up immediately with the shift mouseover.
>
> Something about this screenshot feels broken. I don't think that horizontal
> new line looks good before the the start of the thread. We might need to do
> something special there, because it kind of looks like it belongs to the
> thread above it instead and there's a rendering glitch even though that's
> not true.

The screenshot attached to the verdict,
[`BPR-1290-owner-ref-built-drawer-v002.webp`](../../design/owner-refs/BPR-1290-owner-ref-built-drawer-v002.webp),
shows Read on `inline-drawer-v1`'s long file (fixture data only): the open
thread at line 667 has its header (◇ 667 and the actions), then a "New" rule
with its line running to the right edge, then reviewer-four's first post. The
rule is the first thing under the header, so it reads as the foot of the
card above.

Read as two requirements, relayed by the coordinator: a scroll-bar tick's
tooltip shows at once while Shift is held over it, and other tooltips keep
their delay; and the New rule is never a thread's first element. When the
first shown post is already new, the header says the thread is new and the
posts' unread dots carry the rest; the rule stays only between a read post and
the first new one, clearly inside the card. Nothing else in v002 was rejected,
and BPR-1290:owner-look is asked again on v003.

## Built drawer v003, 2026-10-06

Candidate `033da0859` ("a thread opens with its new count in the header,
never a New rule; Shift shows a tick's tooltip at once"), on `origin/main` at
`b689fd303`: the two changes the v002 Reject asked for and nothing else.
Presentation in the Qt adapter only, drawn with `frontend-design`:

- **New in the header.** An open thread with new posts carries a blue "N new"
  beside its line number (`conversation_open_card`'s `new_pill`, the closed
  note's count kept while the thread is open; accessible name "N new posts").
  Each new post keeps its unread dot.
- **The New rule.** It is never a thread's first element: it stands only where
  a read post is followed by the first new one, in place of the hairline
  between them, from the avatar column to short of the word "New" at its right
  end, so the card's border and amber edge run past it unbroken. A thread whose
  first post is new has the header count and no rule anywhere, also when a read
  post later sits between new ones.
- **Shift tooltip.** With Shift held over a scroll-bar tick, its tooltip shows
  at once, as the ring and the hand do (`diff_workbench`'s tick bar); without
  Shift a tick's tooltip keeps the hover delay. The tooltip is anchored to the
  band across the bar in which the pointer finds that tick, so it stays while a
  press would still choose it. An anchored tooltip no longer goes away on a
  modifier key alone, as `QToolTip`'s does not; another key still dismisses it.

The specification's [Conversation drawer](../../ux/workbench-specification.md#conversation-drawer)
section is amended with both.

### Verification on the candidate

- `qt_conversation_cards_view`: a new test opens one thread in each state.
  All new: "3 new posts" and no rule. First read, then new: "2 new posts", the
  rule between the read post and the first new one, its line from x 12 to 332
  of 344 and the card's left edge pixels the same on the rule's row as beside a
  post. All read, and a single read post: neither. A single new post: "1 new
  post", no rule. Negative control, new, read, new: the count and no rule, and
  no row of the card carries a long line in the New colour.
- `qt_composed_inline_drawer_scale_{100,125,200}` through the composed window:
  line 60's thread, first post new, has "3 new posts" and no rule; read up to
  the reviewer's reply, the rule sits between that reply and the first new
  post with "2 new posts". Shift moving onto a tick shows "Shift-click to go to
  line 250: 1 post, 1 new" at once; moving with Shift to crowded ticks names
  the nearer; pressing Shift while resting shows it; Control leaves it.
  Negative controls: resting without Shift shows nothing before the delay;
  Shift away from every tick shows nothing; a letter key still dismisses it.
- Mutations, each failing its row: a rule before the first post, no header
  count, a rule from the card's edge, a rule before a later new post, no
  immediate tooltip, a modifier dismissing the tooltip.
- Debug, 48 rows (`qt_composed*`, `qt_conversation*`,
  `qt_github_conversation*`, `qt_github_thread_map*`, `qt_feedback_label*`,
  `qt_workbench_window*`, `qt_diff*`, `architecture_boundaries` and every
  `*composition_contract`): passed before the rebase; after it, the drawer
  rows, `qt_conversation_cards_view`, `qt_github_conversation_panel`,
  `qt_feedback_label_{controls,threads}` and `architecture_boundaries` again
  (the base added only benchmark and documentation-gate changes).
- Optimized (`app-release` with `BUILD_TESTING=ON`, Release): the three
  drawer rows and `qt_conversation_cards_view` passed; opening in place 9, 11
  and 14 ms, Shift-click 5 to 7 ms (limit 100, enforced).
- Format check and clang-tidy on the changed units: clean.
  `DOCUMENTATION_OK`. Linux and Windows have not run v003's rows.

### Owner look, session v003

Pending: BPR-1290:owner-look is the owner's, not self-certified. One
question, Accept or Reject: "Drawer v003: the new-post marking and the Shift
tooltip are right."

- Session `ad892b9ca0cff2ad4b6faccc02fdc093`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-built-v003`,
  candidate `BPR-1290-built-drawer-v003` (revision `frozen-033da0859-v003`,
  fingerprint `14f2486d66da…`), ready at event cursor 3. It carries the frozen
  build's own 2x render of both states, because every fixture thread starts
  with someone else's post and the live build only shows "first post new".
- Frozen root `/Users/jaeger/bpr1290-inline-drawer-review-v003.fAs2jw`: the
  `git archive` of `033da0859` built there with the dev preset (tests on),
  source read-only; executable SHA-256 `0db09c74a9a6…`. Offscreen on it,
  `--attended-open-file` showed both `src/ui/conversation_cards_view.cpp` and
  `src/engine/renderer.cpp`, and the three inline rows passed from its build
  tree.
- Launch: `python3 /Users/jaeger/bpr1290-inline-drawer-review-v003.fAs2jw/launch.py`
  (v002's launcher: `--renderer` for renderer.cpp, `--prototype` for the kept
  prototype; it checks the build and fixture tree first and reports an early
  exit within seconds).
- v002's session was released (`resume-development --all`, sequence 5) and
  its frozen root `/Users/jaeger/bpr1290-inline-drawer-review-v002.GSNSiu`
  deleted.

Next: read the session's decision. Accept satisfies :owner-look and the card
finishes; Reject comes back with what differs in its note, for a v004
candidate.

## Owner verdict on built drawer v003, 2026-10-06

Session `ad892b9ca0cff2ad4b6faccc02fdc093` handed back at sequence 5 (cursor
3 to 5 read). `BPR-1290-built-drawer-v003` (revision `frozen-033da0859-v003`):
**Reject** (decision `567768f9`, 16:27:00Z). In chat: "handed back, much
better". Note, verbatim:

> : chortcodes arent working in the reply comments.
>
> I want add comment from the right-click menu. I want add comment to show up in the drawer instead of the full screen add comment window. Fold add comment window into discuss. Drawer comment should have something like a maximize button to go view it in discuss.
>
> I want to try to unify a bunch of this interaction with the local project flow. Maybe we do right click to add comment / suggest change with enter / shift+enter as shortcuts?

The screenshot attached to the verdict,
[`BPR-1290-owner-ref-built-drawer-v003.webp`](../../design/owner-refs/BPR-1290-owner-ref-built-drawer-v003.webp),
shows Read on `inline-drawer-v1`'s long file (fixture data only): line 1080's
thread open in place with "2 new" in its header, and the in-place Reply
editor holding ":p" with no completion under it and the chip saying "Plain".

Read as follows, relayed by the coordinator:

- **This card: shortcodes in the reply.** The in-place Reply editor carries
  its own chip and no label helper, so a typed `:` never completes and a
  `:xx:` code never becomes its label. It gets [[BPR-1083]]'s helper, the same
  one the composer has: the `:` completion, the shortcode replacement, the
  chip and Command+L. v003's attended launch also never turned the helper on
  (it is off in the product until [[BPR-1088]]'s Settings row), so v004's does.
- **To [[BPR-1212]]: the composer items.** Add comment from the right-click
  menu; Add comment in the drawer instead of the full-screen composition page;
  the composition page folded into Discuss; a maximize control on a drawer
  comment that views it in Discuss; and one interaction with the local project
  flow, with right-click to add a comment or suggest a change and Enter and
  Shift+Enter as shortcuts. Another implementer takes these under BPR-1212;
  this card does not.

Nothing else in v003 was rejected; the new-post marking and the Shift tooltip
stand. BPR-1290:owner-look is asked again on v004, about the shortcodes only.

## Built drawer v004, 2026-10-06

The change the v003 Reject asked of this card and nothing else. Presentation
in the Qt adapter only, drawn with `frontend-design`:

- **One helper.** The in-place reply's text is a `QPlainTextEdit`, as the
  composer's body is, and carries `feedback_label_controls`, the composer's own
  ([[BPR-1083]]). The reply's private chip is gone. A lone `:` at the start
  opens the label completion, letters narrow it, the closing colon of `:qu:`
  makes the reply `question: `, a code before a label replaces it, Command+L
  opens the label menu, and the chip names the label with its mark. A pasted
  code is not typing and stays as written.
- **One switch.** The reply follows the draft editor's label helper: the
  draft's reply presentation carries it (`thread_reply_presentation`'s
  `LabelHelper`), and turning the editor's helper on or off refreshes an open
  reply at once. It is off in the product until [[BPR-1088]]; the attended
  launch turns it on with `--label-helper`.
- **Fitted to the drawer.** The blocking, non-blocking and if-minor toggles
  stay in the chip's menu under "Adds" (`set_decoration_toggles(false)`), since
  the drawer is 280 px at its narrowest; the chip's mark still shows them. The
  chip is drawn as the drawer's quiet pill: a hairline in the rule colour on a
  tint of the panel, the signal colour when it has focus. A refused label edit
  is said under the reply's buttons.
- **Esc.** With the completion open, Esc closes it and the reply keeps its text
  and focus; otherwise Esc behaves as before.
- [[BPR-1084]]'s decision mark has not landed (the card is In Progress), so
  there is none to carry.

The specification's [Conversation drawer](../../ux/workbench-specification.md#conversation-drawer)
section says the reply has the composer's label controls.

### Verification on the candidate

- `qt_conversation_cards_view`, a new test on the reply in place: with the
  helper off (negative control) a lone `:` opens nothing and `:qu:` stays as
  typed; on, `:` opens the completion with all twelve labels, Esc closes it
  and keeps the text and focus, `:q` narrows it to question first, `:qu:`
  becomes `question: ` and the chip says "Label: question", the replaced text
  goes to the draft, `:is:` before the label makes it `issue: why`, Command+L
  opens the menu and choosing suggestion makes `suggestion: why`; a pasted
  `:qu: pasted` stays (negative control). The decoration toggles are hidden.
- `qt_composed_inline_drawer_scale_{100,125,200}` through the composed
  window: R opens the draft editor's reply in place with controls of its own
  text; with the composer's helper off (as shipped) `:` opens nothing
  (negative control); turning the composer's helper on turns the open
  reply's on; `:` opens the twelve-label completion while the composer's own
  stays closed (negative control); `:qu:` becomes `question: ` in place and in
  the draft, replaced once, by the reply's controls. Renders at each scale:
  `composed-inline-drawer-<scale>-inline-reply-{completion,label}.png`.
- Mutations, each failing its row: the reply ignoring the draft's switch; the
  editor's switch not refreshing the open reply; Esc leaving the reply while
  the completion is open.
- Debug: `qt_composed_inline_drawer_scale_{100,125,200}`,
  `qt_conversation_cards_view`, `qt_github_conversation_panel`,
  `qt_github_conversation_model_property`, `qt_review_draft_editor`,
  `qt_feedback_label_{marks,controls,threads}` and `architecture_boundaries`
  passed. Format check and clang-tidy (`tidy.py --changed`): clean.
  `src/app` is unchanged. Linux and Windows have not run v004's rows.


### Owner look, session v004

Pending: BPR-1290:owner-look is the owner's, not self-certified. One
question, Accept or Reject: "drawer v004: shortcodes work in replies."

- Session `2c58f7ad375731fd84651160badee24e`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-built-v004`,
  candidate `BPR-1290-built-drawer-v004` (revision `frozen-69ff2d5e3-v004`,
  fingerprint `0149d0b24c45…`), ready at event cursor 4. It carries the frozen
  build's own 2x render of line 250's reply after `:qu:`.
- Frozen root `/Users/jaeger/bpr1290-inline-drawer-review-v004.3ABLRN`: the
  `git archive` of `69ff2d5e3` built there with the dev preset (tests on),
  source read-only (rebased onto `origin/main` as `69bdc0fce`, which differs
  from it only in BPR-1212's design documents); executable SHA-256 `aed8dec988cf59c1…`. Offscreen with the
  launcher's arguments it printed `ATTENDED_FILE_SHOWN`, and
  `qt_composed_inline_drawer_scale_{100,125,200}` and
  `qt_conversation_cards_view` passed from its build tree.
- Launch: `python3 /Users/jaeger/bpr1290-inline-drawer-review-v004.3ABLRN/launch.py`
  (v003's launcher with `--label-helper` added; `--renderer` for
  renderer.cpp). It checks the build and fixture tree first and reports an
  early exit within seconds.
- v003's session was released (`resume-development --all`, sequence 6), its
  app instance stopped and its frozen root
  `/Users/jaeger/bpr1290-inline-drawer-review-v003.fAs2jw` deleted.

Next: read the session's decision. Accept satisfies :owner-look and the card
finishes; Reject comes back with what differs in its note, for a v005
candidate. The composer items of the v003 note are BPR-1212's.

**Note (2026-10-06, from [[BPR-1299]]):** reading posts "as written" makes
RFC-BPR-0036's AC-01 fail for public conversations. The native captures of
`qt_composed_markdown` show raw `- [x]`, fences and table pipes in the open
thread. This card's change to that course also dropped its conversation
structure assertions, so the rows still pass. BPR-1299 is parked on the
owner's choice between rendering posts with the shared reader in this card's
next candidate (recommended) and amending AC-01 to the overview.

## Session v004 stopped unanswered; posts render again, 2026-10-06

Session `2c58f7ad375731fd84651160badee24e` (v004, shortcodes in replies) was
not answered. The coordinator stopped its host, and it was released here
(`resume-development --all`, sequence 5, no decision). Its frozen root
`/Users/jaeger/bpr1290-inline-drawer-review-v004.3ABLRN` was deleted. The
v004 question is asked again in v005.

[[BPR-1299]] found a regression from `201c96f21`: posts in an open thread
showed their Markdown as written (`- [x]`, fences, table pipes, `**bold**`).
That fails RFC-BPR-0036's AC-01 for public conversations. The same commit had
removed the composed course's conversation checks, so the rows still passed.
Coordinator's decision, 2026-10-06: render posts, since it is a regression
against an accepted RFC. The `:kept` amendment's "each post is read in place,
as written" is read with that correction: in place, rendered, with its label
header as written.

## Built drawer v005, 2026-10-06

Candidate `7fb9997b1` ("open-thread posts render Markdown through the shared
reader"), on `origin/main` at `440768bdc`. It holds v004's reply helper and
adds the rendering. Presentation in the Qt adapter, drawn with
`frontend-design`:

- **Rendered posts.** Each post of the open thread, in the drawer and in
  Discuss, reads through `markdown_body_view` in a new in-place mode
  (`set_in_place`). The post is as tall as its content and has no vertical
  scrolling of its own, so a vertical wheel still scrolls the code. Wide code
  and tables scroll sideways. Headings keep to the post's scale, lists indent
  by half Qt's step for the 280 px drawer, and links on a dark panel keep their
  hue at a readable lightness. Closed notes stay one-line summaries.
- **Label header as accepted.** [[BPR-1083]]'s mark under the avatar and the
  rail are unchanged. The recognized header and its decorations stay at the
  front as written, rendered (`**praise:**` reads "praise:"), in semibold and
  the secondary ink. A header written strong stays bold.
- **Source.** View source (angle brackets) and Copy source (two sheets) sit at
  the right of the post's name row. They show while the post is hovered or
  holds focus. View source stays lit while the source is shown. Copy says
  "Source copied" under the button.
- **Links.** A link opens through the window's admission with the post's
  identity, as the overview's do. Its destination is its tooltip, shown at
  once when Tab reaches the link.
- **Keyboard.** Tab from the notes enters the open thread's first post. Tab
  walks that post's links, then its two source buttons, then the next post.
  In a post's text the arrows move the reading caret, not the thread. The
  drawer keeps the caret in view, scrolling the code in Review as a wheel
  would. Esc, R and N keep their meaning there.
- **Fix on the way.** Setting the code font or the colours left the old open
  card behind as a hidden widget. It is now replaced and deleted.

The specification's [Conversation drawer](../../ux/workbench-specification.md#conversation-drawer)
section and [`native-markdown.md`](../../architecture/native-markdown.md)
describe the rendered posts. The fixture `inline-drawer-v1` gains a Markdown
reply at line 430 (a task list, a code block, a table and bold); 45 posts.

### Verification on the candidate

- `qt_composed_markdown_{1_0,1_25,2_0}`: the conversation checks that
  `201c96f21` removed are back, against the drawer, for all six public posts.
  They cover `structures()` (heading, tasks, lists, code, table, accessible
  text) and no Markdown punctuation as prose. They check `original_body()`,
  Tab from the notes into each post's reader, and reading to the end by
  keyboard with the final line inside the drawer. They also check link
  identity, View source and Copy source per post, the source font from the
  appearance profile, and selection kept across an unchanged refresh.
  **Failing-before control:** with these tests on the pre-fix product (`src/`
  at `440768bdc`), all three fail at "every public body and reply reads in
  place through the shared Markdown reader". The native captures
  `conversation-1`/`-2` show rendered tasks, code, a bordered table and links.
- `qt_composed_inline_drawer_scale_{100,125,200}` through the composed window,
  line 200's Markdown reply:
  - The reader is used and the body as written is kept. Tasks, code fence,
    table and bold are rendered, and no punctuation shows as prose.
  - Negative control: a plain post reads as its words, with no header.
  - Tab from the notes reaches the first post, then the Markdown post. Down
    there moves the caret and keeps the thread. Tab reaches the link and shows
    its destination; Return hands it to admission with `review-comment:c17`.
  - View source shows the body as written at its own height; Copy source copies
    it; switching back restores the height. A wheel over the post's text
    scrolls the code.
  - Line 250's `chore (non-blocking):` header is semibold and the words after
    it are not.
  - Render: `composed-inline-drawer-<scale>-inline-markdown-post.png`.
- Mutations on the fixed product, each failing its row: no arrow-key exemption
  in a post's text (the inline rows); no caret reveal (`qt_composed_markdown`).
- Debug, 41 rows: `qt_composed*`, `qt_conversation*`,
  `qt_github_conversation*`, `qt_github_thread_map*`, `qt_feedback_label*`,
  `qt_workbench_window*`, `qt_markdown_body_*`,
  `qt_pull_request_overview_page`, `qt_public_preview_conversation`,
  `qt_review_draft_editor`, `architecture_boundaries`,
  `architecture_boundary_contract`, `core_markdown_navigation`,
  `core_dependency_boundary`, `application_github_window_read_host`,
  `application_setup_destination_public_path` and
  `application_device_authorization_browser_address`. All passed, as did
  `github_recorded_fixture_contract` and `github_contract_fixture_security`.
  `src/app` is unchanged, so the round-trip pin is unchanged.
- Optimized (`app-release` with `BUILD_TESTING=ON`, Release), serial: the
  three drawer rows, `qt_composed_markdown_*` and `qt_conversation_cards_view`
  passed, with the 100 ms checks enforced. Opening in place took 7, 8 and
  10 ms. Opening the Markdown thread took 6, 7 and 8 ms. A scroll frame took
  3 to 5 ms, and Shift-click 3 to 5 ms. In Debug the same steps took
  7 to 10, 6 to 9 and 3 to 6 ms.
- Format check and clang-tidy (`tidy.py --changed`): clean.
  `DOCUMENTATION_OK`. Linux and Windows have not run v005's rows.

### Owner look, session v005

Pending: BPR-1290:owner-look is the owner's, not self-certified. One
question, Accept or Reject: "drawer v005: shortcodes work in replies and
posts render Markdown."

- Session `f76a3bf839eafd23b24baa2da0e15979`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1290-built-v005`,
  candidate `BPR-1290-built-drawer-v005` (revision `frozen-7fb9997b1-v005`,
  fingerprint `08e46c30552e…`), ready at event cursor 4. It carries the
  frozen build's own 2x render of an open thread with the Markdown reply. The
  host was started detached (`nohup`, its own session) and answered on
  loopback.
- Frozen root `/Users/jaeger/bpr1290-inline-drawer-review-v005.sSilse`: the
  `git archive` of `7fb9997b1` built there with the dev preset (tests on),
  source read-only. It was published as `0a7f72068` after a rebase onto
  `63915222e`, which adds only BPR-1084/1085 and BPR-1190 work. The executable
  SHA-256 begins `69b08eb11ab3e24f`. Offscreen with the launcher's arguments it
  printed `ATTENDED_FILE_SHOWN`, and the three inline rows and
  `qt_composed_markdown_*` passed from its build tree.
- Launch: `python3 /Users/jaeger/bpr1290-inline-drawer-review-v005.sSilse/launch.py`
  (`--renderer` for renderer.cpp). It refuses if the build, the fixture or the
  line-430 Markdown reply is missing, and reports an early exit within
  seconds. The label helper is on (`--label-helper`).

Next: read the session's decision. Accept satisfies :owner-look and the card
finishes. Reject comes back with what differs in its note, for a v006
candidate.

## Owner verdict on built drawer v005, 2026-10-06: Accept

Session `f76a3bf839eafd23b24baa2da0e15979`, read with `status --after 4`:
event 5 records **Accept** on `BPR-1290-built-drawer-v005` (revision
`frozen-7fb9997b1-v005`, fingerprint `08e46c30552e…`, decision
`53123a21-bb9d-4c71-906f-e0566a6d58f5`, saved 2026-10-07T02:13:23Z, actor
human) with no note; event 6 hands back with nothing pending. That satisfies
BPR-1290:owner-look on the candidate published as `0a7f72068`. The session was
released (`resume-development --all`) and its frozen root
`/Users/jaeger/bpr1290-inline-drawer-review-v005.sSilse` deleted. The
attachment folders `/Users/jaeger/bpr1290-owner-attachments-v002` and `-v003`
were deleted; their screenshots are committed as
`docs/design/owner-refs/BPR-1290-owner-ref-built-drawer-v002.webp` and `-v003.webp`.

Later `origin/main` commits touch `conversation_cards_view.cpp` only for
[[BPR-1086]]'s tour-stop header (a published tour stop's label is read inside
its narrative); the accepted drawer is unchanged by them.

### Platform evidence

- **macOS** (v005, `7fb9997b1` = `0a7f72068`): the 41 Debug rows, the
  optimized drawer and Markdown rows with the 100 ms checks enforced, and the
  frozen build's own offscreen run and inline rows, all listed under
  [Verification on the candidate](#verification-on-the-candidate-4). The
  owner's look was on macOS.
- **Linux** (`bpr-main-linux`, WSL2, GCC 15.2.0): v001 at `1fecfaef5`, 21 of 21
  in the dev tree and 21 of 21 in the optimized tests-on tree (above).
- **Windows** (`bpr-main-win`, MSVC 14.51, Qt 6.11.2): v001 at `1fecfaef5`,
  21 of 21 in `windows-dev` and the 18 large-review rows optimized; the
  Shift-click rerun at `3036a08af`, 3 of 3 in both trees (above).

### Owed station runs (not blocking)

The acceptance criteria name no per-platform run, and `platforms` is
eligibility metadata, so this card finishes on the macOS evidence and the
owner's Accept. Linux and Windows have not run v002 to v005's rows. Owed, at
`origin/main` holding `0a7f72068`, with no remote mutation:

- `bpr-main-linux`: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure -R '^(qt_composed_inline_drawer_scale_.*|qt_composed_markdown_.*|qt_conversation_cards_view|qt_github_conversation_panel|qt_github_thread_map_scale_.*|qt_feedback_label_threads|qt_markdown_body_.*|qt_public_preview_conversation|qt_review_draft_editor|architecture_boundaries)$'`
- `bpr-main-win`: `cmake --preset windows-dev && cmake --build --preset windows-dev && ctest --preset windows-dev -j4 --output-on-failure -R` with the same expression.

A failure there opens a new card against this drawer; it does not reopen this one.


## Windows station run (bpr-main-win, 2026-10-06)

At `fe64f5541`, a clean tree, on Windows 11 with MSVC:

- **Tree.** `build/windows-dev-tour-cli`, the `windows-dev` preset with
  `-DBPR_BUILD_TOUR_CLI=ON`, Debug.
- **Build.** Rebuilt with no `warning C` line.
- **Runner.** The network-isolated runner, `ctest -j4`. The Qt rows ran on
  the offscreen platform, so nothing touched the desktop.

One pass ran BPR-1290's expression and BPR-1084/1085's together. All 32 rows
pass, 32 of 32: 27 contained and 5 under their existing `WINDOWS_UNCONTAINED`
pins.

This card's 19 rows, all passing:

- `qt_composed_inline_drawer_scale_100`, `_125` and `_200` (10.7, 12.8 and 17.6 s)
- `qt_composed_markdown_1_0`, `_1_25` and `_2_0`
- `qt_markdown_body_1_0`, `_1_25` and `_2_0`
- `qt_github_thread_map_scale_100`, `_125` and `_200`
- `qt_conversation_cards_view`
- `qt_github_conversation_panel`
- `qt_feedback_label_threads`
- `qt_public_preview_conversation`
- `qt_review_draft_editor`
- `architecture_boundaries`

The Windows half of the owed runs is discharged. No new card is opened.
