---
schema: "implement-rfc.card.v1"
id: "BPR-1295"
title: "Merge Read into Review, with one command to hide and restore the open drawers"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1295--BPR-1295--47ff1149b806.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052","RFC-BPR-0005"],"related_cards":["BPR-1290","BPR-1293","BPR-1294","BPR-1021","BPR-0661","BPR-0721","BPR-0724","BPR-0727","BPR-0729","BPR-0649","BPR-0796","BPR-1132"],"acceptance":["BPR-1295:owner-answers","BPR-1295:three-modalities","BPR-1295:entries","BPR-1295:hide-restore","BPR-1295:keys-and-icon","BPR-1295:migration","BPR-1295:governing-text","BPR-1295:tests","BPR-1295:owner-look"],"owner_answers":{"date":"2026-10-06","hide_scope":"every drawer","entering_review":"remember the last arrangement","keys":"Cmd-B (Ctrl+B); Cmd-1/2/3 proposed, confirm at owner-look","icon":"beside the Comments toggle"},"sequence_after":"BPR-1290 (its src/ change is in flight; this card's code waits for it)","sequence_satisfied":"BPR-1290 Done 2026-10-06; its drawer is landed on origin/main as 0a7f72068"}
parked_on: "human-review"
---

# BPR-1295: Merge Read into Review, with one command to hide and restore the open drawers

## Origin

The owner, in chat on 2026-10-06, verbatim:

> Okay what if we merge read and review modalities into just review, and we
> have a hotkey/icon to quickly hide and restore open drawers. That should
> basically give read but be a little less awkward I think.

Earlier the same day, during [[BPR-1290]]:

> Honestly I'm not too attached to the modalities right now, i'm not sure how
> long I want to retain them. (beyond overview and review of course)

## Outcome

Three modalities: Overview, Review and Discuss. Every path that lands in Read
today lands in Review, and Review keeps the arrangement it last had. One
command, on ⌘B (Ctrl+B) and an icon beside the Comments toggle, closes every
open drawer and a second press brings back exactly the drawers that were open.
Discuss is kept as it is for now. Whether it stays is [[BPR-1293]]'s
question and the owner's, not this card's. Amended 2026-10-06 by the owner's
decision on BPR-1293 ("What if we were to replace discuss with
conversations."): Conversations replaces Discuss as the third modality, on ⌘3.
This card still builds the third slot as Discuss on ⌘3, and BPR-1293, built
after it, turns that slot into Conversations (see
[the amendment below](#owner-amendment-2026-10-06-conversations-replaces-discuss)).
Amended again 2026-10-06 by the owner's answers on BPR-1293 ("just keep
discuss for now even after 1294"): Conversations is added and Discuss is kept.
This card still builds Discuss on ⌘3; BPR-1293, built after it, adds
Conversations on ⌘3 and moves Discuss to ⌘4 (see
[the later amendment](#owner-amendment-2026-10-06-conversations-beside-discuss)).

## How Read and Review differ today

This was read from the code at `fc03ddccf`. **The only difference the window
makes between the two is the drawer preset.** The review bar, its commands,
composition, navigation, document tabs, one pane, line numbers and the
Updates line are the same in both. `apply_modality_surface`
(`src/adapters/qt/src/workbench_window.cpp:6042`) checks only for Overview and
Discuss. The other differences are the entry paths that pick Read and the
settings that name it.

| Area | Read (⌘2) | Review (⌘3) | Where |
|---|---|---|---|
| Drawer preset | Files collapsed, Repositories collapsed, Conversations open | Files open, Conversations open, Repositories left as they were | `choose_modality`, `workbench_window.cpp:5127-5200` (preset at 5173-5189; Read collapsing Repositories is [[BPR-0724]]) |
| Centre page, review bar, actions | Comparison page; full review bar | Same | `workbench_window.cpp:6042-6082` |
| Tabs | Shows the active tab's comparison | Same | `tab_modality`, `src/app/src/github_application.cpp:6090-6095`; spec `docs/ux/workbench-specification.md:136` |
| One pane | Independent of modality in code | Same | `src/core/include/bpr/core/workbench_interaction.hpp:47-49`. RFC-BPR-0028 still says "Read presents the comparison in one of two ways" (line 140) |
| Core | Retained, with no rule of its own | Retained, with no rule of its own | `workbench_modality`, `workbench_interaction.hpp:86-101`. Only Overview has a rule (`overview_keeps_drawers_collapsed`, line 79; `src/core/src/workbench_interaction.cpp:320, 472`) |
| Strip control | Button "Read", tooltip "Collapse the files drawer and keep comments beside the code." | Button "Review", tooltip "Open the files and comments drawers." | `src/adapters/qt/src/workbench_strip.cpp:701-787` |
| Keyboard | Ctrl+2 (⌘2) | Ctrl+3 (⌘3) | Assigned by position as `Ctrl+%1` (`workbench_strip.cpp:754-756`), so removing Read moves Discuss from ⌘4 to ⌘3 unless the shortcuts are set explicitly |
| Accessibility | "Read modality"; object names `reviewModalityReadAction`/`Button` | "Review modality"; `reviewModalityReviewAction`/`Button` | `workbench_strip.cpp:739-776`; the control's description "Overview, Read, Review and Discuss presets for which drawers are open" (742) |
| Setting "A pull request opens in" | Option Read (`openModalityRead`) | Not offered | Overview / Read / Last used, `workbench_window.cpp:6366-6384`; `review_open_modality` `src/application/include/bpr/application/workbench_runtime_ports.hpp:466-472` |
| Setting "Continue review enters" | Read, the default | Review | `review_continue_modality` `workbench_runtime_ports.hpp:474-478`, default Read at 538; Start review always enters Review (`enter_overview_at`, `workbench_window.cpp:6109-6123`) |
| Stored settings | `open_modality` 1, `continue_modality` 0 | `continue_modality` 1 | `src/adapters/sqlite/src/workbench_settings_database.cpp:95-98` (CHECK 0-2 and 0-1), read at 1280-1286 |
| Restored windows | Inferred from drawers: Files collapsed with Conversations open reads as Read | Both open reads as Review | `infer_modality_from_drawers`, `workbench_window.cpp:9609-9634`. The modality itself is never stored; Overview is never inferred ([[BPR-0727]]) |
| Last used | In memory only | Same | `LastUsedModality`, `workbench_window.cpp:5136, 6536-6547` |
| Paths that enter Read | Overview file row (`enter_read_at`, 1357 and 6125); Continue review by default; Discuss's Open in code (1519-1528, [[BPR-0649]]); Resume (`github_application.cpp:5994-5999`); a link to a file (12347-12353); Prepare tour and tour stops (`open_review_tour` 5597-5606; `github_application.cpp:8915`) | Start review; Continue review when set; restored windows with both drawers open | As listed |
| Discuss interplay | Discuss collapses Files and moves the Conversations panel into its page; leaving Discuss for Read or Review puts the panel back in the drawer | Same | `apply_modality_surface` 6052-6066. Discuss's "Open in code" returns to Read. A conversation link and defect navigation enter Discuss (`reveal_conversation` 9035; 4846) |
| Automated courses | The Overview course expects Continue review to land in Read (`src/app/main.cpp:6589-6590`). The reveal probe chooses Read and waits for it (`github_application.cpp:24260, 24269, 24379`) | Start/Continue course expects Review (`main.cpp:6141, 7116`); diff and fixture courses state Review (`main.cpp:1148`; `src/app/src/private_review_fixture_course.cpp:34`) | |

A mismatch found on the way: Overview's tooltip says "Open the files drawer
and close the comments drawer to read the pull request overview."
(`workbench_strip.cpp:721`), but Overview collapses every drawer
(`workbench_window.cpp:5173-5189`). Fix it while the purposes are rewritten.

### No hide or restore command exists today

- Each drawer has its own toggle: Files Alt+Shift+1, Conversations
  Alt+Shift+2, Repositories Alt+Shift+R (`workbench_window.cpp:759-788`). The
  title row presents them, with the Comments toggle carrying the count.
- The drawer-controls setting ([[BPR-0721]],
  `workbench_runtime_ports.hpp:500-511`) chooses whether a collapsed drawer
  keeps an 18-pixel rail. It hides nothing.
- There is no command to close every drawer, no focus or "zen" mode, and no
  ⌘\ (`Ctrl+\`) binding anywhere in `src/`. Read was the only quick way to
  get more room for the code, and it opened Conversations.

Note: "hide every open drawer" is not quite Read. Read kept the comments
beside the code. Hiding every drawer leaves the code alone, with its gutter
marks. Question 1 below asks which the owner wants.

## Proposal

### What moves to Review

- Every path that enters Read: Overview's file rows, Continue review, Discuss's
  Open in code, Resume, a link to a file, Prepare tour and tour stops.
- Read's strip slot. Review takes ⌘2.
- RFC-BPR-0028's **Read.** paragraph (aligned cards, gutter marks, unread
  dots, drafts), AC-05 and AC-08's "Read", which become "Review".
- The two-way presentation (one pane), which the code already offers in every
  modality.

### What disappears

- The Read button, its action and its accessible names.
- `qt::review_modality::read` and `core::workbench_modality::read`
  (`review_modality_count` 4 to 3).
- The "Continue review enters" settings row and `review_continue_modality`.
  Start and Continue review both enter Review.
- The Read option of "A pull request opens in". It becomes Overview / Review /
  Last used.
- The modality inference's Read branch.

### Entering Review (question 2, owner: remember)

- **Review remembers its arrangement.** Each window keeps the drawer
  arrangement Review last had, hidden drawers included. Entering Review from
  Overview, Discuss, a link, Resume, Open in code, Continue review or a tour
  stop restores that arrangement rather than a preset.
- **When the preset applies.** Review's preset (Files and Comments open,
  Repositories as they were) applies on a window's first entry to Review, and
  when ⌘2 is pressed while the window is already in Review. That second press
  is the way back to the preset.
- **Where it lives.** The remembered arrangement is core interaction state, as
  the drawer states are. It is window state and is not stored: a restored
  window's drawers are already stored per window, and they are Review's
  arrangement when the window restores in Review.

### Hide and restore the drawers

- **Command.** Hide Drawers / Show Drawers, a checkable window action
  (`toggleWorkbenchDrawersAction`).
- **Behaviour.** If any drawer is open, it closes them all and the window
  remembers which were open. If none is open, it reopens the remembered set at
  their saved widths. With nothing remembered (a new window, or after a
  restart), it opens Review's preset.
- **When the memory is dropped.** Opening a drawer by hand while the drawers
  are hidden, or pressing ⌘2 while already in Review (which applies the
  preset). The restore then cannot undo what the reviewer just did. Leaving
  Review and coming back keeps it: the remembered set is part of the
  arrangement Review remembers (question 2), so a window that left Review with
  its drawers hidden comes back hidden, and ⌘B still restores the same set.
- **Where it works.** Overview refuses it with the existing
  `overview_keeps_drawers_collapsed`. Discuss disables it, because there the
  conversation is the page, not a drawer.
- **Core.** The decision belongs to the core, like every drawer gesture. Add a
  `toggle_workbench_drawers_intent`, the remembered set in
  `workbench_interaction_state`, and a command id (`workbench_command_count`
  10 to 11), so the checked state and the enablement come from one projection.
- **Storage.** The remembered set is window state and is not stored. The
  drawer states it changes are already stored per window.
- **Gutter marks while hidden.** Clicking a thread mark in the gutter opens
  Conversations alone (a manual open), as in BPR-1290's prototype.

### Hotkey (question 3, owner: ⌘B)

**⌘B on macOS, Ctrl+B on Windows and Linux** (`Ctrl+B` in Qt), one binding,
window context. The owner chose it: a left-hand key, and the one VS Code uses
to toggle its side bar. The earlier proposal, ⌘\ with Alt+Shift+0, is
withdrawn. Alt+Shift+0 existed only because backslash needs AltGr on German
and Nordic layouts. B is a plain letter on every Latin layout, so no second
binding is proposed.

**Modality keys, proposed.** ⌘1 Overview, ⌘2 Review, ⌘3 Discuss (moved from
⌘4). The owner objected only to the toggle's keys and the button, not to
these. They are set explicitly rather than by strip position, and are
confirmed at the built-UI review (BPR-1295:owner-look). Amended 2026-10-06
([[BPR-1293]]), as corrected the same day: this card puts Discuss on ⌘3;
BPR-1293 then puts Conversations on ⌘3 and Discuss on ⌘4.

#### Every B binding, and how each collision is resolved

Searched on 2026-10-06 at `e0dd7301d`: every `QKeySequence`, `setShortcut`,
`QShortcut`, `Qt::Key_*` handler and menu mnemonic in `src/`, the chord table
in the window tests, and `docs/` for ⌘B, Ctrl+B and Bold. Qt's tables were
read from the bundled Qt 6.11.2 with a scratch probe (`QKeySequence::keyBindings`
under the cocoa and offscreen platforms), and a second probe put a
window-context `Ctrl+B` action beside a focused `QPlainTextEdit`, plain-text
`QTextEdit` and `QLineEdit`.

| Where | Binding today | Finding | Resolution |
|---|---|---|---|
| Application shortcuts and menus | None | No action, shortcut, key handler or mnemonic in `src/` uses B with any modifier. The menus are File, View, Pull Request, Profiles and Help; none has an `&B` mnemonic. The test chord table (`workbench_window_tests.cpp:2309-2324`) has no B. | Free. The toggle takes it. |
| Qt standard key `QKeySequence::Bold` | Ctrl+B, shown ⌘B on macOS | It is Bold on cocoa and on Qt's generic table. Nothing in Qt acts on it: with each of the three editors focused, ⌘B inserted nothing and the window's action fired, on cocoa and offscreen. The product never binds Bold. | No collision today. It also means a window shortcut fires inside an editor unless the editor claims the key, which the next row does. |
| Comment, reply, review and note composers; the suggestion editor; the tour editor; Review Cycle note fields | None | Plain-text Markdown (`setAcceptRichText(false)`), with no formatting command or toolbar. GitHub's web composer binds ⌘B to wrap the selection in `**`, so a reviewer's hand may expect Bold here. Some editors live in the Conversations drawer (Notes, side chat, and the in-place Reply of [[BPR-1290]]), so a toggle that won there would hide the text being written. | **Recommended: yield.** Each editable multi-line editor claims ⌘B in `ShortcutOverride`, as the label body already claims ⌘L from Home's Open bar (`feedback_label_controls.cpp:561`) and the label menu claims ⌘0. It does nothing with the key for now. Markdown Bold in composers would be its own card. Esc or a click in the code, then ⌘B, hides the drawers. |
| Single-line fields: the Files and Repositories filters, Home's Open bar | None | No meaning for ⌘B. | The toggle wins. If hiding removes the focused field, focus moves to the comparison. Home has no review, so the command is disabled there. |
| macOS text fields: Control-B | `Meta+B` (⌃B), `QKeySequence::MoveToPreviousChar` on cocoa | The Emacs caret move. It is a different key from ⌘B. The suggestion editor maps it. | No collision. |
| The macOS menu bar | View → Hide Drawers, ⌘B | A native key equivalent on the menu item. | Implementation check: with a composer focused on macOS, ⌘B must not trigger the menu item. Prove it in the window course, and on the composed window with the native menu bar. |
| The platforms | — | ⌘B is not a macOS system shortcut, and Ctrl+B is not reserved by Windows or by GNOME or KDE. | Free. |

**The alternative, the toggle winning everywhere,** is not recommended. It
would hide a drawer-hosted composer mid-sentence, and it would answer GitHub's
Bold habit with a layout change.

### Icon (owner: not on the title row's old spot)

The owner rejected the earlier placement, directly right of the modality
segment where Read's segment was. Proposed instead, for the owner's look at
the built UI:

- **Placement.** One icon-only tool button **immediately right of the Comments
  toggle** (the one carrying the count), before the divider that sets off
  Account and Settings. The title row's right end then reads:

  ```text
  … Overview  Review  Discuss │ Comments (3) │ [hide drawers] ┆ Account  Settings
  ```

  It closes the cluster of drawer controls at the right edge, next to the
  drawer it hides most often, and away from the modality segment.
- **Style.** The same size, frame and checked look as the drawer toggles
  (16-pixel glyph, icon only).
- **Glyph.** A window outline with a column on each side. Both columns are
  filled while drawers are shown; outlined while they are hidden, and the
  button is checked.
- **Tooltip.** "Hide drawers (⌘B)", or "Show Files and Comments (⌘B)", naming
  what returns. Disabled in Overview ("Overview keeps every drawer closed") and
  in Discuss ("In Discuss the conversation is the page").
- **Accessibility.** The same text as the accessible name and description, and
  a strong-focus stop.
- **Narrow windows.** It stays at every width, under the specification's rule
  that the title row sheds words, not controls.
- **Menus.** It is also in the View menu, after One pane. The windowless
  desktop menu bar carries the same item, disabled, to keep [[BPR-0796]]'s
  parity.
- **Considered and not proposed.** Left of the Comments toggle (it would read
  as part of the modality segment, the rejected spot), and the review bar's
  right end (that bar holds review commands, not window layout).

### Migration of stored settings and restored windows

- **`open_modality`.** Value 1 keeps its number and is renamed from `read` to
  `review`. No schema change, and the CHECK stays 0-2. A reviewer who chose
  "opens in Read" opens in Review.
- **`continue_modality`.** The column stays and is no longer read, and writes
  keep a valid value. The next schema change that touches the table may drop
  it. A stored Read and a stored Review both now mean Review.
- **Restored windows.** The modality was never stored, so nothing needs
  rewriting. A window saved in Read's shape (Files and Repositories collapsed,
  Conversations open) restores as Review in that shape: the reader keeps the
  room they had. A window with every drawer collapsed restores as Review with
  the drawers hidden, and the command then opens the preset.
- **Tabs and Resume.** Tabs ([[BPR-1132]]) and Resume keep their exact files,
  pairs and positions. Only the modality they land in changes.
- **Last used.** It lives in memory and needs no migration.

### RFC and specification amendments

These are recorded as dated owner amendments. Historical verdict records stay
as written.

**Landed 2026-10-06, ahead of the code** (docs only, while [[BPR-1290]]
changes `src/`):

- RFC-BPR-0028: dated amendments on the Modalities, Overview, Read, Drawers and
  Settings paragraphs and AC-03, each keeping its original wording, "Review
  since 2026-10-06" on the Read heading, and the owner's words in a new
  section, "Owner direction, 2026-10-06: Read merges into Review". That
  section states that AC-05's and AC-08's "Read" means Review. They are not
  edited in place: BPR-1290's branch (`claude/bpr1290-inline-drawer`) amends
  both, and the documentation gate requires [[BPR-1021]]'s closeout rows to
  match each criterion verbatim, so an edit here would collide with that
  branch's. For the same reason the new section sits before the 2026-09-24
  scaffolding section rather than at the end, where that branch appends.
  [[BPR-1021]]'s AC-03 row carries the amended AC-03. `git merge-tree` of this
  change with that branch is clean.
- The workbench specification: the title-row diagram and text, a Modalities
  paragraph that says the window still offers Read until this card is built,
  Tabs, the `#diff-` link, the hide-and-show paragraph under the drawers, the
  primary actions, the shortcut table and the ⌘B yield rule, the View menu and
  the windowless View menu.
- [[BPR-1290]]'s :reading and [[BPR-1294]]'s inventory now say Review.

**Still owed when the code lands:** `docs/architecture/native-markdown.md`
and `docs/ux/conventional-comments-feedback.md` (they describe what the
window does today), and AC-05's and AC-08's in-place wording with their
[[BPR-1021]] rows, once BPR-1290's amendments to them are on main.

The original list:

- **RFC-BPR-0028.**
  - The Modalities paragraph (lines 136-157): three modalities on ⌘1-⌘3, and
    Read's preset and the one-pane sentence move to Review.
  - The Overview paragraph (line 169): Continue review "enters Review".
  - The **Read.** paragraph (172-178) becomes **Review.**
  - The Drawers paragraph (201-204) gains hide and restore.
  - Settings (214): the modality rules.
  - AC-03: "the three modalities", plus hide and restore.
  - AC-05 and AC-08: "Review".
  - The closeout card [[BPR-1021]] follows these changes.
- **`docs/ux/workbench-specification.md`.**
  - The title-row diagram (34).
  - Tabs: "Overview and Discuss show the window's comparison; Review shows the
    active tab's" (136).
  - Links: `#diff-` selects the file "in Review" (222).
  - The View menu (811), and Primary actions (517-521) for the new icon.
- **`docs/architecture/native-markdown.md:4, 106`.** "Read and Discuss" becomes
  "Review and Discuss".
- **`docs/ux/conventional-comments-feedback.md:322`.** The fixture course
  selects `PATH` "in Review".
- **RFC-BPR-0052.** It names no modality and needs no change. Neither does
  RFC-BPR-0005: comments stay in the drawer.
- **Live cards.**
  - [[BPR-1290]]'s :reading ("In Read, …") becomes Review.
  - [[BPR-1293]]'s outcome becomes "opens its file in Review". Done
    2026-10-06 with BPR-1293's owner decision.
  - [[BPR-1294]]'s "in Read the only way to reach a pull request comment …"
    becomes "in Review".
  - Parked [[BPR-1083]]'s launch steps open the fixture "in Read"; reword them
    when the card is next prepared. "In Read" in [[BPR-0720]] and [[BPR-0738]]
    labels historical captures and stays as written.

### Tests affected

Counts are references to the Read or Review modality, or to the two settings,
at `fc03ddccf`.

- `tests/adapters_qt/workbench_window_tests.cpp` (58): the modality presets
  (around 9275-9343), the settings rows (8190-8194), the chord table
  (2309-2324; ⌘B is not an Alt chord, so it gets its own case: it toggles from
  the code, a drawer list and a filter field, and does nothing with a composer
  focused), and the Overview file row (9636).
- `tests/sqlite_contract/workbench_settings_storage_tests.cpp` (10):
  `open_modality` and `continue_modality` round trips and old-row rebuilds.
- `tests/application/workbench_preferences_consumers_tests.cpp` (6) and
  `tests/application/workbench_application_host_tests.cpp` (2).
- `tests/core/workbench_interaction_tests.cpp` (5): the modality loops at
  1010, 1053 and 1204; new cases for the hide-and-restore intent and the
  eleventh command (617).
- `tests/adapters_qt/composed_github_tests.cpp` (4).
- `tests/end_to_end/`:
  - setup_destination (6), profile_multi_window (5),
    profile_window_review_restoration (4), application_scaling (3),
    large_review_benchmark (2);
  - one each in review_submission_composition, agent_invocation_composition,
    agent_finding_composition, profile_window_layout, attended_recent_replay,
    release_maintenance, offline_inventory and rate_limit;
  - `windows_title_row_host_tests.cpp` for the extra title-row button and the
    command count (222).
- Automated courses in `src/app/main.cpp` (2844, 6589-6590, 7089, 7116) and
  `src/app/src/github_application.cpp` (24260, 24269, 24379,
  `ReadEmptyDirected`), which expect Read.

## Relation to other cards

- **[[BPR-1290]]** (being built, 2026-10-06). The inline-only drawer is the
  Conversations drawer of Review. Its :reading criterion named Read and now
  names Review; its prototype captions ("Read on …") label historical frames
  and stay. With hide and restore, the inline drawer is one key away from the
  whole width going to the code. This card's text amendments land first, so
  BPR-1290 is built and accepted against Review. **This card's code waits for
  BPR-1290 to land**: both change `workbench_window.cpp` and the window tests,
  and BPR-1290's src/ change is in flight.
- **[[BPR-1293]]** (Backlog). The PR-wide finder opens a conversation in Read
  today; it opens it in Review. Discuss's fate, which the owner's "beyond
  overview and review" puts in doubt, stays with BPR-1293. This card keeps
  Discuss unchanged apart from its shortcut. Amended 2026-10-06: BPR-1293 is
  Ready and adds Conversations on ⌘3 beside Discuss, which it moves to ⌘4,
  after this card.
- **[[BPR-1294]]** (Backlog). PR-wide comments are heading to Overview, one of
  the two modalities the owner keeps. This card removes the "in Read" wording
  from its inventory and changes nothing else there.

## Open questions for the owner

1. **Hide scope.** Should the command hide every open drawer, Comments
   included, leaving the code alone (your words "hide and restore open
   drawers")? The other choice is to hide only Files and Repositories, which is
   exactly today's Read. Recommended: every open drawer. Alt+Shift+2 brings
   Comments back alone.
2. **Entering Review.** When you enter Review from Overview, a link, Resume or
   Open in code, should the window keep the arrangement Review last had,
   hidden drawers included? Recommended: yes, keep it. The preset (Files and
   Comments open) applies on a window's first entry and when you press ⌘2
   while already in Review. Accept or Reject.
3. **Keys.** ⌘1 Overview, ⌘2 Review, ⌘3 Discuss (moved from ⌘4), and ⌘\ (also
   Alt+Shift+0) to hide and restore. Accept or Reject; put any change in the
   note.

## Owner answers, 2026-10-06

In chat, relayed by the coordinator, verbatim:

1. Hide scope: "Every drawer (Recommended)".
2. Entering Review: "Yes, remember (Recommended)".
3. Keys and icon: "Those hotkeys and that button location don't sound very
   good. I want something for this toggle I can hit with my left hand easily.
   A quick google shows maybe cmd+b is a reasonable option on mac for hiding
   clutter."

What follows from them:

- **1.** The command hides every open drawer: Repositories, Files and
  Comments. Alt+Shift+2 brings Comments back alone.
- **2.** Review remembers its last arrangement, hidden drawers included; the
  preset applies on a window's first entry and on ⌘2 in Review (see
  [Entering Review](#entering-review-question-2-owner-remember)).
- **3.** The toggle is ⌘B (Ctrl+B), with a composer yielding it (see
  [Hotkey](#hotkey-question-3-owner-b)). The title-row spot beside the
  modality segment is dropped for one beside the Comments toggle (see
  [Icon](#icon-owner-not-on-the-title-rows-old-spot)), shown to the owner at
  the built UI. ⌘1, ⌘2 and ⌘3 drew no objection and stay proposed, to be
  confirmed at the same look.

The card is Ready on these answers. Its code waits for [[BPR-1290]] to land.

## Owner amendment, 2026-10-06: Conversations replaces Discuss

Superseded the same day in part by
[the next amendment](#owner-amendment-2026-10-06-conversations-beside-discuss):
Discuss is kept, and Conversations takes ⌘3 beside it.

On [[BPR-1293]]'s finder prototype the owner said in chat, verbatim, "I like
that a lot." Asked "Which finder placement should we build? Like Home / Tab /
Both", the owner answered, verbatim: "What if we were to replace discuss with
conversations."

The modalities become **Overview ⌘1, Review ⌘2 and Conversations ⌘3**.
Conversations is BPR-1293's finder in the main pane; picking a conversation
switches to Review on its file with the thread open in the inline drawer, and
⌘J goes straight to Conversations. For this card:

- **Nothing in its build changes.** It removes Read, sets ⌘1 to ⌘3
  explicitly, and leaves the third slot as Discuss. BPR-1293 is sequenced after
  this card and replaces that slot with Conversations on the same key.
- **⌘B in the third modality.** This card disables it in Discuss, and
  BPR-1293 keeps it disabled in Conversations, whose page is the finder.
- **Entering Review.** A pick in Conversations is one more path that enters
  Review in the arrangement Review last had. When the picked thread needs the
  Comments drawer and it is closed, BPR-1293 opens it as a manual open, as a
  gutter mark's press does here.
- **Owner-look.** The question on the ⌘1-⌘3 keys asks about the keys as built
  by this card (Discuss on ⌘3), and says that Conversations takes ⌘3 when
  BPR-1293 lands.

## Owner amendment, 2026-10-06: Conversations beside Discuss

The owner answered BPR-1293's two questions in chat, verbatim. On the drawers
while Conversations shows: "Hide drawers". On the gap for pull request wide
comments if Discuss is removed: "just keep discuss for now even after 1294".

So Conversations is **added** and Discuss is **kept**. The modalities become
**Overview ⌘1, Review ⌘2, Conversations ⌘3 and Discuss ⌘4**, and ⌘J still goes
to Conversations. For this card:

- **Its build is unchanged.** It removes Read, sets ⌘1 to ⌘3 explicitly with
  Discuss on ⌘3, and disables ⌘B in Discuss. BPR-1293, built after it, adds
  Conversations on ⌘3, moves Discuss to ⌘4, and disables ⌘B in Conversations
  too, where the drawers are hidden.
- **Discuss stays as this card leaves it.** BPR-1293 does not change its
  behaviour or its preset, and Overview's refusal, which names the modalities
  that show drawers, does not gain Conversations, which shows none.
- **Owner-look.** The question on the ⌘1-⌘3 keys asks about the keys as built
  by this card, and says that once BPR-1293 lands Conversations takes ⌘3 and
  Discuss ⌘4.

## Acceptance

- **BPR-1295:owner-answers.** The three questions are answered by the owner
  and recorded here, verbatim. Recorded 2026-10-06 (above).
- **BPR-1295:three-modalities.** The strip offers Overview, Review and Discuss.
  No Read remains in the core, the Qt adapter, settings, tooltips or
  accessible names. (Amended 2026-10-06: Discuss stays here as the third
  modality; [[BPR-1293]] afterwards adds Conversations on ⌘3 and moves
  Discuss to ⌘4.)
- **BPR-1295:entries.** Every path in the inventory that entered Read enters
  Review in the arrangement Review last had in that window; the preset applies
  on the window's first entry and on ⌘2 in Review.
- **BPR-1295:hide-restore.** Hide closes every open drawer (Repositories,
  Files and Comments) and Show restores exactly the remembered set at its
  saved widths. A hand toggle while hidden, or ⌘2 in Review, drops the memory;
  leaving Review and returning keeps it. Overview refuses with its reason and
  Discuss disables the command. The core decides it, and core tests cover it.
- **BPR-1295:keys-and-icon.**
  - ⌘B (Ctrl+B) toggles from the code, the drawers' lists and single-line
    fields, and does nothing while an editable multi-line editor has focus,
    the native macOS menu item included. ⌘1, ⌘2 and ⌘3 choose Overview,
    Review and Discuss (once [[BPR-1293]] lands, ⌘3 is Conversations and ⌘4
    Discuss; amended 2026-10-06). No key sequence is duplicated in the assembled window.
  - The icon sits immediately right of the Comments toggle, shows its state,
    and carries tooltip, accessible name and focus stop.
  - It stays at the platform's narrowest width.
  - The View menu, and the windowless menu bar (disabled), carry the item.
- **BPR-1295:migration.** Stored `open_modality` 1 opens in Review. Stored
  `continue_modality` of either value is accepted and ignored. A window saved
  in Read's shape restores in Review with that shape. Storage tests cover old
  rows.
- **BPR-1295:governing-text.** The amendments listed above land, and the
  documentation gate passes.
- **BPR-1295:tests.** The affected tests are updated, and the focused Qt,
  core, SQLite and end-to-end selectors pass. `qt_composed_github` is
  included, per the board's selector trap.
- **BPR-1295:owner-look.** The owner uses the merged Review and the
  hide-and-restore command in an attended session and accepts it, with one
  question each for the icon's placement and the ⌘1-⌘3 keys. Required, not
  self-certified.

## Boundaries

This changes the presentation and the core's drawer decision. It adds no new
authority and no GitHub, Git or SQLite effect beyond the existing settings
record. It does not decide Discuss's future ([[BPR-1293]]; on 2026-10-06 the
owner added Conversations there and kept Discuss), the drawer's
design ([[BPR-1290]]), or where comments that are not on a file live
([[BPR-1294]]).

## Coordinator note, 2026-10-06: Discuss keeps ⌘4

So Discuss's key does not move twice, this card leaves Discuss on ⌘4 and
leaves ⌘3 unbound; BPR-1293 then gives ⌘3 to Conversations. Overview stays
⌘1 and Review takes ⌘2. Confirmed at this card's built-UI review.

## Owner amendment, 2026-10-06: Discuss on ⌘3, Conversations on ⌘4

The owner, in chat on 2026-10-06, relayed by the coordinator, verbatim:

> can we swap discuss to 3 and conversations to 4

This supersedes the coordinator's note above ("Discuss keeps ⌘4") and the
key assignments in the amendments before it. This card builds **⌘1 Overview,
⌘2 Review and ⌘3 Discuss**, and leaves ⌘4 unbound; [[BPR-1293]] gives ⌘4 to
Conversations (recorded there too). BPR-1295:keys-and-icon reads "⌘1, ⌘2 and
⌘3 choose Overview, Review and Discuss", with ⌘4 free. The owner-look
question on the keys asks about ⌘1 Overview, ⌘2 Review and ⌘3 Discuss.

## Sequencing met, 2026-10-06: ready to implement

This card's code was sequenced after [[BPR-1290]], whose `src/` change was in
flight. BPR-1290 is now Done: the owner accepted built
drawer v005 (session `f76a3bf8`), published as `0a7f72068`, and no BPR-1290
candidate holds the drawer files any more. Nothing else in `depends_on` is
open, so this card is eligible to claim. Implementation starts from
`origin/main` holding `0a7f72068`; the Read paths inventoried above are those
of that tree.

## Built v001, 2026-10-06

Candidate `0456feb78` (implementation `ab311fba5` plus the benchmark's two
phases), on `origin/main` at `c5d9c7fc1`. Drawn with `frontend-design` for the
icon.

- **Core** (`workbench_interaction`). `workbench_modality` is Overview, Review
  and Discuss. `set_workbench_modality_intent` gains `Entry` (`enter` or
  `choose`). The core keeps Review's arrangement (`ReviewArrangement`, the three
  drawers and the hidden set) while another modality shows and restores it on
  entry; its preset (Files and Comments open, Repositories as they were)
  applies on the window's first entry (`ReviewEntered`) and on `choose` while
  Review shows. `toggle_workbench_drawers_intent` hides every open drawer into
  `HiddenDrawers` and shows exactly that set, or the preset;
  `project_workbench_drawer_restore` names what Show opens. A hand-opened
  drawer and `choose` in Review drop the set. Overview refuses with
  `overview_keeps_drawers_collapsed`, Discuss with the new
  `discuss_shows_the_conversation`. `workbench_command_count` is 11.
- **Host.** `workbench_projection::DrawerRestore`, published when it changes.
  Nothing new is stored.
- **Qt.** The strip has three segments, keys set per modality: Ctrl+1, Ctrl+2,
  Ctrl+3 (the owner's swap; Ctrl+4 free). Choosing from the strip sends
  `choose`; every other path sends `enter`, so only Discuss still sends a preset.
  Hide Drawers (`toggleWorkbenchDrawersAction`, Ctrl+B, window context) is in
  View after One pane, on the windowless bar disabled, and in the title row
  immediately right of the Comments toggle (`workbenchToggleDrawersButton`,
  icon only, strong focus). Glyph: a window with filled side panels while
  shown, outlined and checked while hidden. Tooltip "Hide drawers (⌘B)" or
  "Show Files and Comments (⌘B)"; disabled with "Overview keeps every drawer
  closed", "In Discuss the conversation is the page", on Home, and while an
  editable multi-line editor in the window has focus ("The text you are
  writing keeps ⌘B. Click the code first."). Such an editor also claims the
  key in `ShortcutOverride` and drops it. A drawer that held focus when it hid
  gives focus to the code. Settings: "A pull request opens in" is Overview,
  Review, Last used; "Continue review enters" is gone. Inference adopts
  Review for every restored shape. The Overview tooltip no longer says it
  opens the files drawer.
- **Why disabled while composing, not only the key claim.** The new native
  course found that on cocoa the View menu item's key equivalent fires with a
  note focused, and no `ShortcutOverride` reaches Qt
  (`note focused: toggles=1`). With the command disabled while an editor has
  the keyboard, AppKit's item is disabled and the key goes to the editor.
- **SQLite.** No schema change. `open_modality` 1 is Review; `continue_modality`
  is not read and every write stores 1.
- **Entries.** Overview's file row, Start and Continue review, Discuss's Open in
  code, Resume, a link to a file, Prepare tour and tour stops enter Review.
- **Governing text.** RFC-BPR-0028's Modalities paragraph, AC-03 (swap), AC-05
  and AC-08 (Review in place) and its two owner sections; [[BPR-1021]]'s rows
  word for word; [[BPR-1293]] records the swap; the specification's Modalities,
  Conversations, title-row diagram, shortcut table and ⌘B rule;
  `conventional-comments-feedback.md`. `DOCUMENTATION_OK`.
- **Round-trip pins** resealed with notes for the `src/app` wording and course
  changes (Resume, link and tour entries, tab modality, course helpers,
  `ReviewEmptyDirected`, the review-behaviour course). The pins matched at
  `bb9254ff3`; the mutation pin is unchanged.

### Verification on the candidate

- Debug (`build/dev`), macOS arm64: `core_workbench_interaction`,
  `application_workbench_host`, `application_workbench_preferences_consumers`,
  `sqlite_workbench_settings_storage`, `qt_workbench_window_scale_{100,125,200}`,
  `qt_composed_*` (github, inline drawer, markdown, home, loading, filters),
  `qt_feedback_label_settings`, `application_composition_contract`,
  `application_review_round_trip_composition_contract`,
  `architecture_boundaries`, `architecture_boundary_contract`, and the end-to-end
  rows the card names (setup destination, profile multi-window, window layout,
  restart and review restoration, scaling paths, attended replay, release
  maintenance, offline inventory, rate limit, agent invocation and finding,
  review submission, label authoring, overview and review behaviour, first
  tab): all passed. `qt_hide_drawers_native_menu_key` (cocoa) passed.
- **New rows.**
  - Core: `test_review_remembers_its_arrangement`,
    `test_drawers_hide_and_show_exactly` (every subset of open drawers comes
    back exactly).
  - Window: `test_hide_drawers_command_key_icon_and_editor_yield`, through the
    real core. It covers the key from the code, the files list and the files
    filter; restore at the same widths; an editable note keeping the key (the
    command disabled, no text, no gesture) and a read-only one not; a hand
    toggle dropping the memory; disabled in Overview, Discuss and Home; the
    button's place, size, focus, names and glyph; and no window- or
    application-wide key held twice (Ctrl+1/2/3 used, Ctrl+4 free).
  - SQLite: `test_read_settings_open_in_review_and_continue_modality_is_ignored`.
  - `application_attended_recent_replay`: the same key scan on the assembled
    window.
  - `qt_hide_drawers_native_menu_key`: AppKit's View menu offers Hide Drawers;
    ⌘B sent to the key window with a note focused hides and types nothing,
    with the item disabled; from the files list it hides and restores.
- **Failing-before controls**, mutations of the fixed product, each failing its
  row: Show ignoring the remembered set; entering Review ignoring its
  arrangement; a hand open keeping the set (core); no editor yield (window);
  the icon left of the Comments toggle (title-row order); Discuss on Ctrl+4
  (keys). The native course failed on the product before the
  disabled-while-composing change.
- **Optimized tests-on tree** (the frozen root's, Release, `-j1`), 27 of 27,
  `BudgetsEnforced` true, no input failures or known misses: drawers_hide 17.2
  and 17.7 ms, drawers_show 25.9 and 25.0 ms, review_modality 33.0 and 33.6 ms,
  files and comments toggles 20.9 to 37.4 ms; the large-review, first-tab,
  unavailable and approximate rows; the window, inline drawer, markdown and
  cards rows; the native course.
- Format: clang-format on every changed file. clang-tidy on the changed product
  translation units: clean apart from a pre-existing
  `bugprone-signed-bitwise` at `workbench_settings_database.cpp`'s
  `OpenFlags | SQLITE_OPEN_NOFOLLOW`, a line this card does not touch.

### Published

The frozen candidate `0456feb78` is published as `9446afe42` and `04d029511`
after a rebase onto `b078d8bdf`, which adds [[BPR-1089]]'s Overview label
chips (`68f00eb15`) with its own reseal, and otherwise only board moves and
[[BPR-1227]]'s owner-channel test fix (`c30e8bcf0`). The pins conflicted only there: both
notes are kept, BPR-1295's now reads over BPR-1089's, and the values were
recomputed on the merged tree (mutation pin unchanged). On the merged tree the
focused rows pass 38 of 39 in Debug with `qt_hide_drawers_native_menu_key`;
the one failure, `qt_pull_request_overview_page` ("Enter on the file's other
cells still reads it"), is BPR-1089's new assertion on the Overview page alone,
which this card does not touch; it fails the same way on plain `09334cf71`
(checked in a scratch build of that commit).

### Owner look, session v001

Pending: BPR-1295:owner-look is the owner's, not self-certified. One session,
four candidates, one question each, Accept or Reject.

- Session `c8c64c2b945c0d65c6a50addb4129319`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261006-bpr1295-merged-v001`,
  ready at event cursor 16. Candidates (revision `frozen-0456feb78-v001`):
  `BPR-1295-merged-review-v001` (the merged Review), `BPR-1295-hide-drawers-v001`
  (the ⌘B toggle), `BPR-1295-icon-placement-v001` (the icon's place, with the
  frozen tree's 2x renders), `BPR-1295-modality-keys-v001` (⌘1 Overview, ⌘2
  Review, ⌘3 Discuss). The host was started detached and answered on loopback.
- Frozen root `/Users/jaeger/bpr1295-merged-review-v001.Gjexql`: `git archive`
  of `0456feb78` built there Release with tests on, source read-only;
  executable SHA-256 `4152c8d47ee4e6ce…`. Offscreen with the launcher's
  arguments it printed `ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`.
- Launch: `python3 /Users/jaeger/bpr1295-merged-review-v001.Gjexql/launch.py`
  (`--overview` stops on Overview). It checks the build and the fixture tree
  first and reports an exit within seconds.

Next: read the session's decisions (`status --after 16`). Four Accepts satisfy
:owner-look and the card finishes; a Reject comes back with its note for a v002.

### Owed station runs (not blocking)

Linux and Windows have not run this change. At `origin/main` holding
`0456feb78`, no remote mutation:

- `bpr-main-linux`: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure -R '^(core_workbench_interaction|application_workbench_host|application_workbench_preferences_consumers|sqlite_workbench_settings_storage|qt_workbench_window_scale_.*|qt_composed_github_scale_.*|qt_composed_inline_drawer_scale_.*|qt_feedback_label_settings|application_attended_recent_replay|application_setup_destination_public_path|application_profile_multi_window_public_path|application_profile_window_review_restoration_public_path|application_review_round_trip_composition_contract|application_composition_contract|architecture_boundaries)$'`,
  then in the optimized tests-on tree
  (`cmake --preset app-release -B build/app-release-tests -DBUILD_TESTING=ON -DCMAKE_TEST_BUILD_DEPENDS=ON && cmake --build build/app-release-tests && ctest --test-dir build/app-release-tests -j1 --output-on-failure -R '^(application_large_review_.*|application_first_tab_comparison|qt_composed_inline_drawer_scale_.*|qt_workbench_window_scale_.*)$'`).
- `bpr-main-win`: `cmake --preset windows-dev && cmake --build --preset windows-dev && ctest --preset windows-dev -j4 --output-on-failure -R` with the same first expression, then the optimized tree (`build/bpr0967-release`, MSVC `/O2`) with the second.

`qt_hide_drawers_native_menu_key` is macOS only. A failure there opens a new
card against this change.

