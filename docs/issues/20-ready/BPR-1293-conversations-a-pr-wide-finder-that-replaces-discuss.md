---
schema: "implement-rfc.card.v1"
id: "BPR-1293"
title: "Conversations: a PR-wide finder that replaces Discuss"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06, during BPR-1290","reservation":"docs/issues/_reservations/BPR-1293--BPR-1293--370e927e88e9.json","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028","RFC-BPR-0037","RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1295","BPR-1294","BPR-1089","BPR-1021","BPR-1135","BPR-0418","BPR-0211","BPR-0649","BPR-0926","BPR-0929"],"implementation_note":"Build faithful to docs/design/BPR-1290-inline-drawer-v2 with the frontend-design skill; owner review is required","owner_decision":{"date":"2026-10-06","prototype":"I like that a lot.","placement":"What if we were to replace discuss with conversations."},"open_questions":["Q1 drawers while in Conversations","Q2 the gap for comments not on a file until BPR-1294"],"acceptance":["BPR-1293:owner-decision","BPR-1293:owner-questions","BPR-1293:modality","BPR-1293:finder","BPR-1293:pick","BPR-1293:keys","BPR-1293:entries","BPR-1293:removed","BPR-1293:governing-text","BPR-1293:tests","BPR-1293:owner-look"],"sequence_after":"BPR-1290 and BPR-1295 (all three change the modality strip, workbench_window.cpp and the window tests; this card's code waits for both)"}
---

# BPR-1293: Conversations: a PR-wide finder that replaces Discuss

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

**Conversations** replaces **Discuss** as the third modality. The modalities
are Overview ⌘1, Review ⌘2 and Conversations ⌘3 (Read merges into Review under
[[BPR-1295]]). Conversations puts the finder from
[prototype v2](../../design/BPR-1290-inline-drawer-v2/index.html) in the main
pane: every conversation in the pull request that is on a file, grouped by
file, for **navigation only**. Picking one switches to Review on its file, with
the thread open in the inline drawer ([[BPR-1290]]) at the picked post, as the
prototype's Tab placement does. ⌘J (Ctrl+J) goes straight to Conversations.

Pull request comments and review summaries that are not on a file are not in
Conversations. They are [[BPR-1294]]'s, whose leading direction is Overview.

### As first filed (superseded 2026-10-06)

A view for **navigation only**: the Treeverse finder ([[BPR-0418]], the
owner's [[BPR-0211]] choice) across every conversation in the pull request,
grouped by file, used to jump to the right conversation. Picking one opens its
file in Read at its line with the thread open in the drawer (BPR-1290). It is
not tied to the Read or Discuss modalities.

It does not hold or read pull request comments or review summaries that are not
on a file. Those are [[BPR-1294]]'s open problem.

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
  modality, **Conversations**, in the main pane, and it replaces Discuss. The
  open question "main-pane tab or summoned window" is closed.
- **Picking works as the Tab placement did.** It switches to Review on the
  picked conversation's file, with the thread open in the inline drawer pinned
  at its line and the picked post washed amber and scrolled into view.
- **⌘J stays** as the shortcut that goes straight to Conversations.

The points this card left open are settled as follows:

- *The finder's drawing.* Prototype v2's, kept by the owner.
- *Deep threads* (`branching-thread-v1`, RFC-BPR-0028 AC-08). Conversations
  shows a conversation's branches as indented rows only where they exist, and
  the drawer reads the thread whole (BPR-1290). The thread map is not used.
- *What happens to Discuss.* Conversations replaces it (inventory below).

## What Conversations does

Proposed from the owner's decision and the prototype. Q1 and Q2 below are the
two points left to the owner; the rest is shown at BPR-1293:owner-look.

- **The modality.** The strip's third segment, "Conversations", on ⌘3 (set
  explicitly, as BPR-1295 sets ⌘1 to ⌘3). Overview, Review and Conversations
  are the only modalities. Its tooltip: "Find a conversation in the pull
  request (⌘3, ⌘J)".
- **The page.** The finder fills the centre, as Discuss's page does today: the
  search field with ⌘J printed in it, the Only new chip with its count, Hide
  resolved, one label, and the list grouped by file. The review bar stays, as
  it does in every modality. Its file and change arrows move the comparison
  retained underneath, as they do under Discuss today ([[BPR-0929]]), and do
  not move the finder's cursor.
- **The drawers (Q1).** Recommended: while in Conversations the drawers are not
  shown, and nothing changes their states. Review comes back with the
  arrangement it last had (BPR-1295), hidden drawers included. Discuss's preset
  (Files collapsed, Conversations expanded) is not carried over. ⌘B is
  disabled in Conversations, as BPR-1295 disables it in Discuss.
- **Entering.** ⌘3, the strip segment, ⌘J, and the finder button at the top of
  the Comments drawer (the prototype's, tooltip "Find a conversation in the
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
- **Last used.** "A pull request opens in: Last used" may be Conversations, as
  it may be Discuss today ([[BPR-0661]]).
- **Comment and Compose review.** They stay on the review bar. In
  Conversations, Comment answers the thread the reviewer picked and never a
  selected line, as in Discuss today (`comment_target_text`).
- **Names.** The drawer toggle's action is "Conversations drawer" today
  (`workbench_window.cpp:773-775`), while its tab and title-row toggle say
  Comments. Proposed: rename the action and its accessible names to "Comments
  drawer", so only the modality is called Conversations. Shown at
  BPR-1293:owner-look.
- **Not in Conversations.** Pull request comments, review summaries and
  threads that are not on a file ([[BPR-1294]]). Reading, replying, resolving
  and marking read happen in Review's drawer, not on this page: the finder
  only previews a post's first words on hover, as drawn.

## Discuss today, and where each part goes

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

Q2 asks whether the owner accepts that gap or wants Discuss's removal to wait.

## Questions for the owner

Asked in chat, one question each, Accept or Reject.

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
the board keeps a card with unfinished dependencies in Backlog. Q1 and Q2 are
answered before the build starts.

## Acceptance

- **BPR-1293:owner-decision.** The owner's words on prototype v2 and the
  placement are recorded here verbatim. Recorded 2026-10-06 (above).
- **BPR-1293:owner-questions.** Q1 and Q2 are answered by the owner and
  recorded here verbatim before the build starts, and the build follows them.
- **BPR-1293:modality.** The strip offers Overview, Review and Conversations
  on ⌘1, ⌘2 and ⌘3, set explicitly. No Discuss remains in the core, the Qt
  adapter, tooltips, accessible names or object names. The drawer toggle's
  action is named Comments drawer.
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
  Conversations from Overview or Review with the search focused. ⌘J, or Esc on
  an empty search, returns to the modality it came from. No key sequence is
  duplicated in the assembled window. Under Q1 as answered, entering and
  leaving Conversations changes no drawer state, and ⌘B is disabled there.
- **BPR-1293:entries.** Every entry point in the inventory lands where the
  inventory says: `#discussion_r`, the published-post reveal and defect
  navigation in Review with the thread open in place; Show them in
  Conversations on that file; `#pullrequestreview-` per Q2. Window and
  public-path courses cover each.
- **BPR-1293:removed.** Discuss's page, arrangement, file choice, summary,
  back button, feed mode, excerpt locator and `discuss_filters` are removed,
  and `github_thread_map` too unless the finder reuses it, with their tests
  removed or migrated.
- **BPR-1293:governing-text.** The amendments owed when the code lands
  (inventory above) are made, with [[BPR-1021]]'s and [[BPR-1135]]'s rows
  matching their criteria, and the documentation gate passes.
- **BPR-1293:tests.** The tests in the inventory are updated and a finder
  view test is added. The focused core, Qt and end-to-end selectors pass, with
  `qt_composed_github` included per the board's selector trap.
- **BPR-1293:owner-look.** The owner uses Conversations in the built
  application in an attended session and accepts it, with one question each for
  the finder's drawing, picking, the keys (⌘3 and ⌘J), and the drawer
  toggle's new name. Required, not self-certified.

## Boundaries

Presentation in the Qt adapter, plus the core's modality enum and the window's
modality wiring. No new authority and no GitHub, Git or SQLite effect: the
finder reads the conversation projection the drawer already reads, and every
gesture it starts is a file selection and a thread selection that already
exist. It does not give comments that are not on a file a home
([[BPR-1294]]), redesign the drawer ([[BPR-1290]]) or change Review's
arrangement and ⌘B ([[BPR-1295]]).
