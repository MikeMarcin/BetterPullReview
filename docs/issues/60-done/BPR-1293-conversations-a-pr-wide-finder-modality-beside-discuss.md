---
schema: "implement-rfc.card.v1"
id: "BPR-1293"
title: "Conversations: a PR-wide finder modality beside Discuss"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06, during BPR-1290","reservation":"docs/issues/_reservations/BPR-1293--BPR-1293--370e927e88e9.json","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028","RFC-BPR-0037","RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1295","BPR-1294","BPR-1089","BPR-1021","BPR-1135","BPR-0418","BPR-0211","BPR-0649","BPR-0926","BPR-0929"],"implementation_note":"Build faithful to docs/design/BPR-1290-inline-drawer-v2 with the frontend-design skill; owner review is required","owner_decision":{"date":"2026-10-06","prototype":"I like that a lot.","placement":"What if we were to replace discuss with conversations.","answers_date":"2026-10-06","q1":"Hide drawers","q2":"just keep discuss for now even after 1294"},"open_questions":[],"acceptance":["BPR-1293:owner-decision","BPR-1293:owner-questions","BPR-1293:modality","BPR-1293:finder","BPR-1293:pick","BPR-1293:keys","BPR-1293:entries","BPR-1293:discuss-kept","BPR-1293:governing-text","BPR-1293:tests","BPR-1293:owner-look"],"sequence_after":"BPR-1290 and BPR-1295 (all three change the modality strip, workbench_window.cpp and the window tests; this card's code waits for both)"}
acceptance: [{"criterion":"BPR-1293:owner-decision","outcome":"pass","revision":"3754fee9b","evidence":{"kind":"observation","detail":"The owner's words on prototype v2 ('I like that a lot.') and the placement ('What if we were to replace discuss with conversations.') are recorded verbatim in the card's Owner decision, 2026-10-06."}},{"criterion":"BPR-1293:owner-questions","outcome":"pass","revision":"3754fee9b","evidence":{"kind":"observation","detail":"Q1 'Hide drawers' and Q2 'just keep discuss for now even after 1294' are recorded verbatim in Owner answers, 2026-10-06, before the build; the build hides drawers in Conversations and keeps Discuss."}},{"criterion":"BPR-1293:modality","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"workbench_modality::conversations in the core and Qt enums with its own segment, tooltip, accessible and object names; the strip is Overview Cmd1, Review Cmd2, Discuss Cmd3, Conversations Cmd4 per the owner amendment; the drawer toggle is the Comments drawer. core_workbench_interaction and qt_workbench_window_scale_* pass on macOS arm64 Debug; the Ctrl+3 Discuss / Ctrl+4 Conversations key scans find each sequence once."}},{"criterion":"BPR-1293:finder","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"qt_conversation_finder_scale_{100,125,200}: the row rule (flat, the prototype's deep shape, a leaf at a fork, hostile parents, a 20,000-post chain), conversations on a file only, search over words/person/file/line, Only new, Hide resolved, one label, +n folding never folding new posts or the picked one, keys, and renders in light, dark and both high-contrast themes at larger text and 640 px with nothing clipped. Failing-before controls: every reply its own row; new posts folding."}},{"criterion":"BPR-1293:pick","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"Window test test_conversations_finds_and_picks_into_review through the real core: Review, then the file, Comments opened if closed, the thread opened in place at the picked post washed amber once the file's comparison shows; reopening starts on that conversation with its path traced (also seen offscreen in the real application on conversations-v1). Controls: the file selected before Review; the thread opened before its file shows (large review)."}},{"criterion":"BPR-1293:keys","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"Cmd+J and the drawer's finder button enter Conversations from Overview, Review and Discuss with the search focused; Cmd+J or Esc on an empty search returns to the origin modality; drawers hidden with states unchanged and Cmd+B disabled (core refusal conversations_finds_a_conversation). Key scans: Ctrl+3, Ctrl+4 and Ctrl+J each once. Controls: Cmd+B offered in Conversations; drawers shown in Conversations; Conversations on Ctrl+3."}},{"criterion":"BPR-1293:entries","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"#discussion_r links, the published-post reveal and defect navigation (a thread on a file) open the thread in place in Review; #pullrequestreview-, Show them, a Discuss card's Open in code and the preflight rows land where they did. The window course test_conversations_finds_and_picks_into_review checks the entries by identity; the setup-destination, restoration, review-submission and attended-recent-replay public paths pass unchanged at 7778886c9."}},{"criterion":"BPR-1293:discuss-kept","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"Discuss's page, arrangement, preset, file scope, filters, summary, map, feed, Open in code, tooltip and names are unchanged; only its key is Cmd3 (owner amendment). qt_github_conversation_panel, qt_conversation_cards_view, qt_composed_github_scale_* and the Discuss public paths pass with only the key and modality loops changed."}},{"criterion":"BPR-1293:governing-text","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"The workbench specification (Modalities, shortcuts, defects, thread map), RFC-BPR-0028's Conversations section and RFC-BPR-0052's tab paragraph and AC-06 with BPR-1135's AC-06 row word for word describe the build; python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK."}},{"criterion":"BPR-1293:tests","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","detail":"57 of 57 focused rows on macOS arm64 Debug, merged tree, including core_workbench_interaction, qt_conversation_finder_scale_*, qt_workbench_window_scale_*, qt_composed_github_scale_* (selector trap), qt_composed_inline_drawer_scale_*, architecture_boundaries and application_large_review_responsiveness; optimized tests-on tree 32 of 32 with budgets enforced (conversations_enter 9.9 ms, pick 18.4 ms, pick to thread open 110.5 ms). Linux and Windows station runs owed, not blocking."}},{"criterion":"BPR-1293:owner-look","outcome":"pass","revision":"7778886c9","evidence":{"kind":"observation","detail":"Owner accepted BPR-1293-conversations-v001 ('Conversations (Cmd4) matches the prototype', the one question the coordinator set) with no note: decision 941b04a3, event 5, session 2588378b1ac64914f3302695cdb348bf, on the frozen build of 7778886c9 (executable SHA-256 83f003ddfe0b8e9e...)."}}]
---

# BPR-1293: Conversations: a PR-wide finder modality beside Discuss

## Origin

While redesigning the Conversations drawer ([[BPR-1290]]), the owner said in
chat on 2026-10-06, verbatim:

> what's the point of the conversation view if the inline comments view shows
> the whole thread like the first mockup before I said split it? Seems like
> they show the same thing except for navigation.

> I kind of feel like all of these tree view mockups are worse than what we had
> before the redesign. At least they all confuse me and most encroach on the
> screen real-estate heavily. I'd almost rather just have the tree view be a
> tab in the main pane or a window you can summon like the home screen.

> Honestly I'm not too attached to the modalities right now, i'm not sure how
> long I want to retain them. (beyond overview and review of course)

The agreed plan split the drawer's two jobs. The drawer becomes inline only
(BPR-1290). Finding the right conversation across the pull request moves out
of the drawer into a place of its own: this card.

The coordinator first filed this card as "A PR-wide conversations view you can
summon", holding the finder, pull request comments, review summaries and deep
threads. The owner then narrowed it, verbatim:

> Well I dont necessarily want to summon non-file comments like that. Just the
> navigation I was thinking. We might need a different better solution for PR
> wide comments that arent on a file.

## Outcome

Amended 2026-10-06 by the owner's answers to Q1 and Q2
([below](#owner-answers-2026-10-06)): Conversations is added and Discuss is
kept, even after [[BPR-1294]].

**Conversations** is a new modality beside **Discuss**. The modalities are
Overview ⌘1, Review ⌘2, Conversations ⌘3 and Discuss ⌘4 (Read merges into
Review under [[BPR-1295]], which puts Discuss on ⌘3; this card moves it to
⌘4). Conversations puts the finder from
[prototype v2](../../design/BPR-1290-inline-drawer-v2/index.html) in the main
pane: every conversation in the pull request that is on a file, grouped by
file, for **navigation only**. The drawers are hidden while it shows. Picking
one switches to Review on its file, with the thread open in the inline drawer
([[BPR-1290]]) at the picked post, as the prototype's Tab placement does. ⌘J
(Ctrl+J) goes straight to Conversations.

Discuss keeps its behaviour. Pull request comments and review summaries that
are not on a file are not in Conversations; Discuss still reads them, and
[[BPR-1294]] may give them a better home without removing Discuss.

### As decided before the answers (superseded 2026-10-06)

Conversations replaced Discuss as the third modality: Overview ⌘1, Review ⌘2
and Conversations ⌘3, with Discuss removed and pull request comments left
without a reader until BPR-1294.

### As first filed (superseded 2026-10-06)

A view for **navigation only**: the Treeverse finder ([[BPR-0418]], the
owner's [[BPR-0211]] choice) across every conversation in the pull request,
grouped by file, used to jump to the right conversation. Picking one opens its
file in Read at its line with the thread open in the drawer (BPR-1290). It is
not tied to the Read or Discuss modalities.

It does not hold or read pull request comments or review summaries that are not
on a file. Those are [[BPR-1294]]'s open problem.

## Owner amendment, 2026-10-06: Discuss on ⌘3, Conversations on ⌘4

The owner, in chat on 2026-10-06, relayed by the coordinator, verbatim:

> can we swap discuss to 3 and conversations to 4

This supersedes every key assignment below that puts Conversations on ⌘3 and
Discuss on ⌘4, including the acceptance criteria's. **Conversations is ⌘4
(Ctrl+4); Discuss stays on ⌘3**, where [[BPR-1295]] put it, so its key does
not move. ⌘J still goes to Conversations. The strip's order is unchanged by
the keys: where this card names the segment's place, read it with the keys
swapped (Conversations' tooltip names ⌘4 and ⌘J). It also supersedes the
coordinator's earlier note on [[BPR-1295]] that Discuss keeps ⌘4.

## Owner direction, 2026-10-06

In chat, about the inline drawer prototype
([`BPR-1290-inline-drawer-v1`](../../design/BPR-1290-inline-drawer-v1/index.html)),
verbatim as relayed by the coordinator:

> I like it

> Of course it didnt show the tree nav

The coordinator took this card up from there: extend the same prototype so its
finder button and a hotkey open the Treeverse finder over the same pull request,
and let the owner try both placements (a main-pane tab beside the file, and a
view summoned over the window like Home) in the prototype itself. The placement
stays the owner's open question; the prototype answers nothing on its own.
Feedback comes in chat, with no attended session.

## Finder prototype v2, 2026-10-06

[The prototype](../../design/BPR-1290-inline-drawer-v2/index.html) is BPR-1290's
inline drawer prototype ([v1](../../design/BPR-1290-inline-drawer-v1/index.html),
unchanged) extended across the whole pull request. Drawn with `frontend-design`,
on the product palette, light and dark.

- **Opening it.** The finder button at the top of the drawer, or ⌘J (Ctrl+J,
  unbound in the product today). ⌘J again, or Esc, closes it.
- **Both placements, switched in the status bar.** A. A tab in the main pane
  beside the file; the drawer stays, the tab stays open, and picking switches
  to the file's tab. B. Summoned over the whole window like Home (`Ctrl+0`);
  picking closes it.
- **A row per conversation**, grouped by file: its line, the first post's label
  mark, its first words, then one small avatar per post in posting order, and
  on the right its new count, ✓ for resolved, or a lock for my draft. Blue dot
  is new, amber is where you are (the accepted Treeverse "in focus"), resolved
  rows are faded. A row folds its oldest read posts into "+n" past nine; new
  posts and the picked one never fold.
- **Branching only where it exists.** A reply nobody answered stays on its
  parent's row, so a GitHub thread is always one row. Where a post has two or
  more replies that have replies of their own, each starts an indented row
  under it, joined by an outline line. `branching-thread-v1`'s shape (line 667)
  draws as the first comment, three branches under it, and the third branch's
  continuation and aside under that.
- **Picking navigates.** A row picks its first post; an avatar picks that post
  (hovering one previews it). The file opens, the thread opens in the drawer
  pinned at its line, and the picked post is washed amber and scrolled into
  view; a long thread keeps its header at the top of the drawer. Reopening the
  finder starts on that conversation, with the path from the first comment to
  the picked post traced in amber.
- **The drawer reads a deep thread whole**, each branch in turn, and a reply
  that answers anything but the post above it carries a "↩ name" chip that
  jumps to the post it answers.
- **Search and filters.** Words, a person, a file or a line number; the row
  shows the matched post's words, and matched avatars are ringed. Only new,
  hide resolved, and one label (from the labels the pull request uses), which
  rings that label's posts and fades the rest.
- **Keyboard.** Type to search; Up and Down between rows; Left and Right along
  a row's posts; Return goes there; Esc clears the search, then closes. Left
  and Right on a file's row fold and unfold it.

Content: the pull request is made up, 22 conversations in 10 files, in the
`conventional-comments-v1` people's voices, each placed on a real line it talks
about. The code is verbatim in
[`code.js`](../../design/BPR-1290-inline-drawer-v2/code.js):
`conversation_cards_view.cpp` at `fbbabb3ef` as in v1, the other nine files at
`25f700e09`. The deep thread's words and new state are illustrative.

## Owner decision, 2026-10-06

In chat, relayed by the coordinator, verbatim. On prototype v2:

> I like that a lot.

Asked "Which finder placement should we build? Like Home / Tab / Both", the
owner answered:

> What if we were to replace discuss with conversations.

What follows from them:

- **The drawing is kept.** The finder is built as prototype v2 draws it: rows,
  grouping, branching, marks, avatars, states, search, filters and keys
  ([Finder prototype v2](#finder-prototype-v2-2026-10-06) above).
- **The placement is decided.** It is neither a tab nor a summoned window but a
  modality, **Conversations**, in the main pane. The open question "main-pane
  tab or summoned window" is closed. (As first read, it replaced Discuss;
  amended 2026-10-06 by the owner's Q2 answer: Discuss stays beside it.)
- **Picking works as the Tab placement did.** It switches to Review on the
  picked conversation's file, with the thread open in the inline drawer pinned
  at its line and the picked post washed amber and scrolled into view.
- **⌘J stays** as the shortcut that goes straight to Conversations.

The points this card left open are settled as follows:

- *The finder's drawing.* Prototype v2's, kept by the owner.
- *Deep threads* (`branching-thread-v1`, RFC-BPR-0028 AC-08). Conversations
  shows a conversation's branches as indented rows only where they exist, and
  the drawer reads the thread whole (BPR-1290). The thread map is not used.
- *What happens to Discuss.* First read as replaced (inventory below);
  amended 2026-10-06 by the owner's Q2 answer: Discuss stays, unchanged, on
  ⌘4.

## Owner answers, 2026-10-06

The owner answered the two questions below in chat, relayed by the
coordinator, verbatim:

- **Q1** (drawers while Conversations shows): "Hide drawers".
- **Q2** (the gap for pull request wide comments if Discuss is removed):
  "just keep discuss for now even after 1294".

What follows from them:

- **Q1.** The drawers are hidden while Conversations shows and nothing
  changes their states; Review comes back with the arrangement it last had
  (BPR-1295). ⌘B is disabled in Conversations.
- **Q2.** Conversations is **added** and Discuss is **kept**, even after
  [[BPR-1294]]. The modalities are Overview ⌘1, Review ⌘2, Conversations ⌘3 and
  Discuss ⌘4, set explicitly. ⌘J still goes to Conversations. Discuss's own
  behaviour does not change: its page, list, map, feed, filters, preset,
  tooltip and names stay as they are; only its key moves from ⌘3 (BPR-1295) to
  ⌘4. There is no gap for comments that are not on a file: Discuss still reads
  them.
- **Entry points.** Each keeps its current destination unless it is better
  as Review in place ([Entry points, as amended](#entry-points-as-amended)).
- **Removal is out of scope.** Nothing of Discuss is removed or renamed, so
  the criterion that removed it is withdrawn and BPR-1293:discuss-kept takes
  its place.

## What Conversations does

Proposed from the owner's decision, answers and the prototype; shown at
BPR-1293:owner-look. Amended 2026-10-06 for the answers.

- **The modality.** The strip's third segment, "Conversations", on ⌘3, between
  Review and Discuss. The four keys are set explicitly, as BPR-1295 sets ⌘1 to
  ⌘3; Discuss moves to ⌘4. Its tooltip: "Find a conversation in the pull
  request (⌘3, ⌘J)". The strip's control description becomes "Overview,
  Review, Conversations and Discuss"; Discuss's segment, tooltip and object
  names are unchanged.
- **The page.** The finder fills the centre, as Discuss's page does today: the
  search field with ⌘J printed in it, the Only new chip with its count, Hide
  resolved, one label, and the list grouped by file. The review bar stays, as
  it does in every modality. Its file and change arrows move the comparison
  retained underneath, as they do under Discuss today ([[BPR-0929]]), and do
  not move the finder's cursor.
- **The drawers (Q1, answered "Hide drawers").** While in Conversations the
  drawers are not shown, and nothing changes their states. Review comes back with the
  arrangement it last had (BPR-1295), hidden drawers included. Discuss's preset
  (Files collapsed, Conversations expanded) is not carried over. ⌘B is
  disabled in Conversations, as BPR-1295 disables it in Discuss. Overview's
  refusal text does not name Conversations, which shows no drawer.
- **Entering.** ⌘3, the strip segment, ⌘J from Overview, Review or Discuss,
  and the finder button at the top of the Comments drawer (the prototype's, tooltip "Find a conversation in the
  pull request ⌘J"). The search field has focus. The cursor starts on the
  conversation open in the drawer, with the path from the first comment to the
  picked post traced in amber, or on the first row when none is open.
- **Leaving without picking.** ⌘J again, or Esc on an empty search, returns to
  the modality Conversations was entered from, as it was. The strip's other
  segments work as usual.
- **Picking.** A row picks its first post and an avatar picks that post.
  Return picks the cursor's. The window enters Review in its remembered
  arrangement (BPR-1295) and selects the file through the same intent the
  Files drawer and Overview's file rows send. It then opens the Comments
  drawer if it is closed (a manual open, as a gutter mark's press is under
  BPR-1295) and opens the thread in place, pinned at its line, with the picked
  post washed amber and scrolled into view. A thread that cannot be placed in
  the shown comparison opens among the file's unplaceable threads, as BPR-1290
  draws them.
- **Remembered.** Each window keeps the search, the filters, the folded files
  and the cursor across visits, in memory only, as Discuss keeps its scope,
  filters, selection and scroll today. Nothing is stored. A restored window
  never opens in Conversations, since the modality is never stored or
  inferred ([[BPR-0727]]).
- **Last used.** "A pull request opens in: Last used" may be Conversations or
  Discuss ([[BPR-0661]]).
- **Comment and Compose review.** They stay on the review bar. In
  Conversations, Comment answers the thread the reviewer picked and never a
  selected line, as in Discuss today (`comment_target_text`).
- **Names.** The drawer toggle's action is "Conversations drawer" today
  (`workbench_window.cpp:773-775`), while its tab and title-row toggle say
  Comments. Proposed: rename the action and its accessible names to "Comments
  drawer", so only the modality is called Conversations. Shown at
  BPR-1293:owner-look.
- **Not in Conversations.** Pull request comments, review summaries and
  threads that are not on a file. Discuss lists and reads them, as today;
  [[BPR-1294]] looks for a better home. Reading, replying, resolving
  and marking read happen in Review's drawer, not on this page: the finder
  only previews a post's first words on hover, as drawn.

## Discuss today, and where each part goes

**Background since 2026-10-06.** This inventory was written while
Conversations was to replace Discuss. After the owner's Q2 answer Discuss
stays, so it is kept only as the map of what Discuss does and how the finder
relates to it. Where its last column drops, removes or renames a part of
Discuss, read "kept in Discuss, unchanged". It does not govern the build: the
entry points follow [Entry points, as amended](#entry-points-as-amended), and
the acceptance below governs the rest.

Read from the code at `15f9de54c` (origin/main). BPR-1290 is changing the
drawer and Discuss's list in its branch; where that matters it is noted. Every
row goes somewhere or is dropped on purpose.

### Layout and arrangement

| Discuss today | Where | Under Conversations |
|---|---|---|
| A full centre page that hosts the Comments drawer's own panel, moved out of the drawer and back (`DiscussPage`, `apply_modality_surface`) | `workbench_window.cpp:1747-1751, 6052-6087` | Replaced by the finder page. The drawer's panel never leaves the drawer. |
| The left column: the thread list with scope, file choice and filters; below it the thread map | `github_conversation_panel.cpp:513-534, 955-1016` (`set_discuss`, `DiscussSplitter`) | The list becomes the finder's rows. The map is dropped: the finder draws branches as rows. |
| The right column: a feed, one card per thread with its code excerpt, every post on a rail, Reply, Resolve, Mark read and Open in code | `conversation_cards_view` feed mode (`set_feed`, `.hpp:203-211`); excerpt locator `workbench_window.cpp:1537` | Dropped from the page. Bodies and actions are read and used in Review's drawer after a pick. Code excerpts are dropped: Review shows the code. |
| The drawers are hidden while Discuss shows (they keep their states); the centre takes the whole width | `workbench_window.cpp:10560-10566, 10674-10678` | Kept (Q1). |
| Discuss's preset sets Files collapsed and Conversations expanded before the drawers are hidden | `choose_modality`, `workbench_window.cpp:5175-5200` | Dropped. Conversations changes no drawer state. |
| Below 85 average characters the columns stack and "Back to conversations" returns from the feed | `arrange_discuss`, `github_conversation_panel.cpp:523-531, 582-592` | Dropped. Picking leaves the page, so nothing needs a way back. |
| Summary line "n conversations, n unresolved, n new", and the file-scope note about pathless comments | `refresh_summary`, `github_conversation_panel.cpp:940-953` | The Only new chip carries the new count. The rest is dropped; pathless comments are BPR-1294's. |
| Remembers selection, scroll, scope and filters across visits | `github_conversation_panel.cpp:955-1016` | Kept as the finder's search, filters, folds and cursor. |
| Remembers the focused widget inside Discuss when leaving and restores it on return | `DiscussReturnFocus`, `workbench_window.cpp:6062-6087` | Entering focuses the search, as drawn; leaving without a pick returns focus to where it was before. |
| BPR-1290 (in flight) redraws Discuss's list as the drawer's notes in file order, each opening in place | BPR-1290:kept, as amended | Replaced by the finder when this card lands; BPR-1290 only has to keep Discuss working. |

### Scope and filters

| Discuss today | Where | Under Conversations |
|---|---|---|
| Whole PR (the default in Discuss) or File scope | `github_conversation_panel.cpp:909, 970-984` | Always the whole pull request, grouped by file; a file's group folds. |
| File choice (a combo box of the files with threads) | `conversationFileScope`, `github_conversation_panel.cpp:317-329, 662-681` | The search takes a file name or path. |
| Unread, Unresolved, Clear | `github_conversation_panel.cpp:330-358`; `discuss_filters` | Only new and Hide resolved; Esc clears the search. The label filter is new. |
| Pathless pull request comments and review summaries are listed in Whole PR | `conversation_cards_view` compact mode admits every top-level kind | Not listed: [[BPR-1294]]. See [the gap](#the-gap-until-bpr-1294-is-built). |

### Entry points

| Into Discuss today | Where | Under Conversations |
|---|---|---|
| The strip's Discuss segment, ⌘4 (⌘3 after BPR-1295) | `workbench_strip.cpp:700-787` | The Conversations segment, ⌘3. |
| A `#discussion_r<id>` link, when the read identified the comment | `reveal_conversation`, `workbench_window.cpp:9044-9070`; `github_application.cpp:12321-12329` | Review on the comment's file, the thread open in place at that post, as a pick does. No stop in Conversations. |
| A `#pullrequestreview-<id>` link reveals the review | Same | Not on a file: [[BPR-1294]]. Until then the link opens the pull request and says, under the bar and in the status bar, that a review is not shown from a link yet, as an `#issuecomment-` link does today. |
| After an inline comment is published, the window reveals the published post (in Discuss) | `select_published_conversation`, `github_application.cpp:9805-9828` | Review stays; the thread opens in place at the published post. |
| Previous and Next defect, when the defect's thread is not among the drawer's | `navigate_defect`, `workbench_window.cpp:4832-4858` | Review on the defect's file, the thread open in place. |
| An older-pair tab's Show them, when the window's comparison is no later than the tab's | `workbench_window.cpp:5805-5827`; specification, Tabs | Conversations with the search set to that file. Picking opens the thread among the file's unplaceable ones. |
| Overview Comments column chip drill-down: "opens Discuss filtered to that scope, label and mode" | Delivery contract `conventional-comments-feedback.md:411`; [[BPR-1089]], not built | Conversations with the file or folder in the search, the label filter, and Hide resolved for Open. BPR-1089 builds it. |
| Overview's ? chip "lists those conversations in Discuss" (unknown or not-resolvable states, unknown label) | `conventional-comments-feedback.md:402`; [[BPR-1089]], not built | No matching finder filter. BPR-1089 decides when it is built; noted on its delivery contract when this card lands. |
| Overview's "Not on a file" row | Same, BPR-1089 | [[BPR-1294]]. |
| Overview file rows, Continue review, Resume, a file link, tours | BPR-1295 inventory | Unchanged: they enter Review. |

### Leaving Discuss

| Today | Where | Under Conversations |
|---|---|---|
| A card's Open in code: Read (Review after BPR-1295) at the thread's line | `workbench_window.cpp:1524-1533`; [[BPR-0649]] | It becomes the pick. |
| Choosing Review or Overview puts the panel back in the drawer at its width | `apply_modality_surface` | Nothing moves, since the panel never leaves the drawer. |
| Code navigation beneath Discuss does not take the selected conversation ([[BPR-0929]]) | `workbench_window.cpp:1572-1580, 4873-4876` | Kept: the arrows move the retained comparison, not the cursor. |
| The tab driver treats Overview and Discuss as tab-less; leaving Discuss may cancel its window-pair diff | `tab_modality`, `github_application.cpp:6091-6095, 6169, 7344-7352, 8186-8189` | Conversations is tab-less in the same way. Its own comparison is never read: no window-pair diff starts for it. |

### Keys, strip, names and settings

| Today | Where | Under Conversations |
|---|---|---|
| Ctrl+4 (⌘4), by strip position | `workbench_strip.cpp:754-756` | ⌘3, set explicitly; ⌘J too. ⌘J is unbound in `src/` today. |
| Button "Discuss", tooltip "Collapse the files drawer and give the conversation the room." | `workbench_strip.cpp:708-731` | "Conversations", "Find a conversation in the pull request (⌘3, ⌘J)". |
| Control description "Overview, Read, Review and Discuss presets for which drawers are open"; object names `reviewModalityDiscussAction`/`Button` | `workbench_strip.cpp:739-776` | "Overview, Review and Conversations"; object names `reviewModalityConversationsAction`/`Button`, tests migrated. |
| Overview's refusal: "Choose Read, Review or Discuss to open one." | `workbench_window.cpp:326-329` | "Choose Review to open one." Conversations shows no drawer. |
| `qt::review_modality::discuss`, `core::workbench_modality::discuss` (no rule of its own) | `workbench_strip.hpp:223`; `workbench_interaction.hpp:100` | Renamed `conversations`. Under BPR-1295 the core disables ⌘B there. |
| Panel names `conversationDiscussSplitter`, `conversationDiscussColumn`, `conversationDiscussSummary`, `conversationBackToIndex`, `conversationFileScope`, `workbenchDiscussPage` | Panel and window | Removed with their tests. |
| Settings: no Discuss setting. "Last used" can be Discuss. The Comments filter setting (All, Unread, Unresolved) seeds the drawer | `workbench_runtime_ports.hpp:467-498`; `workbench_window.cpp:5138-5142` | Last used can be Conversations. The finder's chips start off; the Comments filter setting stays the drawer's. |
| Course reporting names the centre "discussion" and the modality "Discuss" | `github_application.cpp:17563-17580, 17675-17676` | "conversations" and "Conversations". |
| The composed review probe chooses Discuss to press Reply and Resolve on a card | `github_application.cpp:24114, 24194` | Presses them on the open thread in Review's drawer. |
| Review Cycle's "Discuss" response mode, and `cached_discussion` in the cached overview | `review_cycle_round.hpp:54`; `cached_review_overview.hpp` | Unrelated, unchanged. |

### Tests (counts of Discuss references at `15f9de54c`)

- `tests/adapters_qt/github_conversation_panel_tests.cpp` (41): the Discuss
  arrangement, file scope, filters and summary. Removed with the arrangement,
  except the filter cases that still test the drawer.
- `tests/adapters_qt/workbench_window_tests.cpp` (38), including
  `test_discuss_uses_main_content_and_open_in_code_returns_to_read` (7346),
  the presets (1584-1595), the seam round trip (2947-2954), Show them (6323),
  the refusal wording (9424) and the modality loops (5824, 7281, 9538, 9586,
  10763). They become Conversations cases: the page, the pick, ⌘J and Esc,
  the drawer states left untouched, and ⌘B disabled.
- `tests/end_to_end/setup_destination_public_path_tests.cpp` (9): the
  `#pullrequestreview-` reveal (571-575) becomes the interim refusal;
  `read_cli_source_update_discussion` (1019-1036, used at 1214, 1441, 1520)
  reads file threads in Conversations without code. Its pathless conversation
  is BPR-1294's.
- `tests/end_to_end/profile_multi_window_public_path_tests.cpp` (7),
  `profile_window_review_restoration_public_path_tests.cpp` (3),
  `tests/adapters_qt/composed_github_tests.cpp` (4: Discuss reveals PR-wide
  conversations without a focus action, 2031-2040),
  `composed_markdown_tests.cpp` (2: the full reader in Discuss, 290-294),
  `conversation_cards_view_tests.cpp` (4: the feed and `discuss_filters`).
- `tests/core/workbench_interaction_tests.cpp` (3 modality loops: 1010, 1053,
  1206).
- New: a finder view test (rows, branching, folding past nine, search, the
  three filters, keyboard) on `branching-thread-v1` and the recorded fixtures.

### Specifications and RFCs

| Text | Says today | Under Conversations |
|---|---|---|
| RFC-BPR-0028, Modalities paragraph and AC-03 | Overview, Review and Discuss (BPR-1295's amendment) | Amended 2026-10-06 in place, with [[BPR-1021]]'s AC-03 row. |
| RFC-BPR-0028, Discuss paragraph; AC-05's Discuss clause; AC-08's "Read and Discuss" | Discuss's list, map and feed; BPR-1290's branch amends all three | Not edited in place while BPR-1290's amendment to the same lines is in flight. RFC-BPR-0028's new section "Owner decision, 2026-10-06: Conversations replaces Discuss" says how they read. Worded in place, with BPR-1021's rows, by whichever of this card or BPR-1290 lands second. |
| Workbench specification: title row, Modalities, Tabs, Show them, Hide drawers, links, shortcut table | Discuss | Amended 2026-10-06. |
| Workbench specification: "Every thread card offers Reply, beside the code as in Discuss"; the Conversation drawer's "Retained Discuss borrows the same notes" (BPR-1290's); the thread map section | Discuss, and the map kept for this card | Owed when the code lands. The map section then says the finder draws branches as rows and `github_thread_map` is removed, unless the build reuses it. |
| RFC-BPR-0037 AC-09 and "Discuss without a selected file"; [[BPR-0926]], [[BPR-0929]] (Done) | Discuss lists PR conversations without a source file | Owed when the code lands: Conversations lists file conversations without loading source; pathless ones are BPR-1294's. Done cards stay as written. |
| RFC-BPR-0052 (195, AC-08) and [[BPR-1135]]'s AC-08 row | "Overview and Discuss have no tab"; "beside the code as in Discuss" | Owed when the code lands, with the row. |
| `docs/architecture/native-markdown.md:4, 106`; `docs/ux/conventional-comments-feedback.md:361, 402, 411` | Read and Discuss; drill-downs open Discuss | Owed when the code lands (they describe the window as built and BPR-1089's contract). |
| RFC-BPR-0031, RFC-BPR-0039, RFC-BPR-0050, `docs/development/private-alpha.md` | Name Discuss in closed or imported text | Historical; stay as written. |

### The gap until BPR-1294 is built

**No gap now** (amended 2026-10-06, Q2: "just keep discuss for now even after
1294"). Discuss stays, so pull request comments and review summaries stay
readable there and a `#pullrequestreview-` link keeps revealing its review in
Discuss. The rest of this section is the gap as it would have been.

Today Discuss is the only place in the application where a pull request
comment or a review summary can be read. With Conversations and the inline
drawer, neither shows them. Until [[BPR-1294]] gives them a home:

- They cannot be read in the application. Overview's header counts them ("n
  reviews · n pull request comments"), and Overview's cached text
  (`overviewCachedDiscussion`) shows them only while the overview is read from
  cache.
- A `#pullrequestreview-` link stops revealing its review and says why.
- A thread that cannot be placed stays readable in Review's drawer among its
  file's unplaceable threads (BPR-1290), so threads are not part of the gap.
- Replying to a pull request comment and the review summary at Submit are
  unaffected: they never went through Discuss.

Q2 asked whether the owner accepts that gap or wants Discuss's removal to
wait. The owner kept Discuss.

## Entry points, as amended

Decided 2026-10-06 after the owner's answers. Each entry point keeps the
destination it has today unless it is better as Review in place. Discuss's
own behaviour is not changed.

**Changed: Review in place.** These name a conversation on a file, the place
that reads it with its code is Review's inline drawer, and a pick from
Conversations lands there too. Each enters Review in its remembered
arrangement on the thread's file, opens the Comments drawer if it is closed (a
manual open), and opens the thread in place at the named post, or among the
file's unplaceable threads when it cannot be placed.

- **A `#discussion_r<id>` link** (`reveal_conversation` from the link
  location), when the read identified the comment. Today it reveals the
  comment in Discuss.
- **The published-post reveal** (`select_published_conversation`): after an
  inline comment is published, the window stays in Review with the thread
  open at the published post. Today it switches to Discuss.
- **Previous and Next defect** (`navigate_defect`), when the defect's thread
  is not among the drawer's: Review on the defect's thread's file. Today it
  switches to Discuss. A defect thread that is not on a file still goes to
  Discuss.

**Kept: Discuss, as today.**

- **A `#pullrequestreview-<id>` link** reveals the review in Discuss. A review
  is not on a file.
- **An older-pair tab's Show them**, when the window's comparison is no later
  than the tab's (`show_later_threads`), opens Discuss. Conversations is for
  navigation only and is not better here.
- **A Discuss card's Open in code** returns to Review at the thread's line, as
  BPR-1295 leaves it.
- **Overview's Comments column drill-down, its ? chip and its "Not on a file"
  row** ([[BPR-1089]], not built) open Discuss as the delivery contract says.
  BPR-1089 may choose Conversations for file scopes when it is built.

**Into Conversations:** ⌘3, the strip segment, ⌘J and the drawer's finder
button only.

## Questions for the owner

Asked in chat, one question each, Accept or Reject. Both answered 2026-10-06
([Owner answers](#owner-answers-2026-10-06)): Q1 "Hide drawers" (Accept); Q2
"just keep discuss for now even after 1294" (Reject: Discuss is kept, and not
only until BPR-1294).

1. **Q1. Drawers while in Conversations.** The finder takes the whole centre
   and the drawers are hidden while it shows, then come back exactly as Review
   left them, as Discuss works today. The prototype's Tab kept the Comments
   drawer beside the finder; as a modality it would show only the last file's
   threads. Accept, or Reject to keep the drawers beside it.
2. **Q2. Comments that are not on a file, until BPR-1294.** When Discuss goes,
   pull request comments and review summaries cannot be read in the
   application until BPR-1294 is built, and a `#pullrequestreview-` link says
   so rather than showing the review ([the gap](#the-gap-until-bpr-1294-is-built)).
   Accept the gap, or Reject to keep Discuss until BPR-1294 lands.

## Sequence

The code waits for [[BPR-1290]] (In Progress: the inline drawer this card
picks into) and [[BPR-1295]] (Ready: Read into Review, the ⌘1 to ⌘3 keys and
⌘B). All three change the modality strip, `workbench_window.cpp` and the
window tests. This is recorded as `sequence_after`, not `depends_on`, because
the board keeps a card with unfinished dependencies in Backlog. Q1 and Q2 were
answered on 2026-10-06.

## Acceptance

Read with the [owner amendment of 2026-10-06](#owner-amendment-2026-10-06-discuss-on-3-conversations-on-4):
wherever a criterion below gives Conversations ⌘3 and Discuss ⌘4, the keys
are Conversations ⌘4 and Discuss ⌘3.

Amended 2026-10-06 for the owner's answers: :modality, :keys, :entries,
:governing-text, :tests and :owner-look follow them, and :removed is replaced
by :discuss-kept.

- **BPR-1293:owner-decision.** The owner's words on prototype v2 and the
  placement are recorded here verbatim. Recorded 2026-10-06 (above).
- **BPR-1293:owner-questions.** Q1 and Q2 are answered by the owner and
  recorded here verbatim before the build starts, and the build follows them.
  Recorded 2026-10-06 ([Owner answers](#owner-answers-2026-10-06)).
- **BPR-1293:modality.** The strip offers Overview, Review, Conversations and
  Discuss on ⌘1, ⌘2, ⌘3 and ⌘4, set explicitly. Conversations has its own
  value in the core's and the Qt adapter's modality enums, its own segment,
  tooltip, accessible name and object names. The drawer toggle's action is
  named Comments drawer.
- **BPR-1293:finder.** Conversations draws the finder faithful to prototype
  v2: one row per conversation grouped by file, with its line, the first
  post's label mark, its first words, an avatar per post in posting order, and
  the new count, resolved tick or draft lock. Rows fold their oldest read posts
  into "+n" past nine, never folding new posts or the picked one. Branch rows
  appear only where a post has two or more replies that have replies, and
  `branching-thread-v1` draws as the prototype draws it. Search covers words,
  a person, a file and a line number; the filters are Only new, Hide resolved
  and one label. The keys are Up/Down, Left/Right, Return and Esc as drawn. It
  renders without clipping in light, dark and both high-contrast themes, at
  larger text and at the platform's narrowest window. It does not list
  comments that are not on a file.
- **BPR-1293:pick.** Picking a row, an avatar or the cursor with Return
  enters Review in its remembered arrangement on the conversation's file, opens
  the Comments drawer if it is closed, and opens the thread in place at its
  line with the picked post washed amber and scrolled into view. Reopening
  Conversations starts on that conversation with its path traced. A thread
  that cannot be placed opens among the file's unplaceable threads.
- **BPR-1293:keys.** ⌘J (Ctrl+J) and the drawer's finder button enter
  Conversations from Overview, Review or Discuss with the search focused. ⌘J,
  or Esc on an empty search, returns to the modality it came from. No key
  sequence is duplicated in the assembled window. Under Q1 as answered, the
  drawers are hidden while Conversations shows, entering and leaving it
  changes no drawer state, and ⌘B is disabled there.
- **BPR-1293:entries.** Every entry point lands where
  [Entry points, as amended](#entry-points-as-amended) says: `#discussion_r`,
  the published-post reveal and defect navigation in Review with the thread
  open in place; `#pullrequestreview-`, Show them and a Discuss card's Open in
  code where they land today. Window and public-path courses cover each
  changed entry and each kept one.
- **BPR-1293:discuss-kept.** Discuss keeps its page, arrangement, preset, file
  scope, filters, summary, map, feed, Open in code, tooltip and names, and
  reads pull request comments and review summaries as before; only its key
  moves to ⌘4. Its existing courses pass with no change other than the key
  and the modality loops gaining Conversations.
- **BPR-1293:governing-text.** The workbench specification and RFC-BPR-0028
  describe the build; RFC-BPR-0052's "Overview and Discuss have no tab" names
  Conversations too, with [[BPR-1021]]'s and [[BPR-1135]]'s rows matching
  their criteria; and the documentation gate passes. Text that describes
  Discuss stays as written.
- **BPR-1293:tests.** The modality loops and key cases in the inventory gain
  Conversations and Discuss on ⌘4, Conversations window cases cover the page,
  the pick, ⌘J and Esc, the hidden drawers and ⌘B, and a finder view test is
  added. The focused core, Qt and end-to-end selectors pass, with
  `qt_composed_github` included per the board's selector trap.
- **BPR-1293:owner-look.** The owner uses Conversations in the built
  application in an attended session and accepts it, with one question each for
  the finder's drawing, picking, the keys (⌘3 for Conversations, ⌘4 for
  Discuss, and ⌘J), and the drawer toggle's new name. Required, not
  self-certified.

## Boundaries

Presentation in the Qt adapter, plus the core's modality enum and the window's
modality wiring. No new authority and no GitHub, Git or SQLite effect: the
finder reads the conversation projection the drawer already reads, and every
gesture it starts is a file selection and a thread selection that already
exist. It does not change Discuss's behaviour beyond its key, give comments
that are not on a file a new home ([[BPR-1294]]), redesign the drawer
([[BPR-1290]]) or change Review's arrangement and ⌘B ([[BPR-1295]]).

## Built v001, 2026-10-06

Candidate `7778886c9`, on `origin/main` at `f5a64e041`. Drawn with
`frontend-design` against prototype v2. One implementation commit; the claim is
`5d5de1c02`.

- **Core** (`workbench_interaction`). `workbench_modality::conversations`.
  Entering and leaving it changes no drawer; Review comes back with the
  arrangement it last had, hidden drawers included. ⌘B is refused there with
  the new `conversations_finds_a_conversation`.
- **Strip.** Overview ⌘1, Review ⌘2, Discuss ⌘3, Conversations ⌘4, in that
  order. The card's text ("the strip's order is unchanged by the keys … read it
  with the keys swapped") was read as the segments following their keys,
  since each segment prints its key; the owner-look shows it. Conversations'
  tooltip names both keys; Discuss's segment, tooltip and names are unchanged.
  The drawer toggle and panel are named the Comments drawer.
- **⌘J** (`showWorkbenchConversationsAction`, View menu, window context) and
  the Comments drawer's finder button go to Conversations from Overview,
  Review or Discuss; ⌘J again, or Esc on an empty search, goes back to where it
  was entered from, focus included. The drawers are hidden there.
- **The finder** (`conversation_finder`, Qt). Rows grouped by file, a header
  that stays at the top while its rows scroll, line, mark, words, one avatar per
  post in posting order, the new count, resolved tick or my draft's lock; blue
  new dots, the amber row and path for where Review is, resolved rows faded.
  Branch rows only where a post has two or more replies that have replies.
  "+n" folds the oldest read posts past nine, never new posts, the path to the
  picked post, the search's or the label's; it unfolds the row. Search over
  words, a person ("you" for me), a file or a line; Only new with its count,
  Hide resolved, and the label menu. Keys as the prototype: typing goes to the
  search, Up and Down (Up past the first row returns to the search), Left and
  Right along a row's avatars with a preview or folding a file, Return picks;
  hovering an avatar previews its post. The list model applies filters by
  identity (inserts and removes, no reset) and the cursor stays on its row or
  on none. It rebuilds from the forest only when shown.
- **Counts.** The label filter reads [[BPR-1087]]'s `feedback_counter_model`
  through the new `application::project_conversation_labels`: a conversation is
  its root's label, every state, so the finder's label counts are the
  Overview's chips' numbers for the conversations on a file. The "Only new"
  count is the ledger's read state, which BPR-1087 does not count. Marks are
  the shared label marks (Icons or Letters).
- **Thread map.** Not reused: the finder draws branches as rows.
  `github_thread_map` and its test are removed; its rules that apply (pre-order
  with an explicit stack, the traced path, an unexpected shape costing nesting
  and never a post) live in the finder.
- **Pick.** Review, then the file through the files drawer's intent (in that
  order, so the tab driver keeps the file), Comments opened by an exact set
  intent if it is closed, and once that file's comparison is the one shown the
  thread opens in place at the picked post: `conversation_cards_view::open_thread_at`
  washes the post amber and brings it into view once the code has settled, the
  foot of a tall thread from the drawer's own overflow. The selection authority
  names the picked post.
- **Entries.** A `#discussion_r` link the read identified, the reveal of a
  just-published inline comment and Previous/Next defect for a thread the
  drawer does not hold open in Review in place (a defect thread not on a file
  still goes to Discuss). `#pullrequestreview-`, Show them, a Discuss card's
  Open in code and the review page's preflight rows land where they did.
  Comment in Conversations answers the picked thread, as in Discuss.
- **Fixture** `conversations-v1`: prototype v2's pull request, its ten files
  verbatim from `code.js` and its 21 conversations (91 posts), line 667
  branching with an aside that has replies; posts in the order their ages say,
  so `read_state.json` reproduces the prototype's new posts through the ledger
  (`--attended-read-state`, tests-on).
- **Governing text.** The specification's Modalities paragraph (as built),
  links, shortcut table, defect and thread-map passages; RFC-BPR-0028's
  Conversations section; RFC-BPR-0052's tab paragraph and AC-06, with
  [[BPR-1135]]'s AC-06 row word for word; the fixture catalog.
  `DOCUMENTATION_OK`.
- **Round-trip pins** resealed with a note over BPR-1301's (mutation pin
  unchanged).

### Verification on the candidate

- Debug (`build/dev`), macOS arm64, merged tree: 57 of 57 focused rows, among
  them `core_workbench_interaction`, `application_overview_feedback`,
  `qt_conversation_finder_scale_{100,125,200}`, `qt_workbench_window_scale_*`,
  `qt_composed_github_scale_*`, `qt_composed_inline_drawer_scale_*`,
  `qt_composed_markdown_*`, `qt_conversation_cards_view`,
  `qt_github_conversation_panel`, `qt_pull_request_overview_page`,
  `application_attended_recent_replay`, the setup-destination, multi-window,
  restoration, review-submission and overview-labels public paths,
  `application_review_round_trip_composition_contract`,
  `application_composition_contract`, `architecture_boundaries`,
  `github_recorded_fixture_contract`, `github_contract_fixture_security` and
  `application_large_review_responsiveness`. Before the rebase,
  `application_profile_multi_window_public_path` failed once at "New Window
  remains usable after initial retirement" under `-j4`; it passed alone twice
  and in the merged run.
- **New rows.** Core: `test_conversations_changes_no_drawer` and the modality
  loops. `qt_conversation_finder_scale_*`: the row rule (flat, the prototype's
  deep shape, a leaf at a fork, hostile parents, a 20,000-post chain),
  conversations on a file only, search, filters, labels, folding, keys,
  pointer picks, and renders light, dark and both high-contrast at larger text
  and 640 px with nothing clipped. Window: `test_conversations_finds_and_picks_into_review`
  through the real core (⌘J, Ctrl+4, the strip and the button from every
  modality; hidden drawers with states unchanged; ⌘B disabled; Esc; the pick's
  order, the Comments open, the wait for the file's comparison, the picked
  post; entries by identity). Application: `test_each_label_lists_the_conversations_it_counts`.
  Large review: `conversations_enter`, `_search`, `_search_clear`, `_leave`,
  `_pick` against 100 ms and `_pick_open` against 2,500 ms with its input gap.
  The key scans: Ctrl+3 Discuss, Ctrl+4 and Ctrl+J Conversations, each once.
- **Failing-before controls**, mutations of the product, each failing its row:
  ⌘B offered in Conversations; Conversations opening Comments (core); every
  reply its own row; new posts folding (finder); drawers shown in
  Conversations; the file selected before Review; Conversations on Ctrl+3
  (window); the thread opened before its file shows (large review).
- **Optimized tests-on tree** (the frozen root's, Release, `-j1`): 32 of 32,
  `BudgetsEnforced` true, no input failures or known misses:
  conversations_enter 9.9 ms, search 1.8, clear 2.5, leave 29.4, pick 18.4,
  pick to thread open 110.5 (input gap 10.8); drawers_hide 18.5,
  review_modality 32.3.
- The real application, offscreen on `conversations-v1`: the finder as drawn,
  the deep pick opening at its last post washed amber, and the path traced on
  reopening (checked with a local capture not committed).
- Format: the coding standard's clang-format on every changed file.

### Owner look, session v001

Pending: BPR-1293:owner-look is the owner's, not self-certified. On the
coordinator's instruction it is one question, Accept or Reject, "Conversations
(⌘4) matches the prototype", rather than the four this card's criterion lists;
a note carries any change.

- Session `2588378b1ac64914f3302695cdb348bf`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1293-conversations-v001`,
  ready at event cursor 4. Candidate `BPR-1293-conversations-v001` (revision
  `frozen-7778886c9-v001`). The host was started detached and answered on
  loopback.
- Frozen root `/Users/jaeger/bpr1293-conversations-v001.sRXUZr`: `git archive`
  of `7778886c9` built there Release with tests on, with its own CPM cache
  clone, source read-only; executable SHA-256 `83f003ddfe0b8e9e…`. Offscreen
  the launcher printed `ATTENDED_READ_STATE:18` and `ATTENDED_FILE_SHOWN`.
- Launch: `python3 /Users/jaeger/bpr1293-conversations-v001.sRXUZr/launch.py`.
  It checks the build and the fixture tree first and reports an exit within
  seconds.

Next: read the session's decision (`status --after 4`). Accept satisfies
:owner-look and the card finishes; Reject comes back with its note for a v002.

### Owed station runs (not blocking)

Linux and Windows have not run this change. At `origin/main` holding
`7778886c9`, no remote mutation:

- `bpr-main-linux`: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure -R '^(core_workbench_interaction|application_overview_feedback|qt_conversation_finder_scale_.*|qt_workbench_window_scale_.*|qt_composed_github_scale_.*|qt_composed_inline_drawer_scale_.*|qt_conversation_cards_view|qt_github_conversation_panel|application_attended_recent_replay|application_setup_destination_public_path|application_profile_multi_window_public_path|application_review_round_trip_composition_contract|application_composition_contract|architecture_boundaries|github_recorded_fixture_contract|github_contract_fixture_security)$'`,
  then in the optimized tests-on tree
  (`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON -DCMAKE_TEST_BUILD_DEPENDS=ON && cmake --build build/app-release-tests && ctest --test-dir build/app-release-tests -j1 --output-on-failure -R '^(application_large_review_responsiveness|application_first_tab_comparison|qt_conversation_finder_scale_.*|qt_workbench_window_scale_.*)$'`).
- `bpr-main-win`: `cmake --preset windows-dev && cmake --build --preset windows-dev && ctest --preset windows-dev -j4 --output-on-failure -R` with the same first expression, then the optimized tree (`build/bpr0967-release`, MSVC `/O2`) with the second.

A failure there opens a new card against this change.

## Owner decision, session 2588378b (2026-10-07)

Session `2588378b1ac64914f3302695cdb348bf`, read from cursor 4 to event 5. The
owner said "handed back" in chat. The session had recorded no handed-back
event, but the decision is saved.

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1293-conversations-v001` | Accept (`941b04a3`, 14:51:07Z) | (none) |

The verdict was given on the frozen build of `7778886c9`. It satisfies
BPR-1293:owner-look, asked as the one question the coordinator set:
"Conversations (⌘4) matches the prototype". Every other criterion stands on
its recorded evidence at `7778886c9` ([Built v001](#built-v001-2026-10-06)
and [Verification on the candidate](#verification-on-the-candidate)). The
card is Done.

The session's candidate was released (`resume-development --all`, event 6). The host
was already stopped. The frozen root
`/Users/jaeger/bpr1293-conversations-v001.sRXUZr` was deleted.

The [owed station runs](#owed-station-runs-not-blocking) remain owed and do
not block. A failure there opens a new card against this change.
