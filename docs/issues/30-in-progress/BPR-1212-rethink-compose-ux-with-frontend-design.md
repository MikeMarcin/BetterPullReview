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
