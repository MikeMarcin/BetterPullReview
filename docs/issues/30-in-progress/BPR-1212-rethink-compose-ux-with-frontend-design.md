---
schema: "implement-rfc.card.v1"
id: "BPR-1212"
title: "Rethink the Compose UX with frontend-design"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"BPR-0966 final owner verdict, 2026-09-26","related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"related_issues":["BPR-0966","BPR-1203","BPR-1207","BPR-1161","BPR-1083"],"requested_skill":"frontend-design","timing":"Later follow-up; not active m2b work or a blocker for accepted Dif placement","decision_id":"3bcafec9-7811-46dd-83dd-f7bda4cdac1b","candidate":"BPR-0966-compose-slot-v002","candidate_revision":"388f814c7be796b91c4dd6779422be3d4321e4f3","mandate_revision":"r4-with-owner-compose-20260925-r3","design_round":"docs/design/BPR-1212-compose-v1/index.html","open_questions":["Q1 Return/Shift+Return","Q2 local Suggest change","Q3 Compose review placement"]}
---

# Rethink the Compose UX with frontend-design

## Owner request and boundary

The owner accepted Dif's placement in the Compose workshop header, then requested
a separate, later redesign of the overall Compose experience. Their final note,
verbatim:

> Dif is fine.
> I utterly hate the compose UX and it needs a full rethink with $frontend-design as a later follow-up.

Attended session `54fd75833481882c3bb47745f8e30b75`, final Accept at sequence 5,
decision `3bcafec9-7811-46dd-83dd-f7bda4cdac1b`, handed back at sequence 6.
Candidate `BPR-0966-compose-slot-v002`, product
`388f814c7be796b91c4dd6779422be3d4321e4f3`, fingerprint
`87eef088bdcccaf17caeaa52833756bca5bb4bee56287ccbe81a1bc888f8e3fd`.
[The exact verdict](../../rfcs/plans/bpr0966-dif-cues/owner-verdict-compose.json)
retains the superseded earlier Reject as history.

This is a full UX rethink, not merely a label fix. Start from the integrated
product when this card is activated, including [[BPR-1203]]'s kept-draft state
and [[BPR-1207]]'s usable Comment entry. Preserve those independently owned m2b
fixes. This later card does not expand m2b, undo [[BPR-0966]]'s accepted Dif
placement, or reopen RFC-BPR-0037's historical acceptance by implication.
No redesign has been undertaken or accepted in this registration round.

## Intended work

Use the requested `frontend-design` skill in the later design round. It was
not available in the registration session; resolve the actual installed skill
before design work and report absence rather than silently substituting a
style. Develop concrete alternatives from the actual Compose workflows:
whole-review summary and disposition, new inline comment, thread reply, kept
or unavailable drafts, and AI workshop proposals with explicit acceptance.
Make the destination, scope, draft state, navigation and next action clear.
Translate the chosen design to native C++/Qt presentation; a browser prototype
is design evidence, not the implemented product.

Keep GitHub as shared authority, drafts private until explicit publication,
and the existing host/core gates for account, PR, head, line, session and draft
revision. Preserve review/comment/reply semantics, retained draft content,
keyboard return paths and accessible controls. Dif remains decorative with one
widget; redesigning the surrounding page does not authorize asset changes or
invented success cues.

## Related: sharing a Review Cycles round ([[BPR-1161]], 2026-10-02)

The owner rejected Review Cycles' Compose feedback page,
`BPR-1161-compose-share-v001`, at event 834. Their note: "I have no idea how
I got to this screen, why I would wnat to do any of these actions, and what
is important." The `frontend-design` round that followed lives on BPR-1161,
because it is that card's RC61-05 and RC61-06 and not this card's pull
request flows. The round is [design round
v1](../../design/BPR-1161-share-round-v1.html), with directions A (a share
sheet), B (sharing from the Notes tray) and C (the round as a letter), and
the owner's choice is asked as `BPR-1161-share-direction-v001`. When this
card is activated, start from that choice and its plain-word vocabulary. It
covers a labelled entry, purpose stated beside each action, one primary
action, and machine detail behind "exact text". Do not redraw it as a
separate convention. This note neither activates this card nor changes its
criteria.

## Owner amendment: an inline composer beside the code ([[BPR-1083]], 2026-10-05)

Accepting `BPR-1083-chip-menu-v002` in attended session
`c2a2442c5ce16c3fbb77598fc87360bb` (decision
`0df6d634-f0bf-49c2-b41c-d786bc3e3f64`, 2026-10-06T02:50Z), the owner wrote,
verbatim:

> Functionality works as expected.
>
> I hate this full screen comment page being the default. I want to be able
> to still see the code I'm commenting on. Maybe it has an expand button to
> get something similar to this.

Their screenshot,
[`BPR-1083-owner-ref-full-page-composer.png`](../../design/owner-refs/BPR-1083-owner-ref-full-page-composer.png),
shows Read after Comment on a line of `renderer.cpp`: the comment page
(Back, the target line, Publish and Discard, the label chip and the body)
fills the whole comparison area, and no code is visible. The note's third
paragraph, about the decorations, is BPR-1083's and was built there.

