---
schema: "implement-rfc.card.v1"
id: "BPR-1295"
title: "Merge Read into Review, with one command to hide and restore the open drawers"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1295--BPR-1295--47ff1149b806.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052","RFC-BPR-0005"],"related_cards":["BPR-1290","BPR-1293","BPR-1294","BPR-1021","BPR-0661","BPR-0721","BPR-0724","BPR-0727","BPR-0729","BPR-0649","BPR-0796","BPR-1132"],"open_questions":["hide scope: every open drawer or only Files and Repositories","entering Review: reapply its preset or keep the window's last arrangement","keyboard map: Cmd-1 Overview, Cmd-2 Review, Cmd-3 Discuss, Cmd-backslash hide and restore"],"acceptance":["BPR-1295:owner-answers","BPR-1295:three-modalities","BPR-1295:entries","BPR-1295:hide-restore","BPR-1295:keys-and-icon","BPR-1295:migration","BPR-1295:governing-text","BPR-1295:tests","BPR-1295:owner-look"]}
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
today lands in Review. One command, with a key and a title-row icon, closes
every open drawer and a second press brings back exactly the drawers that were
open. Discuss is kept as it is for now. Whether it stays is [[BPR-1293]]'s
question and the owner's, not this card's.

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

### Hide and restore the drawers

- **Command.** Hide Drawers / Show Drawers, a checkable window action
  (`toggleWorkbenchDrawersAction`).
- **Behaviour.** If any drawer is open, it closes them all and the window
  remembers which were open. If none is open, it reopens the remembered set at
  their saved widths. With nothing remembered (a new window, or after a
  restart), it opens Review's preset.
- **When the memory is dropped.** Opening a drawer by hand, or choosing a
  modality, while the drawers are hidden. The restore then cannot undo what the
  reviewer just did.
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

### Hotkey

**⌘\ on macOS, Ctrl+\ on Windows and Linux** (`Ctrl+\\` in Qt), with
**Alt+Shift+0** as a second binding on the same action.

- ⌘\ is unbound in the application. ⌘0 is Home, ⌘1-⌘4 are the modalities,
  ⌘, is Settings, ⌘L focuses the Home bar, ⌘W closes a tab and ⌘N opens a
  window. The Alt and Alt+Shift chords are listed in
  `tests/adapters_qt/workbench_window_tests.cpp:2309-2324`.
- No standard Qt key is bound to it on any of the three platforms.
- On German and Nordic layouts, backslash needs AltGr or ⌥⇧7, so a second
  binding is needed. Alt+Shift+0 is free and belongs to the drawer family:
  Alt+Shift+1, 2 and R are the drawers, and 3, 4 and 5 are taken by details,
  progress and line numbers. Read "0" as "no drawers".

### Icon

- **Placement.** An icon-only tool button on the title row, directly right of
  the modality segment and before Refresh, where Read's segment was.
- **Glyph.** A window frame with a column on each side: filled while drawers
  are shown, outlined while they are hidden. It is checked while hidden.
- **Tooltip.** "Hide drawers (⌘\)", or "Show Files and Comments (⌘\)",
  naming what returns.
- **Accessibility.** The same text as the accessible name and description, and
  a strong-focus stop.
- **Narrow windows.** It stays at every width, under the specification's rule
  that the title row sheds words, not controls.
- **Menus.** It is also in the View menu, after One pane. The windowless
  desktop menu bar carries the same item, disabled, to keep [[BPR-0796]]'s
  parity.
- **Considered and not proposed.** The review bar's right end. That bar holds
  review commands, not window layout.

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
  - [[BPR-1293]]'s outcome becomes "opens its file in Review".
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
  (2309-2324, which gains ⌘\ and Alt+Shift+0), and the Overview file row
  (9636).
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

- **[[BPR-1290]]** (Parked, owner design discussion). The inline-only drawer
  is the Conversations drawer of Review. Its prototype is drawn "Read on …"
  and its :reading criterion names Read; both become Review. With hide and
  restore, the inline drawer is one key away from the whole width going to the
  code. Neither card blocks the other, but land this card's text amendments
  first, so BPR-1290 is built and accepted against Review.
- **[[BPR-1293]]** (Backlog). The PR-wide finder opens a conversation in Read
  today; it opens it in Review. Discuss's fate, which the owner's "beyond
  overview and review" puts in doubt, stays with BPR-1293. This card keeps
  Discuss unchanged apart from its shortcut.
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

## Acceptance

- **BPR-1295:owner-answers.** The three questions are answered by the owner
  and recorded here, verbatim.
- **BPR-1295:three-modalities.** The strip offers Overview, Review and Discuss.
  No Read remains in the core, the Qt adapter, settings, tooltips or
  accessible names.
- **BPR-1295:entries.** Every path in the inventory that entered Read enters
  Review, with the arrangement question 2 settles.
- **BPR-1295:hide-restore.** Hide closes every drawer in scope and Show
  restores exactly the remembered set at its saved widths. A hand toggle or a
  modality choice drops the memory. Overview refuses with its reason and
  Discuss disables the command. The core decides it, and core tests cover it.
- **BPR-1295:keys-and-icon.**
  - The keys in question 3 work from anywhere in the window and collide with
    nothing in the chord table.
  - The title-row icon shows its state and carries tooltip, accessible name
    and focus stop.
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
  hide-and-restore command in an attended session and accepts it. Required,
  not self-certified.

## Boundaries

This changes the presentation and the core's drawer decision. It adds no new
authority and no GitHub, Git or SQLite effect beyond the existing settings
record. It does not decide Discuss's future ([[BPR-1293]]), the drawer's
design ([[BPR-1290]]), or where comments that are not on a file live
([[BPR-1294]]).