This card's "new inline comment" flow already covers the ask, so it is
recorded here and not filed separately (coordinator, 2026-10-05: "if 1212
already covers it, amend 1212"). It adds AC5 below. The rest of this card's
scope, timing and Backlog lane are unchanged.

## Acceptance

- AC1: Use `frontend-design` to inventory the integrated flows and present concrete native-feasible layout and interaction alternatives, including normal, empty, unavailable/conflicted and kept-draft states. Record the chosen design and rationale against the owner's full-rethink request.
- AC2: Implement the chosen composition flow with visible target and draft context, usable authoring space and clear local accept/reject versus publish actions. Preserve exact identity, draft retention, explicit publication, keyboard navigation and accessibility through the existing core/host/Qt boundaries.
- AC3: Exercise the actual composed entry-to-edit-to-return paths using safe fixtures, including comments, replies, whole reviews and workshop proposals; prove draft preservation, stale-target refusal and zero publication from navigation or local acceptance. Verify rendered narrow/wide and larger-font states on each claimed platform and keep unavailable native evidence explicit.
- AC4: Obtain an owner judgment on the built Compose UX at its exact candidate, with one question per independent criterion and Accept/Reject for keep-or-change decisions. Record any resulting follow-ups without treating the earlier Dif Accept as Compose acceptance.
- AC5 (owner amendment, above): A new inline comment, and a reply on a thread, open their composer inline beside the code by default, so the commented line and its neighbours stay visible while writing. An expand control, with a keyboard route and an accessible name, opens the full-page composer with the same draft, and returns from it to the inline one. Both keep the draft, label controls, Publish and Discard, and neither adds a scroll trap or a clipped composer (RFC-BPR-0050).
  Amended 2026-10-06 by the owner's direction below ("I want add comment to show up in the drawer instead of the full screen add comment window. Fold add comment window into discuss. Drawer comment should have something like a maximize button to go view it in discuss."): the inline composer is a draft thread in the inline drawer ([[BPR-1290]]) at its line, and the expand control is a maximize that opens the same draft in Discuss, where the full composer now lives, and returns it to the drawer. The full-page comment window is no longer a destination of its own. The rest of AC5 stands.
- AC6 (owner direction, below): In Review, a right-click on a line or a selection offers Add comment, which starts the draft in the drawer as AC5 says. Whether Suggest change, the Return and Shift+Return keys and the local-project flow follow the proposed design is settled by the owner's answers to Q1 to Q3 below.

## Owner direction: compose in the drawer, one flow with Review Cycles ([[BPR-1290]], 2026-10-06)

Handing back BPR-1290's built drawer v003, the owner wrote in chat, verbatim
(the BPR-1290 verdict itself is recorded on that card):

> I want add comment from the right-click menu. I want add comment to show up
> in the drawer instead of the full screen add comment window. Fold add
> comment window into discuss. Drawer comment should have something like a
> maximize button to go view it in discuss.
>
> I want to try to unify a bunch of this interaction with the local project
> flow. Maybe we do right click to add comment / suggest change with enter /
> shift+enter as shortcuts?

Earlier, accepting BPR-1083 (quoted in full under AC5's amendment above):

> I hate this full screen comment page being the default. I want to be able
> to still see the code I'm commenting on.

The coordinator activated this card on 2026-10-06 for a design round: an
inventory of today's compose entry points and keys in both flows, a unified
right-click and keyboard flow with the new comment drafted in the inline
drawer and a maximize into Discuss, a clickable prototype extending
[`BPR-1290-inline-drawer-v2`](../../design/BPR-1290-inline-drawer-v2/index.html),
and owner questions answered in chat. No attended session.

## Design round v1: compose in the drawer, 2026-10-06

Drawn with `frontend-design`. Read from the code at `f229d1b78`. Nothing in
`src/` changed.

### How writing starts today: pull requests

| Entry | What it does | Where |
|---|---|---|
| Review bar **Comment ▾**, Alt+C (⌥C) | Replies to the selected thread if there is one ([[BPR-1133]]). Otherwise it opens the full-page composer on the selected line. Its menu names the target ("Comment on line 431…") and lists no labels | `workbench_window.cpp:1222-1257` (`makeComposition`, `CommentMenu`); `comment_reply_thread` 6122, `comment_target_text` 6151, `update_comment_target` 6166, `comment_or_reply` 6215 |
| **Compose review**, Alt+R | The full-page whole-review composer | Same lines; `open_review_composition` 6324 |
| Pull Request menu | Lists both commands | `workbench_window.cpp:1256-1258` |
| The full-page composer | A page in the centre stack holding Back and the editor (`review_draft_editor`). Esc is Back. It replaces the comparison page, which is why the code disappears (owner, BPR-1083) | `makeCompositionPage` 1873-1897; `apply_modality_surface` 6281-6323 |
| Reply in the drawer, R | Writes the reply in place at the foot of the open thread ([[BPR-1290]]). In Discuss, with the drawer closed or on another tab, it falls back to the full page. ⌘↩ publishes; Esc cancels an empty reply | `begin_thread_reply` 6447; `conversation_cards_view.cpp:2286-2300`; `conversation_open_card.cpp:1032` |
| A draft card's Edit | Opens the full-page composer | `workbench_window.cpp:1572-1576` |
| Notes tab and side chat, Promote | Turns a private note or AI text into the draft on the selected line | `update_comment_target` (`set_promotion_target`) |
| Right-click in the code | The standard text menu and Add tour stop only. No comment item | `workbench_window.cpp:1340-1348` |
| Keys in the code | The panes are read-only `QPlainTextEdit`s with no key handling of their own (`diff_workbench.cpp:337`, 1428-1437). Return does nothing, and there is no keyboard caret. A line is selected by a click (2328-2345), by Previous or Next change (Alt+PgUp or Alt+PgDn, `workbench_window.cpp:899-910`), or by a thread | |
| Labels | The chip (⌘L), and the codes `:qu:` and so on at the start of the comment | `feedback_label_controls.cpp`; [delivery contract](../../ux/conventional-comments-feedback.md) |
| Suggestions | Nothing composes one today. [[BPR-1084]] (In Progress) owns fence preservation and native anchors | |
| Anchor | One line: `review_comment_draft::Anchor`. A new comment goes on the right (head) side only | `review_round_trip.hpp:60-75`; `core::supported_comment_target` |

### How writing starts today: a local project (Review Cycles)

| Entry | What it does | Where |
|---|---|---|
| Review bar **Comment**, **Suggest change** | Comment notes the selection or the caret's line, and the whole file when nothing is selected | `review_cycle_view.cpp:458-465` |
| Return and Shift+Return in the code | Return is Comment and Shift+Return is Suggest change. The owner chose this ([[BPR-1157]], native-keyboard v001, 2026-10-02) | `review_cycle_view.cpp:790-807`; `suggestion_editor.cpp:816-825` takes the key only while the view is read-only |
| Right-click in the code | Comment and Suggest change, showing their keys, above the standard menu | `source_menu`, `review_cycle_view.cpp:1761-1801` |
| Note composer | In the Notes tray at the right, under the notes list, away from the line. Save note on ⌘↩ (Ctrl+Enter); Esc, and Control-C on macOS, cancel | 616-645, 300-336, 704-706, `comment_here` 1618 |
| Suggest change | Makes the whole file editable in the main pane from the caret. Save suggestion records exact replacements | `begin_suggestion` 1698-1726 |
| Comparisons (This round and the others) | A separate `Comparison` view (539-544) with no menu item and no Return. Both gestures work only in Full source | `source_menu` returns nothing there (1765) |

### Return and Shift+Return today, and conflicts with the owner's idea

- **Local project.** The owner's idea is already the binding there (BPR-1157).
  The pull request side would adopt it, so both flows match.
- **Pull request code panes.** Nothing uses either key, so both are free.
- **No conflict where focus decides.** These keep their own Return: the drawer
  opens the focused thread (`conversation_cards_view.cpp:2272-2280`), the link
  map (`diff_link_map.cpp:667`) and Home's list (`workbench_window.cpp:9205`).
  Composers keep Return and Shift+Return as a new line, and the label
  completion takes Return or Tab (`feedback_label_controls.cpp:570-595`).
- **Flagged 1, a keyboard line cursor.** A pull request pane has no keyboard
  caret, so keyboard Return needs a selected line that Up and Down move, with
  Shift to extend. Today Up and Down scroll the pane.
- **Flagged 2, ⌘↩ means two things.** In a pull request it publishes, which
  is a GitHub mutation. In a local project it saves on this computer. The
  draft says which ("Only you see it until you publish" or "Stays on this
  computer"), and the button says Publish or Save note.
- **Flagged 3, a line that already has a thread.** Alt+C replies to the
  selected thread first (BPR-1133). The proposal makes Return always start a
  new comment and leaves replies to R. Alt+C keeps reply-first and opens the
  drawer.
- **Flagged 4, the base side.** A new pull request comment cannot go on a
  left-side line. Return there does nothing, and the status line and the
  disabled menu item give the reason.
- **Flagged 5, screen readers.** Return is not a standard command in
  read-only text, so the pane's accessible description names both keys, as
  Review Cycles already does (`review_cycle_view.cpp:1758`).

### The unified flow (proposed)

The pull request and the local project use the same gestures, placement and
keys. Only the words differ.

1. **Right-click** a line or a selection. A click outside the selection
   selects that line first. The menu is headed "Line N" or "Lines a–b" and
   offers:
   - **Add comment** ↩ and **Suggest change** ⇧↩;
   - **Open the thread on line N**, when the selection holds one;
   - Copy and Copy link;
   - then the flow's own item: Add tour stop, or Link selection to note.

   Shift+F10 and the Menu key open the same menu at the selection.
2. **The draft opens in the drawer** as a draft thread pinned at its first
   line, so the code stays visible. A bracket in the gutter spans its lines,
   and the card's edge and notch are blue: amber is where you read, blue is
   where you write.
   - The header holds the lines, Draft (or Suggested change), Maximize and
     Close.
   - Under it: the field, and the label chip (⌘L) with the three decoration
     toggles once a label is chosen. The codes work as today.
   - The foot: "Only you see it until you publish", Discard, and **Publish**
     ⌘↩.
   - Esc with nothing written drops the draft. Otherwise it is kept as a
     dashed draft card at its line, and focus goes back to the code on the
     same line.
   - Opening a kept draft makes it editable again, which absorbs today's Edit.
3. **Suggest change** fills the field with a ```` ```suggestion ```` block
   holding the selected lines exactly. The caret is at the end of the last
   line inside it, the field is monospaced and does not wrap, and "What it
   changes" shows the removed and added lines. A label goes on its own line
   above the block.
4. **Maximize**, ⌘⇧↩ (Ctrl+Shift+Enter), opens Discuss (Notes in a local
   project):
   - at the left, this file's conversations and drafts in line order;
   - at the right, the lines and three either side under the same bracket,
     then Write and Preview over the same draft at full width;
   - Publish, and **Back to the code** on the same key or Esc, which returns
     the draft to the drawer with its text and caret.

   Open threads have the same button, so a long thread can be read with room.
   The full-page composer stops being a place of its own (AC5, amended).
   Comment ▾, Alt+C and a draft card all open the drawer draft.
5. **A local project.** The words are Save note, "Stays on this computer",
   Unsaved note and Notes. The drawer's tab is Notes, and it holds the cycle's
   notes at their lines in place of the tray's list. This is the largest
   consequence of unifying the two flows.

### Shortcuts (proposed)

| Key | Where | Does |
|---|---|---|
| Return (↩) | The code pane has focus and a line is selected | Add comment on the selection |
| Shift+Return (⇧↩) | Same | Suggest change on it |
| Up, Down; with Shift | The code pane | Move the selected line; extend it (new in pull requests) |
| Shift+F10, Menu | The code pane | The right-click menu |
| ⌘↩ (Ctrl+Enter) | A draft | Publish, or Save note |
| ⌘⇧↩ (Ctrl+Shift+Enter) | A draft, or Discuss | Maximize, or Back to the code. Unbound in `src/adapters` today |
| ⌘L, and ":" at the start | A draft | The label menu and codes, as today |
| Esc | A draft, or Discuss | Back to the code. The draft is kept unless it is empty |
| Alt+C (⌥C) | The window | Unchanged reply-first command, now opening the drawer |

Focus rules:

- Return and Shift+Return act only while the code pane itself has keyboard
  focus and a line is selected. They never act in a text field, the drawer, a
  menu, Home or the link map.
- No other modifier is taken, and keypad Enter counts as Return, as Review
  Cycles already treats it.
- With no line selected they do nothing, and the status line says how to
  select one.

### Consequences for the build (not decided here)

- **Ranges in pull requests.** A draft anchors to one line. A comment or a
  suggestion over several lines needs GitHub's `start_line`/`line` pair
  through the core, the host and the publication identity. Until then a range
  would anchor to its last line, and a suggestion would replace only that
  line, so a range must be either refused or given a two-line anchor.
  Coordinate with [[BPR-1084]].
- **A keyboard line cursor** in pull request panes.
- **Review Cycles notes in the inline drawer**, and the whole-file
  suggestion editor kept or replaced (Q2).
- **A draft view in Discuss.** `composeInlineDraftPage` and its tests migrate.
  `composeInlineDraftAction` keeps its name.

### Prototype v1

[`BPR-1212-compose-v1`](../../design/BPR-1212-compose-v1/index.html) is
[`BPR-1290-inline-drawer-v2`](../../design/BPR-1290-inline-drawer-v2/index.html)
(the drawer and the finder are unchanged) with the flow above, light and dark,
and the drawer from 280 to 760 px. The status bar switches between a pull
request and a local project and has a Keys list. Content: the conversations
are made up and the code is verbatim. It is design evidence, not the product.

Exercised in headless Chromium, with no page errors:

- the right-click menu, and Shift+F10 with its Open the thread item;
- Return to a draft, `:qu` completion with Tab, and the chip and its toggles;
- ⌘⇧↩ to Discuss and back with the caret kept, then ⌘↩ to publish;
- Shift-click a range, then Shift+Return to a prefilled block with its
  "What it changes";
- Esc keeping a written draft and dropping an empty one;
- Return and Shift+Return inside a draft giving new lines;
- the local project's Save note;
- 280 px, and dark.

A stale scroll-bar tick left by a deleted draft, a latent bug in v2's Delete,
is fixed in this copy.

### Questions for the owner (in chat)

- **Q1 (Accept or Reject).** Return comments and Shift+Return suggests a
  change on the selected lines, in pull requests and local projects alike.
  They act only while the code has focus and a line is selected; Up and Down
  move the line, and Shift extends it. Put any change in the note.
- **Q2 (A or B).** Suggest change in a local project:
  - **A.** As in a pull request: a suggestion block in the drawer, which
    Save note records as exact replacements of those lines.
  - **B.** Today's whole-file editor in the main pane stays ([[BPR-1157]]).
    Only Add comment moves into the drawer.
- **Q3 (A or B).** Compose review (the whole-review summary, ⌥R):
  - **A.** It moves into Discuss beside the drafts, so no full-page composer
    is left.
  - **B.** It stays a full page until [[BPR-1294]] finds a home for comments
    that are not on a file.

Next: record the answers here and amend AC6 accordingly. Then prepare the
authored plan for AC2 and AC3, starting with the range anchor and the
keyboard line cursor.

## Scheduling

Activated 2026-10-06 (above). Before that: Backlog, explicitly for later
activation. No new predecessor status, remote mutation, installed-package or
release approval follows from this card. The design round neither builds
anything in `src/` nor claims a native or human result.

## Owner answers, 2026-10-06 (chat)

Asked by the coordinator on the BPR-1212 prototype
(`docs/design/BPR-1212-compose-v1/`):

- Q1, Return comments and Shift+Return suggests a change on the selected
  lines in both flows, only while the code pane has focus with a line
  selected; Up/Down move the line and Shift extends it: "Accept".
- Q2, Suggest change in a local project: "B: whole-file editor". The local
  project keeps today's whole-file editor in the main pane; only pull
  request review drafts a suggestion block in the drawer.
- Q3, the whole-review summary composer (⌥R): "A: move to Discuss". No
  full-page composer remains.

Build order: after BPR-1290 (owner-look pending) and BPR-1295. The first
build slice is the multi-line draft anchor and the keyboard line cursor.

## Build slice 1, claimed by bpr-main-linux (2026-10-07)

The coordinator assigned slice 1 to `bpr-main-linux`, with no visual redesign
yet. It covers three parts:
- **(a) Multi-line anchor:** a pull request draft anchors to a start line, an
  end line and a side, through core and application. Publication sends
  GitHub's multi-line review comment fields.
- **(b) Keyboard line cursor:** the pull request code panes get a line cursor
  that Up and Down move and Shift extends. It is visible and accessible.
- **(c) Return and Shift+Return:** they act only while the code pane has focus
  with a line selected. Return opens a draft in the existing composer;
  Shift+Return prefills a suggestion fence with the selected lines.

Local-project flows stay unchanged (Q2: B, the whole-file editor). The owner
look is left to the coordinator.

### Slice 1 implementation (bpr-main-linux, 2026-10-07)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-linux` (WSL2, GCC 15.2.0, Qt 6.11.2). main is frozen for a macOS
prerelease, so the work is on branch `bpr-1212-slice1`, rebased onto
`origin/main` at `fd6c21b36`; product commit `6f3bd33de`. Nothing is pushed.
The `frontend-design` skill is not installed on this station; the slice has
no visual redesign and reuses the panes' current-line band and text selection
and a plain editor label. [[BPR-1084]] (Done) is reused, not duplicated: its
`classify_review_body` recognises the prefilled block and its mark marks it.

**(a) Multi-line anchor.**
- Core: `review_comment_draft::StartLine` and
  `review_publication_proposal::StartLine` beside the anchor, which stays the
  last line (GitHub's `line`). `valid_comment_range`: a range is a new comment
  on the right whose first line precedes its anchor. A revisioned edit with
  `ChangeRange` sets the first line in the same revision as the body, refused
  otherwise as `review_draft_rejection::invalid_range`. Answering a thread
  drops the first line. An invalid range never begins publishing.
- `core::decide_comment_range` (new `review_comment_range.hpp`) decides a
  selection's two ends: one line is its own anchor, refused only on the left
  (`base_side`, Flagged 4) or past the document. Several lines must be on one
  side (`mixed_side`) and inside one hunk (`outside_hunk`, `crosses_hunk`),
  where `github_review_hunks` draws hunks as GitHub's diff does: the change
  groups with three lines of context, touching ones merged.
  `core::suggestion_prefill` makes the block (part c).
- Host: `review_mutation_application_host` refuses any other range before a
  request exists. The idempotency key gains a leading range prefix only for a
  range, so a single line's key is unchanged.
- GitHub adapter: `start_line` and `start_side` follow the single-line fields
  in the same sorted key order, written only for a range. A range on a reply
  or as a file comment is refused before the wire. GitHub's answer is the
  range's receipt only with its exact start line and side, and a single line's
  only without them. A range's body counts as needing a line subject, so a
  refusal is `incompatible_body` with the text kept, never a file comment with
  one line's link.
- Storage: the private draft record keeps a range as codec version 2 (the
  first line after the version 1 fields). Single-line drafts keep their
  version 1 bytes, and every earlier row reads as one line. No SQL schema
  change. The store refuses an invalid range on write and on read.
- Fixture server: it records `start_line` and `start_side` as received, and a
  range's thread reads back with `startLine`.

**(b) Keyboard line cursor** (`diff_workbench`).
- With a code pane focused, Up and Down move the selected line on its side,
  and Shift extends the pane's own text selection. The cursor is the moving
  end, and the semantic selection follows it. With no line on that pane, the
  first press selects the first line in view.
- The mouse and keyboard share one selection: a click, a drag or a Shift-click
  then Shift+Down extends what the mouse selected.
- The cursor keeps itself in view through the linked scroll, and a pane never
  scrolls on its own.
- Each move is announced politely, once ("Line 22 of the later version
  selected", "Lines 22 to 24 selected").
- Each pane's accessible description names Up, Down, Shift, Return and
  Shift+Return (Return on macOS, Enter elsewhere).
- One pane moves by rows.

**(c) Return and Shift+Return.**
- The pane claims them in `ShortcutOverride` only while it has focus. Keypad
  Enter is Return, and no other modifier is taken. Text fields, the drawer,
  menus, Home and the link map keep their own Return.
- The window asks `decide_comment_range`. A refusal opens nothing and the
  status line says why:
  - a left line gives Comment's reason;
  - lines on two sides, or outside one hunk, each get their own reason;
  - with no line, "Select a line first: click it, or move to it with Up and
    Down while the code has focus."
- Otherwise the window selects the range's last line, hands the range to the
  composition and opens the existing composer.
- The composition decides again from the accepted diff. Once the line's draft
  is there, it sends one ordinary revisioned edit (`ChangeRange`) through the
  durable store. For Shift+Return the edit adds a block of the right side's
  exact lines: without line endings, in a fence longer than any backtick run
  in them, as the whole body of an empty draft or once after existing text.
  The caret goes to the end of the block's last line, and the editor shows
  "Lines a to b".
- Alt+C keeps reply-first and reopens the line's draft as it is. The Review
  Cycles flows are untouched.

**Tests** (Linux, Debug `build/dev` and `build/core-only`, at `6f3bd33de`):
- `core_review_comment_range` (new) covers hunks on the real engine,
  every refusal, the prefill bytes, and the draft and proposal range.
- `github_mutation_contract` gains the range's exact bytes, the
  single-line key equal to an independent recomputation of the pre-range key,
  the refusals, and the response matching.
- `application_comment_range_draft_store` (new) runs the real SQLite
  store and draft owners: restart, v1/v2 bytes, refusal, proposal and key.
- `qt_diff_workbench_scale_100/125/200` gain the cursor, Shift, the mouse,
  view keeping, one pane, the announcements, the descriptions, keypad Enter
  and modifiers.
- `qt_workbench_line_draft` (new, `--line-draft`) checks the status-line
  reasons, the request and the composer, and the keys left to the composer
  and drawer.
- `application_line_draft_contract` (new, `--line-draft-course`) is the
  composed public path. With the focused right pane it drives Shift+Down
  twice and then Shift+Return, gets a draft on lines 9 to 11 with the block,
  and the fixture server receives `line` 11, `start_line` 9 and both sides
  `RIGHT`. `LINE_DRAFT_EVIDENCE:111111`.
- `application_label_authoring_contract` now ends with the same phases and
  passes (26 rows, see below).
- Also pass: `core_review_round_trip`, `core_revisioned_review_draft`,
  `core_suggestion_applicability`, `core_feedback_suggestion_preservation`,
  `core_feedback_draft_edit`, `github_recorded_fixture_contract`,
  `github_review_comment_permalink`, `application_feedback_draft_store`,
  `application_private_review_host`, `application_review_mutation`,
  `application_review_mutation_application_host`, `qt_review_draft_editor`,
  `qt_conversation_cards_view`, `qt_composed_github_scale_100`,
  `architecture_boundaries`, `architecture_boundary_contract`,
  `test_network_isolation_manifest`,
  `application_review_round_trip_composition_contract` and
  `application_composition_contract` (1,024 s). The core-only rows pass,
  with `core_dependency_boundary` and `core_boundary_contract`.
- `coding-format-check` (clang-format 22.1.8) and `DOCUMENTATION_OK` pass. A
  clang 22 `-fsyntax-only` pass over every changed unit is clean (Mac
  warnings).

**Failing-before controls** (each mutation applied, the row run, then
reverted, and the row green again):

| Mutation | Row | Failure |
|---|---|---|
| No `crosses_hunk` check | `core_review_comment_range` | "two hunks' lines are refused as crossing a hunk" |
| Fence always three backticks | `core_review_comment_range` | "a line holding backtick runs gets a longer fence…" |
| `ChangeRange` unvalidated | `core_review_comment_range` | "a first line that does not precede the anchor is refused…" |
| No `start_line` on the wire | `github_mutation_contract` | "lines 9 to 13 send start_line 9 and start_side RIGHT…" |
| Response start unchecked | `github_mutation_contract` | "an answer without the exact start line and side is not the range" |
| Key ignores the range | `github_mutation_contract` | "a range's key differs from its last line's…" |
| Codec always version 1 | `application_comment_range_draft_store` | "storage holds the range as version 2" |
| Shift does not extend | `qt_diff_workbench_scale_100` | "Shift+Down extends the selection; the cursor is its moving end" |
| Any modifier taken | `qt_diff_workbench_scale_100` | "no other modifier is taken" |
| Description without keys | `qt_diff_workbench_scale_100` | "each code pane's accessible description names Up, Down, Shift, Return and Shift+Return" |
| Base side not refused | `qt_workbench_line_draft` | "Return on a left line opens nothing and the status line gives the reason" |
| No suggestion prefill | `application_line_draft_contract` | the course waits at the prefill phase until the run budget expires (`LINE_DRAFT_EVIDENCE:000000`) |
| Range not set on the draft | `application_line_draft_contract` | `LINE_DRAFT_EVIDENCE:111000` |

**100 ms gates.** The optimized tests-on tree was built with
`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON -DCMAKE_TEST_BUILD_DEPENDS=ON`
(GCC 15 `-O3`, no warning in project code). With the machine idle, `-j1`
passes 10 of 10, and the tree was then deleted:
- `application_large_review_responsiveness` measures three new phases on
  `large/comparison.cpp`: `keyboard_line_down` 9.0 ms, `keyboard_line_extend`
  8.7 ms and `keyboard_line_reveal` (Down from a line out of view) 10.2 ms.
  Across all 31 phases there are no `InputBudgetFailures`, the largest pulse
  interval is 69.6 ms, and budgets are enforced.
- `application_first_tab_comparison`: no `InputBudgetFailures`, budgets
  enforced.
- `qt_workbench_window_scale_100/125/200`: 22.5, 23.7 and 27.2 s.
- `qt_workbench_line_draft`, `qt_diff_workbench_scale_100`,
  `core_review_comment_range`, `application_comment_range_draft_store` and
  `application_line_draft_contract` also pass there.

**Pin.** `application_review_round_trip_composition_contract` is resealed over
the BPR-1303 seal. After the rebase, `full_application` is `953799a2…` and
`production_app_manifest` is `915c9317…`. The `production_mutation_branch`
pin is unchanged (`e067befe…`). The dated "BPR-1212 slice 1 reseal" comment
sits above `bpr_lf_normalized_sha256`.

**Finding (already handled on main).** At the claim (`c3ab0094d`) the
label-authoring course stalled after its first publication: the next line got
no draft. A bisect found the cause at `7778886c9` (BPR-1293): the
published-comment reveal opens in Review, and after that refresh the
workbench kept no selected file. That is [[BPR-1303]], fixed on main at
`bba8a8d26`. After the rebase the full course passes with this slice's
phases.

**Limits.**
- The hunks come from the accepted comparison and diff options (whitespace
  options, a base other than GitHub's merge base). Where GitHub disagrees, its
  422 is `incompatible_body` with the text kept.
- Several lines outside every hunk are refused, even where a host allows
  comments outside a hunk.
- A range's uncertain publication is not matched by the read-only pending
  repair (`repair_pending_publications`). It stays pending, which is the safe
  direction; the in-request reconciliation does match it.
- GitHub's two-sided ranges (`start_side` differing from `side`) are not
  offered.
- Return always takes the selection, so Return on one line narrows a kept
  range draft on that line.
- The composed course does not publish a single line itself. The
  label-authoring phases before it do, and the contract test pins its bytes.
- Linux only: macOS and Windows runs are owed. The owner look is open for the
  coordinator.

### Coordinator decision (2026-10-07)

Return on a single line keeps narrowing an existing range draft that ends on
that line, because Return always uses the current selection. Alt+C reopens a
draft unchanged. The coordinator decided this on the implementer's question.

### Slice 1 integrated onto slice 2 (bpr-main-linux, 2026-10-08)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-linux` (WSL2, GCC 15.2.0, Qt 6.11.2). Slice 1 was rebased, not
merged, onto `origin/main` (slice 2 at `a90419791`, then main at `e9f461835`,
`5efe30739` and later), and integrated so that slice 2's drawer flow is the
one path.
The composer parts of the implementation notes above ("opens the existing
composer", the editor's "Lines a to b" label, `core::suggestion_prefill`) are
superseded by this section; the anchor, publication, storage and keyboard
cursor are unchanged.

**One path.** Return and Shift+Return in a focused code pane, the code's
right-click Add comment and Suggest change, and the gutter's "+" all go to
`workbench_window::comment_on_selected_lines`. It keeps slice 1's decision
(`core::decide_comment_range`, its typed refusals and status-line reasons),
selects the range's last line, has the draft there take the range's first
line, and calls slice 2's `begin_new_comment`, which opens the draft in the
drawer. The focus gating stays slice 1's: the pane claims the keys in
`ShortcutOverride` only while it has focus, with keypad Enter as Return and
no modifier but Shift. Slice 2's window-level Return filter on the panes is
removed, as are the composition's `begin_line_draft` and
`complete_line_draft`, the window's line-draft dispatch and request type, and
the editor's `RangeTarget` label and `place_body_caret`. Comment (Alt+C)
still calls `begin_new_comment` directly, so it reopens a draft unchanged;
Return on one line narrows a range draft ending there (the coordinator's
decision above).

**The range on the draft.** `review_draft_editor::change_range` sends the
first line (`ChangeRange`, `StartLine`) with the body in one ordinary
revisioned edit through the editor's own edit path, the path the drawer and
Discuss already write through. An edit already with the draft owner is
answered first, and the range goes with the next one. If the line's draft is
not there yet (the range's last line was only now selected), the window waits
for it and drops the request when another line is selected. Nothing reaches
the composition but that edit; publication still takes `StartLine` from the
draft as `start_line`/`start_side`, and a single line's bytes and idempotency
key are unchanged.

**Ranges in slice 2's surfaces.** `draft_source_lines` reads the draft's first
line to its anchor, so Suggest change fills the block with all the selected
lines and "What it changes" compares against them. The drawer header and its
accessible name say "Lines a–b", the gutter's blue bracket spans the lines
(`diff_line_mark::FirstLine`, also mapped to rows in one pane), Discuss's
excerpt brackets them, and the right-click menu is headed "Lines a–b" and
keeps the selection when the click is inside it. The menu's items are
unavailable with the core's reason when the lines cannot take one comment.

**One fence builder.** `core::suggestion_prefill` is deleted. Its behaviour
is now `core::suggestion_source_lines` (exact bytes without line endings; a
range past the document, a reversed range or a line that is not UTF-8 gives
none) and `core::suggestion_fence_body`, whose fence now outruns a backtick
run anywhere in the lines, as both slices' notes describe, not only at a
line's start. The caret stays at the end of the block's last line. Slice 1's
prefill cases moved to `core_suggestion_draft` as `range_block`.

Local-project flows are unchanged. The workbench specification and the
conventional-comments delivery contract describe the integrated flow.

**Tests** (Linux, Debug `build/dev` and `build/core-only`, offscreen with no
display, on the integrated tree over `e9f461835`; the focused rows again over
`5efe30739`, below):
- `core_suggestion_draft` gains `range_block`, slice 1's prefill cases: the
  exact bytes, tabs and trailing blanks, an empty line, a backtick run in
  mid-line outrun by the fence, a last line without a newline, refusal past
  the document, reversed or not UTF-8, the caret's place, and an unedited
  range changing nothing. `core_review_comment_range` keeps the hunks, the
  refusals and the draft and proposal range.
- `qt_workbench_line_draft` is rewritten for the one path with a revisioned
  editor and the core applying each edit: a left line and lines outside one
  hunk refused with their status-line reasons and no range edit; Shift+Down
  twice and Shift+Return give the draft on line 6 the first line 4 in one
  edit, the block of lines 4 to 6 exactly, the drawer draft "Lines 4–6" with
  the caret at the end of the block's last line and the bracket across the
  three lines; Esc keeps it; Comment reopens it with no edit; Down, Up and
  Return narrow it to line 6 with its text kept; keypad Enter on a new line
  sends no range; Shift+Return in the notes starts nothing; no draft on the
  line, and no line at all, each say why.
- `application_line_draft_contract` (`--line-draft-course`, the composed
  public path) drives the same keys into the drawer: the left line refused,
  one line opened as "Your draft on line N" with no first line and left with
  Esc, Shift+Down twice and Shift+Return to the drawer draft on three lines
  with the bracket, the caret and the native suggestion mark, and the drawer
  draft's own Publish. The fixture server receives `line` 11, `start_line` 9
  and both sides `RIGHT`. `LINE_DRAFT_EVIDENCE:111111`.
- Pass: `core_review_comment_range`, `core_suggestion_draft`,
  `application_comment_range_draft_store`, `qt_review_draft_editor`,
  `qt_workbench_line_draft`, `application_line_draft_contract`,
  `github_mutation_contract`, `qt_diff_workbench_scale_100/125/200`,
  `qt_conversation_cards_view`, `qt_workbench_window_scale_100/125/200`
  (24.1, 27.8 and 55.4 s; slice 2's compose-in-the-drawer test included),
  `application_review_submission_composition_public_path` and its 125 and 200
  scales, `application_label_authoring_contract`,
  `application_review_round_trip_composition_contract`,
  `application_composition_contract` (909 s), `architecture_boundaries`,
  `test_network_isolation_manifest` and `github_recorded_fixture_contract`.
  All 252 core-only rows pass.
- `coding-format-check` (clang-format 22.1.8) and `DOCUMENTATION_OK` pass.
  The Debug build has no warnings.
- After the rebase onto `5efe30739` (BPR-1328's keys, BPR-1329's drawer
  Publish status, BPR-1335's gutter), the 22 focused rows above pass again,
  with `application_composition_contract` (970 s). Each later rebase onto a
  moving `main` (BPR-1337's Discuss header, BPR-1339's Overview; no
  pinned-scope change, the pins unchanged) passes the 22 again before the
  push.
- Finding on `main`, not this card's: `test_inline_publish_cli_consent`
  (BPR-1329, `512c2836b`) does not compile with GCC 15:
  `tests/end_to_end/inline_publish_cli_consent_tests.cpp:282` binds a
  range-for's structured binding by value, which `-Werror=range-loop-construct`
  refuses (the tests/AGENTS.md rule). Left to its owner; every other target
  builds.

**Failing-before controls** on the integrated tree (each mutation applied,
the build and the row run, then the file restored byte for byte, rebuilt and
the row green again):

| Mutation | Row | Failure |
|---|---|---|
| The range not set on the draft | `qt_workbench_line_draft`; `application_line_draft_contract` | "Shift+Return gives the draft on line 6 the first line 4 in one edit…"; the course waits at the prefill until its budget expires (`LINE_DRAFT_EVIDENCE:000000`) |
| `draft_source_lines` takes only the anchor | the same two rows | the same: the block holds one line, not three |
| The fence counts only leading backtick runs (slice 2's earlier rule) | `core_suggestion_draft` | "a line holding backtick runs gets a longer fence…" |
| The fence always three backticks | `core_suggestion_draft` | the same |
| A single line sends `start_line` | `github_mutation_contract` | "comment request has exact ordinary GitHub fields and bytes" |
| Any modifier taken | `qt_diff_workbench_scale_100` | "no other modifier is taken" |
| The keys taken without the pane's focus (an application-wide filter) | `qt_workbench_line_draft` | "Shift+Return in the notes starts no draft from the code" |
| Lines outside one hunk not refused | `qt_workbench_line_draft` | "Shift+Return on lines outside a hunk opens nothing and says why" |

A base-side mutation in the window's decision alone stays green: the comment
target's own check refuses the left side with the same reason, so the left
line is refused twice over. `core_review_comment_range` covers the core's
`base_side` refusal.

**100 ms gates.** The optimized tests-on tree was built once on the
integrated tree with `cmake --preset app-release -B build/app-release-tests
-DBUILD_TESTING=ON -DCMAKE_TEST_BUILD_DEPENDS=ON` (GCC 15 `-O3`; the only
warning is vendored SQLite's). With the machine idle, `-j1` passes 10 of 10,
and the tree was then deleted:
- `application_large_review_responsiveness`: `keyboard_line_down` 9.6 ms,
  `keyboard_line_extend` 8.7 ms and `keyboard_line_reveal` 13.4 ms on
  `large/comparison.cpp`; no `InputBudgetFailures`, budgets enforced, the
  largest pulse interval 63.4 ms.
- `application_first_tab_comparison`: no `InputBudgetFailures`, budgets
  enforced.
- `qt_workbench_window_scale_100/125/200`: 23.1, 24.8 and 28.7 s.
- `qt_workbench_line_draft`, `qt_diff_workbench_scale_100`,
  `core_suggestion_draft`, `core_review_comment_range` and
  `application_line_draft_contract` also pass there.

**Pin.** `origin/main`'s pins matched at its head `e9f461835`, and again at
`5efe30739` after the last rebase (detached worktrees,
`BPR_PRINT_PINNED_DIGESTS`). Resealed for the integrated result over
`5efe30739`: `full_application` `cf8290ad…` and `production_app_manifest`
`2cdb82c6…`.
`production_mutation_branch` is unchanged (`e067befe…`). The dated "BPR-1212
slice 1 reseal" comment, now describing the integration, follows slice 2's,
BPR-1328's and BPR-1329's reseal comments.

**Limits.**
- Linux only; macOS and Windows runs of these rows are owed by the stations.
- The integrated drawer flow on several lines has had no owner look. Slice
  2's session `aae8f3fc` looks at its frozen one-line build, not at this
  integration; the owner look stays with the coordinator.
- If a range's last line was only now selected and its draft never arrives,
  the request waits silently until another line is selected; if the line was
  already selected, the status line says why there is no draft and the draft
  still opens should it arrive.
- Slice 1's remaining limits above (hunks from the accepted comparison, no
  two-sided ranges, the read-only pending repair) are unchanged.

## Build slice 2: compose in the drawer (bpr-main-mac, 2026-10-08)

Drawn with `frontend-design` (loaded first, as the repository asks). Built
on `origin/main` because slice 1 (`bpr-1212-slice1`, `bpr-main-linux`) was not
on `origin` when this slice started or when it was pushed; see "Coordination
with slice 1" below.

### Owner notes this slice answers (chat, 2026-10-08, verbatim)

> On the latest pre release build adding a comment on a line without one opens
> only the full screen composer instead of the new drawer comment ux

> I don't know how to publish my draft comment, compose review only lets me
> approve with an extra comment but I don't want to do that

> Perhaps what were missing is the equivalent of the GitHub start a review flow
> or if we have it I can't find it

### What was built

- **A new comment is a draft thread in the drawer** at its line: Return on the
  selected line (keypad Enter too), Add comment on the code's right-click
  menu, Comment (Option-C) on a line no thread is on, and a "+" the head
  gutter shows under the pointer. The card's edge and notch are blue and a
  blue bracket marks the line in the gutter. Header: the line, Draft (or
  Suggested change), Maximize, Close. Field with the label chip and its
  `:xx:` codes (the reply's controls). Foot: "Only you see it until you
  publish", Discard and **Publish** (⌘↩), whose tooltip and accessible
  description say it posts this one comment now. Return in the field is a new
  line. Esc keeps a written draft as its dashed note and drops an empty one,
  and focus goes back to the code; opening the note writes it again (Edit is
  gone). The code stays in view throughout.
- **Right-click** selects the clicked line first when it is outside the
  selection. The menu is headed "Line N" and offers Add comment ↩ and Suggest
  change ⇧↩, then Copy and the standard items, then Add tour stop. On the base
  side both are disabled and say why. The keys shown are the menu items' own:
  no window action claims Return (the Review Cycles row checks this).
- **Suggest change** fills an empty draft with a ```` ```suggestion ```` block
  holding the line exactly as the head document holds it (tabs and backticks
  kept, the fence longer than any backtick run in it), with the caret at the
  end of the block's last line; a draft with words keeps them and gains the
  block below. The field turns monospaced and unwrapped, and "What it
  changes" shows the removed and added lines as they are edited. The text
  decisions are core: `bpr/core/suggestion_draft.hpp`.
- **Maximize** (⌘⇧↩, the header button) opens the same draft in Discuss,
  which is now the home of the full composer: the conversations narrow to a
  list at the left; at the right Back to the code, the draft's line with three
  either side under the same bracket, and the full editor. ⌘⇧↩, Esc or Back
  to the code return it to the drawer with its text and caret. A conflicted
  draft (Reload/Reapply) and a comment-workshop proposal open there directly.
  The full-page comment window is no longer a destination of its own.
- **Compose review (⌥R) opens in Discuss** (Q3: A), with Esc and Back
  returning to where it was opened.
- **Local-project flows are unchanged** (Q2: B).
- Boundaries: Qt presentation plus one pure core header. The draft is still
  the draft editor's; the drawer and Discuss edit it through its one edit
  path, and nothing publishes but its Publish. No application, provider,
  storage or publication change.

### Publishing, Compose review and "start a review": findings

- Compose review's Comment needs a summary today because the review carries
  no line comments: `review_submission` posts only `body`, `commit_id` and
  `event` (`github_mutation_contract.cpp`), and line drafts publish one at a
  time from their own Publish. GitHub refuses that shape without text. Probed
  live on the owner's test repository, `MikeMarcin/BPR-Test#41` (opened,
  probed, closed, branch deleted): `COMMENT` with an empty body and no
  comments is 422; with one line comment and an empty body it is
  `COMMENTED`. The core rule (`validate_body`, nonblank for Comment and
  Request changes) is right for what BPR sends.
- How pending drafts relate to a review today: BPR-1085's preflight counts my
  selected unsent draft ("Yours, not sent: 1 blocking") and refuses Submit
  when that selection changed after the reviewer chose it, but the submission
  does not carry it.
- This slice makes publishing a single draft obvious (Publish on the drawer
  draft, tooltip "Post this one comment to the pull request now ⌘↩") and makes
  Compose review say why Comment needs a summary and that line comments are
  published from their drafts.
- **Start a review is not built.** It needs a pending mark kept with each
  draft on this computer, every pending draft shown at its line (today only
  the selected line's draft is drawn), and the review submission carrying the
  pending drafts as GitHub review comments, marking each published only when
  the review is accepted. That crosses core, the draft store, the GitHub
  adapter and publication identity, so it is a separate milestone, not part of
  this presentation slice. Its direction is drawn for the owner as
  [`BPR-1212-start-a-review-v1`](../../design/BPR-1212-start-a-review-v1/index.html)
  (frontend-design: GitHub's Add single comment / Start a review, a review-bar
  "Review · N pending" chip with Finish review, and Finish review in Discuss
  where Comment needs no summary) and asked as `start-a-review-v001`.

### Verification (macOS, Debug, offscreen)

- New: `core_suggestion_draft`; the conversation-reveal course's
  `test_compose_in_the_drawer` (Return, keypad Enter, Esc keep/drop, reopen,
  Maximize and back with text and caret, right-click selection and items,
  Suggest change prefill and "What it changes", ⌘↩ publishes once, the gutter
  "+", base-side refusal; negative controls: Return on the base side and in
  the notes open no draft). The drawer draft opens in 3 to 18 ms here
  (100 ms target).
- Failing before: the BPR-1133 Comment course asserted Comment opened the
  full page (`stack->currentWidget() == page`); it now asserts the drawer
  draft with the code visible and no Discuss page, and the opposite fails.
- Migrated: `qt_conversation_cards_view` (the open draft is its composer),
  `qt_workbench_window_scale_{100,125,200}`, the review-submission public
  paths (Comment and promotion open the drawer draft), and the app courses for
  threaded reply and label authoring.
- Passing on the candidate: the 34 focused rows (cards view, workbench window
  and conversation reveal, composed GitHub at three scales, review-submission
  public paths at three scales, review cycle desktop, agent-finding and
  round-trip composition contracts, review editors, focus order, overview).
- Round-trip pin resealed with a note (full application and production
  manifest digests; mutation pin unchanged).
- Pre-existing failures on `origin/main`, not touched here:
  `qt_composed_inline_drawer_scale_*` ("each post's body fits its width",
  then exit -11; same at `2fe84cd26` without this slice) and
  `qt_workbench_home_discovery_scale_*` (both named in the 0.1.5 test-gate
  exception), and `application_profile_window_layout_public_path` (its seed
  crashes clicking `overviewReadDescription` in Overview).
- Linux and Windows runs of these rows are owed by the stations.

### Coordination with slice 1

Slice 1 adds the multi-line anchor, the keyboard line cursor and its own
Return/Shift+Return. When it lands, rebase this slice onto it: route its
Return/Shift+Return to `begin_new_comment`, take the range from the draft's
anchor for the bracket, the "Lines a–b" words and the suggestion's lines
(`draft_source_lines`), and keep one fence builder (`core::suggestion_fence_body`).

Done on 2026-10-08 the other way round, slice 1 rebased onto this slice: see
"Slice 1 integrated onto slice 2" above.

### Owner look, session v001 (handed back; verdicts below)

One new short session, not the long default one. Five questions, each Accept
or Reject with the change in the note; none is self-certified.

- Session `aae8f3fcd307cfca3bfb8ae0b28f1b83`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1212-compose-v001`
  (repository root `/Users/jaeger/Documents/GitHub/BetterPullReview`), all five
  candidates ready at event cursor 15. The review host runs detached.
- Candidates, revision `frozen-80bea4636-v001`: `draft-in-drawer-v001`,
  `right-click-v001`, `maximize-to-discuss-v001`, `publish-draft-v001`; and
  `start-a-review-v001` (revision `design-80bea4636-v1`, the design frames).
- Frozen root `/Users/jaeger/bpr1212-compose-look-v001.T6ZCxT`: the
  `git archive` of `80bea4636` built there with the dev preset (tests on),
  app target only; executable SHA-256 `763b42b37854…`. Offscreen on it,
  `--github-fixture inline-drawer-v1 --attended-open-file
  src/ui/conversation_cards_view.cpp` printed `ATTENDED_FILE_SHOWN`.
- `80bea4636` landed on `main` as `a90419791`, rebased over BPR-1331
  (`88957a791`): only `workbench_window.cpp`'s anonymous namespace merged,
  each side's helper kept. The frozen build is the reviewed candidate.
- Launch: `python3 /Users/jaeger/bpr1212-compose-look-v001.T6ZCxT/launch.py`
  (the 1557-line file in Review); `--frames` opens the start-a-review frames.
  It checks the build, its fingerprint and the fixture tree first and reports
  an early exit.

Next: read the session's decisions. Accepts on the four built candidates
answer AC5 and AC6 for slice 2; a Reject comes back as a v002 candidate. An
Accept on `start-a-review-v001` starts its own build milestone.

## Owner look v001: verdicts (session aae8f3fc, handed back 2026-10-09)

Session `aae8f3fcd307cfca3bfb8ae0b28f1b83`, handed back at event 21
(2026-10-09T02:15:09Z) with nothing pending; its host is stopped. The
candidates ran on the frozen build of `80bea4636` (revision
`frozen-80bea4636-v001`: slice 2 only, without slice 1's ranges).

| Candidate | Event | Decision | Verdict |
|---|---|---|---|
| `draft-in-drawer-v001` | 16 | `d08bdb82-8f57-44b6-9269-b2b2f08e62d0` | Accept, no note |
| `maximize-to-discuss-v001` | 17 | `6021e631-e8e9-429f-ae7d-f86c45ea5fcc` | Accept, with a note |
| `publish-draft-v001` | 18 | `051a8af2-ce5c-46c4-ba7c-b677391dbbc8` | Accept, with a note and a screenshot |
| `right-click-v001` | 19 | `038cffa0-f481-4a04-9de5-f943dfbf4d13` | **Reject**, with a note and a screenshot |
| `start-a-review-v001` | 20 | `dc7e0a01-68f0-42b2-8d34-ede92665e96c` | Accept, no note |

The notes, verbatim:

- `maximize-to-discuss-v001`:
  > the restore (back to code) should still be in the top right
- `publish-draft-v001` (the screenshot shows the drawer draft's foot, its
  note cut off after "until yo…"):
  > just noticed this "only you see it unti..." doesnt fit.
- `right-click-v001` (the screenshot shows lines 585 to 598 selected, drawn
  white on solid blue):
  > are we meant to lose all syntax highlighting when we select something?
  >
  > it's awkward I can only add comment on the right pane, I have to single select a line with left click first and then right click otherwise the option is greyed out, I cant seem to do a suggest change on a multi-select. I cant just right click a line and add comment. It's just unintuitive.

The fingerprints are in the session file,
`/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1212-compose-v001/session.json`.

What follows from them (coordinator, 2026-10-08):

- `draft-in-drawer-v001` answers AC5's drawer half for one line.
- `maximize-to-discuss-v001` is accepted with a change: in Discuss the
  restore takes the top-right place Maximize had in the drawer.
- `publish-draft-v001` is accepted with a change: the privacy note must not
  be cut off (shorter, or a tooltip on an icon).
- `right-click-v001` comes back as `right-click-v002`, fixing all of it:
  (a) a selection keeps the syntax colours, with a translucent selection that
  does not replace the foreground; (b) a right-click on any line, with no left
  click first, selects it and offers Add comment, never greyed out on a valid
  line; (c) Suggest change works on several lines; (d) base (left) lines take
  Add comment, published as GitHub's `side: LEFT`, and Suggest change only the
  head side, with the reason in the menu when it is not available; (e) a
  right-click inside the selection keeps it.
- `start-a-review-v001` starts its own build milestone, [[BPR-1345]] (Ready,
  filed on `main` at `95369f486`), built after this slice.

## v002: right-click, base lines, the restore and the privacy note (bpr-main-mac, 2026-10-08)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-mac` (macOS, Apple clang, Qt 6.11.2, Debug and an optimized
tests-on tree, offscreen). Drawn with `frontend-design` (loaded first). Built
on `origin/main`, rebased onto `b864e319d` and then `2f960c927`; slice 1 and
slice 2 are both in.

### What changed

- **(a) A selection keeps the syntax colours.** Qt draws a text selection in
  the palette's HighlightedText, which replaced every selected glyph's colour
  (the owner's screenshot: white on solid blue). The code panes now paint
  their own text (`source_pane::paint_text`, public and protected
  `QPlainTextEdit` members only) and draw the selection as a translucent wash
  of the accent (the selected lines' wash alpha) with no foreground, laid
  among the extra selections after the bands and the selected lines' wash and
  before the intraline emphasis. Each glyph is drawn in its own colour over
  it. The selection itself, and Copy, are unchanged.
- **(b) A right-click on any line offers Add comment.** v001 greyed the item
  out whenever the line's draft had not arrived yet, and a right-click on a
  new line selects it and asks the host for that draft at the same moment, so
  the item was greyed exactly when the owner right-clicked a line nobody had
  clicked. The menu now judges only whether the line can take a comment; a
  draft on its way is no reason. Choosing the item waits for it: the status
  line says "Opening a draft on line N…", the draft opens in the drawer when
  it arrives, and if none comes within 1.5 s the status line says why.
- **(c) Suggest change on several lines** (slice 1's ranges, now in the
  reviewed build): a drag, Shift-click or Shift+arrows, then Suggest change,
  starts one suggestion block of all the lines on the draft that spans them.
- **(d) Base lines take comments.** GitHub takes a new comment on a base line
  as `side: LEFT` (BPR-1335 checked it live on `MikeMarcin/BPR-Test` #42), but
  counts those lines in the merge base's version of the file, while BPR's
  base pane shows the base branch's tip. A line read off the base pane names
  other text on GitHub whenever the file changed on the base branch after the
  pull request branched, so base lines are admitted only with evidence:
  - The epoch host derives the unique merge base of GitHub's base OID and the
    accepted head from the retained objects, never fetching, and compares the
    file's blob there with the comparison's left blob
    (`resolve_github_base_numbering`, on the review's cache actor). Same, or
    the merge base is the left commit: the base side is numbered as GitHub's.
    Another blob, no file there, several or no merge bases, or a sparse review
    without the history: it is not, and base lines say why.
  - That one answer sets the window's left endpoint (existing base-side
    threads are now placed on the base pane), the composition's draft on a
    base line, the core's decision (`decide_comment_range(..., true)`: a base
    line must be in a hunk, since it has no file-comment fallback, and a
    range on the base side keeps both ends there), and the mutation host's
    admission (`BaseNumberedAsGitHub` on the source material; the root is bound
    on the head's path and commit with the base document's line count).
  - The GitHub adapter writes `side: LEFT` and `start_side: LEFT`, validates a
    base line against the source without a permalink, and never falls back to
    a file comment (its body counts as needing a line subject). The answer is
    the comment's receipt only on the left.
  - The recovery store keeps the side: a publication record is version 1 while
    every child is on the right (bytes unchanged) and version 2, with each
    child's side, once one is on the left. The side-less legacy table refuses a
    base-side anchor.
  - Suggest change stays on the head side: on a base line it is unavailable in
    the menu with its reason, and Shift+Return says why.
  - The menu says "Base line N"; the drawer draft's header "Base line N"
    (`cb16bfeed`; its placeholder "Comment on base line N") and its name "Your
    draft on base line N"; leaving it returns focus to the base pane.
  - The gutter's "+" stays on the head pane (BPR-1335).
- **(e) A right-click inside the selection keeps it**, with its selected line.
- **The reason in words.** An unavailable item is explained in a row under the
  items ("This file's base is not GitHub's", "Base lines need a changed
  region", "Suggest change: head side only", "Lines must be in one changed
  region"), with the full sentence as its tooltip.
- **The restore at the top right** (maximize-to-discuss-v001's note). In
  Discuss, Back to the code moves from the top left to the top right, its
  arrows at the far right where the drawer draft's Maximize sits; the draft's
  lines title moves to the left. Esc and Command+Shift+Return are unchanged.
- **The privacy note never cut off** (publish-draft-v001's note). The drawer
  draft's foot says "Private until you publish" beside its lock, shown whole
  or not at all: where the drawer is too narrow, the lock alone, with "Only
  you see this draft until you publish it." as its tooltip and accessible
  description.

Boundaries: core decides the comment range and target shape; the epoch host
answers the base numbering on the cache actor; the composition carries the
answer to the window, the source material and the draft; Qt presents. No new
GitHub read; the only GitHub write is the existing Publish.

### Overlap with BPR-1335

BPR-1335 v002 (`b864e319d`) answered the owner's "why can't I add a comment
from the left pane" with the same diagnosis, proposed this follow-up for the
owner to accept (`BPR-1335:base-side`), and kept a local WIP branch,
`bpr-1335-base-side-wip` (not pushed). This slice builds that follow-up
because the coordinator asked for it on the right-click Reject; the evidence
is wider than the proposed ancestor check (the file's blob at the merge base,
so a base branch that moved on in other files still qualifies). The WIP
branch was not read or reused. Not done here: a fixture whose base moved on
in the same file (the `differs` answer is covered by the epoch host's own
test), and a live round on BPR-Test.

### Verification (macOS, Debug, offscreen)

New and changed rows, all passing on the candidate (`61dae6211` on
`b864e319d`, and again after the rebase below):

- `core_review_comment_range`: a base line or range is `base_side` without the
  evidence; with it, one base line in a hunk anchors on the left, a base range
  keeps its first line, a base line outside every hunk is `outside_hunk`, two
  hunks are `crosses_hunk`, past the base document `outside_document`, and the
  evidence changes nothing on the right; a new base-side draft takes a range.
  `core_review_round_trip` and `core_review_composition` (a base line is a
  root draft target; no third side is) follow the shape rule.
- `application_epoch_host`: `resolve_github_base_numbering` answers `same`
  for the same blob at the merge base and for a merge base that is the left
  commit (no lookup), `differs` for another blob and for no file there,
  `unavailable` for several, none or missing merge bases, and refuses another
  head, another snapshot, a missing path and a closing host before admission.
- `application_review_mutation_application_host`: a base-line root is
  unsupported without `BaseNumberedAsGitHub` and before any effect; with it a
  base range reaches the provider on the left, on the head's path, with its
  first line and the base document's line count; a base line past the base
  document is refused.
- `github_mutation_contract`: base lines 9 to 13 send `side: LEFT` and
  `start_side: LEFT` in sorted key order on the head's path and commit, the
  key differs from the head-side one's, a base line has no file-comment form,
  and GitHub's answer is the receipt only on the left.
- `sqlite_review_mutation_recovery_storage`: a head-side publication record
  keeps version 1, a base-side one is version 2 and reads back on the left
  after a restart, and the side-less legacy table refuses a base-side anchor.
- `qt_diff_workbench_scale_100/125/200`: a selection over a comment line is a
  wash lighter than the solid accent, the selected line's darkest ink stays
  the comment colour and not HighlightedText, and the selection is unchanged.
- `qt_workbench_line_draft` (real right-button press, context-menu request
  and release; the host answers a selection a turn later): a right-click on
  line 21, which nobody clicked, selects it and offers Add comment and Suggest
  change enabled before its draft arrives; Add comment opens the draft in the
  drawer once it does; a mouse drag over lines 4 to 6 and a right-click inside
  keep the selection and its selected line, and Suggest change starts the
  three-line block on the draft spanning them; a base line without the
  evidence is refused in the menu with "This file's base is not GitHub's";
  with it, "Base line 5" offers Add comment, Suggest change is unavailable
  with "Suggest change: head side only", and Add comment opens "Your draft on
  base line 5"; leaving it returns focus to the base pane; Shift+Return on a
  base line says why, Return opens it; a base line outside every hunk says
  "Base lines need a changed region"; a line whose draft never comes says it
  is opening one and then why none came. Discuss's Back to the code sits at
  the page's top right, its arrows at the far right, after the title, and
  brings the draft back; the privacy note's tooltip and description carry the
  sentence, and its words are painted only when they fit whole.
- Migrated: `qt_workbench_conversation_reveal` (a base line outside every
  hunk), `qt_workbench_window_scale_100/125/200` (the left line's reason,
  then its thread's Reply with the evidence), `qt_review_draft_editor` (a
  base-line root offers Publish; a line on no side keeps its text and says
  why), `application_review_submission_composition_public_path` and its 125
  and 200 scales (the left deletion takes its draft once the base numbering is
  GitHub's; without it, refused with the reason, and forcing Comment opens
  nothing).
- `application_line_draft_contract` (`--line-draft-course`, the composed
  public path): after the head-side phases, the epoch host answers the
  fixture's base numbering (`left-current=1 numbered=1`), Return on base line
  3 opens "Your draft on base line 3", and its Publish sends `side: LEFT`,
  `line` 3 and no range to the fixture server.
  `LINE_DRAFT_EVIDENCE:111111`, `BASE_LINE_DRAFT_EVIDENCE:111`.
- Also pass: every `core_*`, `sqlite_*` and `github_*` row (137 with the Qt
  rows below), `qt_conversation_cards_view`, `qt_composed_github_scale_100`,
  `application_label_authoring_contract`, `application_threaded_reply_contract`,
  `application_comment_range_draft_store`, `application_review_round_trip_composition_contract`
  (resealed), `github_recorded_fixture_contract`, `architecture_boundaries`,
  `architecture_boundary_contract` and `test_network_isolation_manifest`.
- `coding-format-check` (clang-format 22.1.8, BPR-1352's gate) and
  `DOCUMENTATION_OK` pass. clang-tidy 23 on the changed translation units
  reports nothing on changed lines; its findings there (identifier naming in
  lambda parameters, optional conversions) are on lines this slice does not
  touch.

**Failing-before controls** (each mutation applied, the row built and run,
the file restored byte for byte):

| Mutation | Row | Failure |
|---|---|---|
| Qt's own text paint | `qt_diff_workbench_scale_100` | "the selection shows as a wash, lighter than the solid accent" |
| The wash keeps a HighlightedText foreground | `qt_diff_workbench_scale_100` | "selected, the comment line keeps its comment colour, not the highlighted-text colour" |
| The menu requires the draft (v001) | `qt_workbench_line_draft` | "before its draft arrives the menu offers Add comment and Suggest change, enabled, with no reason" |
| No wait for an arriving draft | `qt_workbench_line_draft` | "when no draft comes the status line says why" |
| A right-click always reselects | `qt_workbench_line_draft` | "a right-click inside a dragged selection keeps it and its selected line, …" |
| The range dropped from the waiting comment | `qt_workbench_line_draft` | "Shift+Return gives the draft on line 6 the first line 4 in one edit, …" |
| Base evidence ignored in the window's decision | `qt_workbench_line_draft` | "with the evidence a base line offers Add comment, and Suggest change is unavailable with its reason shown" |
| Suggest change allowed on base lines | `qt_workbench_line_draft` | the same |
| The base comment target refused (v001 rule) | `qt_workbench_line_draft` | the same |
| No reason row | `qt_workbench_line_draft` | "without the evidence a base line's menu says in words why it takes no comment" |
| Focus always to the head pane | `qt_workbench_line_draft` | "leaving a base-side draft returns focus to the base pane" |
| Restore at the left (v001 order) | `qt_workbench_line_draft` | "Back to the code sits at the page's top right, …" |
| Privacy note as in v001 | `qt_workbench_line_draft` | "the draft's privacy note is short, and its tooltip and description carry the sentence" |
| A base draft headed "Line N" | `qt_workbench_line_draft` | "Add comment on a base line opens its draft on the left side in the drawer, headed "Base line 5"" |
| Core ignores the evidence | `core_review_comment_range` | "one left line is refused without evidence that the base is numbered as GitHub's" |
| Core lets one base line off the hunks | `core_review_comment_range` | "one left line outside every hunk is refused: a base line has no file-comment fallback" |
| Host admits a base root without evidence | `application_review_mutation_application_host` | "left, absent, arbitrary-path, and stale bindings perform no provider work" |
| Host counts a base line in the head document | `application_review_mutation_application_host` | "it reaches the provider on the left, on the head's path, with its first line and the base document's line count" |
| Wire compiler refuses a base range (slice 1 rule) | `github_mutation_contract` | "base lines 9 to 13 send side LEFT and start_side LEFT on the head's path and commit" |
| Record codec always version 1 | `sqlite_review_mutation_recovery_storage` | "a base-side record is written as version 2" |
| Legacy table admits a base-side anchor | `sqlite_review_mutation_recovery_storage` | "the side-less legacy table refuses a base-side anchor" |
| App gives a base line no draft (v001 rule) | `application_line_draft_contract` | phase 31 `base-line=2 draft=-` until the budget (`BASE_LINE_DRAFT_EVIDENCE:000`) |
| Evidence never reaches the source material | `application_line_draft_contract` | phase 30 `left-current=1 numbered=0` until the budget |
| Adapter validates a base line by permalink | `application_line_draft_contract` | phase 32 "Publication was refused because the pull request changed." |
| Host refuses every base-line root | `application_line_draft_contract` | phase 32 "Publication is unavailable for this target. Your draft text is kept." |
| The merge base is never the left document | `application_line_draft_contract` | phase 30 `left-current=0 numbered=0` |

**Pin.** `main`'s pins matched at `2f960c927` (an exported tree). Resealed,
after rebasing onto it, over the BPR-1331 v003 seal: `full_application`
`7d5d75e1…`, `production_app_manifest` `ec2dfc94…`;
`production_mutation_branch` unchanged (`e067befe…`). The dated "BPR-1212
v002 reseal" comment follows BPR-1331's.

**Rebased onto `2f960c927`** (BPR-1355's thread words, BPR-1331 v003, and
others): the branch is `a51d126f6` (product), `b8904fb38` (card),
`759fe7a0f` (the base draft's header), on top of `2f960c927`; the reviewed
frozen build is `cb16bfeed`, the same change before this rebase. After it,
the focused rows above pass again (150 with the Qt rows), with the composed
line-draft, label-authoring and threaded-reply courses;
`application_label_authoring_contract` passed on its rerun after one
load-budget expiry. `test_network_isolation_manifest` fails on `origin/main`
itself since BPR-1355 (`application_thread_words_public_path` is registered
uncontained without an entry); not this slice's.

**100 ms gates: not measured.** The optimized tests-on tree
(`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON
-DCMAKE_TEST_BUILD_DEPENDS=ON`, Apple clang Release) was built, but the
machine never went idle: two runs of `application_large_review_responsiveness`
at load averages of about 310 and 230 failed on different, unrelated phases
each time (the first on `paged_review_and_git_load`, `review_modality` and
`drawers_show`; the second on fourteen others, with `keyboard_line_down`
59.7 ms, `keyboard_line_extend` 10.5 ms and `keyboard_line_reveal` 90.7 ms
against slice 1's idle 9.6, 8.7 and 13.4). `application_first_tab_comparison`
passed. The gates are owed on an idle machine before this lands on `main`;
the branch is pushed for that, not merged.

**Observed, not this slice's:** the machine ran at a load average near 290
from other stations' builds while these rows ran. Under it,
`application_line_draft_contract` failed 4 of about 20 runs and passed alone
on rerun: twice stalled before the course's first file opened, with the
fixture's recording refusing an unexpected `search_pull_requests` read (the
queue's), once after every phase passed, on a `list_review_threads` refresh
that overlapped the base-line publication's (fixed: each publication now
waits for the previous refresh to settle), and once at the budget.
`application_epoch_host`'s BPR-1327 timing row failed once and passed three
times.

**Limits.**
- macOS only; Linux and Windows runs of these rows are owed by the stations.
- Base-side comments need the retained history: a sparse review (selected
  objects only) cannot derive the merge base, so its base lines stay refused
  with the reason.
- No fixture has a base branch that moved on in the commented file; the
  epoch host's test covers `differs` with its fake cache, and the composed
  course covers `same`. No live round on BPR-Test from this slice.
- The gutter's "+" stays on the head pane, and the one-pane view's code has
  no right-click menu, as before.

### Owner look v002 (pending)

One new short session, not the long default one. Three questions, each Accept
or Reject with any change in the note; none is self-certified.

- Session `aab71651cfa5303e138bc36a669c96b2`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1212-compose-v002`
  (repository root `/Users/jaeger/Documents/GitHub/BetterPullReview`), all
  three candidates ready at event cursor 12. The review host runs detached.
- Candidates, revision `frozen-cb16bfeed-v002`: `right-click-v002`
  (fingerprint `28938375…`), `restore-position-v002` (`654c50a1…`) and
  `publish-text-v002` (`6bf03f61…`).
- Frozen root `/Users/jaeger/bpr1212-compose-look-v002.TbOqXc`: the
  `git archive` of `cb16bfeed` built there with the dev preset (tests on), app
  target only; executable SHA-256 `9df888055a42…`. Offscreen,
  `--github-fixture inline-drawer-v1 --attended-open-file
  src/ui/conversation_cards_view.cpp` printed `ATTENDED_FILE_SHOWN`; the
  launcher opened it natively once and the window was closed. The fixture's
  merge base is its base commit (`916aad15`), so its base lines are GitHub's.
- Launch: `python3 /Users/jaeger/bpr1212-compose-look-v002.TbOqXc/launch.py`
  (the 1557-line file in Review). It checks the build, its fingerprint and the
  fixture tree first and reports an early exit.
- v001's session was released (`resume-development --all`) and its frozen
  root deleted.

Next: read the session's decisions. Accepts answer the v001 Reject and the
two notes; a Reject comes back as v003. [[BPR-1345]] (start a review) is next.


## Owner look v002: verdicts (session aab71651, handed back at event 16)

Session `aab71651cfa5303e138bc36a669c96b2`. All three are Accept, with no
notes:
- `publish-text-v002`: decision `fccea1dd-cd2e-42cf-a7e5-f37870298459`
  (event 13).
- `restore-position-v002`: decision `ff83cacc-085a-487d-8d1b-efdb3960bb3d`
  (event 14).
- `right-click-v002`: decision `9a74d750-690f-4a9e-83cb-f0088f0b9a91`
  (event 15).

The coordinator relayed these. The Mac releases the session and removes the
frozen root `/Users/jaeger/bpr1212-compose-look-v002.TbOqXc` once this
merges.

## v002 onto main: base numbering asked again (bpr-main-win, 2026-10-09)

**The fix** (the cause is recorded in the Windows section above).
- When an accepted review keeps the comparison and the base numbering is
  neither known nor pending, `github_application` asks again.
- A `cancelled` answer re-arms the question, and so does an ask the epoch
  host cannot admit while the authority is being replaced. The re-ask comes
  25 ms later, at most 80 times in a row.
- A first fix that re-armed only cancelled answers, at once, still hung. A
  trace showed the re-ask refused at admission while the accept was replacing
  the authority, which is why refused asks re-arm too.

**Deterministic test.** `application_line_draft_contract` now runs the course
twice: plain, and with `--cancel-first-base-numbering`. That testing hook (a
fixture-access switch, set on the course's first tick) turns the first
answer into `cancelled`.
- The fixed tree with both remedies switched off (a control, never
  committed): both runs time out at 40 s, `BASE_LINE_DRAFT_EVIDENCE:000`.
- With them: 3 of 3 pass, about 11 s for both runs together.

**Landed.**
- Rebased onto `512170300`. The pins were resealed twice: over main's
  intervening reseals, then over the fix. Mutation pin unchanged;
  `PIN_CHECK_OK`.
- Windows, Release tests-on, at `1c1b2ccec`: 26 of 26 focused rows pass. They
  include `application_line_draft_contract` (both runs), the review mutation
  and submission rows, `qt_workbench_line_draft`,
  `qt_diff_workbench_scale_{100,125,200}`,
  `qt_workbench_window_scale_{100,125,200}`, the round-trip contract,
  `architecture_boundaries` and `test_network_isolation_manifest`.
- `application_large_review_responsiveness` and
  `application_first_tab_comparison` pass with no `InputBudgetFailures`. The
  worst pulse is 61.4 ms; `keyboard_line_down`, `keyboard_line_extend` and
  `keyboard_line_reveal` are 14.6, 14.6 and 17.4 ms.
