---
schema: "implement-rfc.card.v1"
id: "BPR-1532"
title: "First and last change in the current file on Option-Shift-Up and Option-Shift-Down"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Add Alt+Shift+Up / Alt+Shift+Down (Option-Shift-Up / Option-Shift-Down on macOS) to go to the first / last change in the current file.","governing_rfc":"RFC-BPR-0076","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028"],"related_cards":["BPR-1328","BPR-1417","BPR-1493","BPR-1531"],"reservation":"docs/issues/_reservations/BPR-1532--BPR-1532--fc312ac373f6.json"}
---

# BPR-1532: First and last change in the current file on Option-Shift-Up and Option-Shift-Down

The owner asked on 2026-10-15 for Option-Shift-Up and Option-Shift-Down
(Alt+Shift+Up and Alt+Shift+Down elsewhere) to go to the first and the last
change in the current file. They sit beside Previous change and Next change,
Option-Up and Option-Down ([[BPR-1328]]), and Previous file and Next file,
Option-Command-Up and Option-Command-Down.

Governing design: [RFC-BPR-0076](../../rfcs/RFC-BPR-0076-file-change-navigation-enters-review.md)
(file and change navigation enters Review, [[BPR-1417]]), under the
[workbench allocation](../../architecture/system-architecture.md#rfc-bpr-0005-workbench-allocation):
the core decides the target and whether the command is offered, the host
resolves the exact Review context, and Qt translates the key and presents.
The keys are recorded in the [workbench specification](../../ux/workbench-specification.md).

## Behaviour

- **Unit.** A change is what Option-Up and Option-Down step through: one
  change group of the selected file's exact comparison, in the comparison's
  order. The comparison shows the whole file; it folds no hunks and hides no
  change, so the first and last change are the first and last group in that
  order, in two panes and in One pane alike. Change navigation never crosses
  into another file.
- **Target.** First change selects the first group and Last change the last,
  wherever the selection is: on a change, on an unchanged line, or nowhere.
  The core decides the target through the existing change-navigation
  transition, extended with a reach that names the outermost change.
- **Already there.** On the first change, First change keeps that selection
  and scrolls it into view again, with no bound message: the press is the way
  back to it after scrolling away. Likewise Last change.
- **Offered.** Both commands are disabled where nothing can be reached: no
  active workbench, and in Review a file with no open comparison or with no
  changes. The menu item greys and its tooltip names the reason. From
  Overview, Discuss or Threads the commands are offered and enter Review
  first, as the other four do ([[BPR-1417]]); if the Review context then has
  no changes, the press shows the existing three-second message and keeps
  the position.
- **Menus.** The window's View menu and the macOS desktop bar's View menu
  offer `Fi&rst change` and `Last c&hange` after `Ne&xt change`, with the
  keys. The mnemonics R and H are unused in View. The review bar gains no
  buttons.
- **Keys yield.** In any widget that takes typed text (editable line and
  multi-line editors, editable combo boxes, spin boxes and the Review Cycle
  suggestion editor) Option-Shift-Up and Option-Shift-Down keep their text
  meaning: selection extension on macOS, box carets in the suggestion editor.
  The window keys are unbound there. Where the focused widget or one of its
  ancestors holds its own widget-scoped binding of the same key, as the tour
  editor's stop outline does to reorder stops, the window keys are unbound
  too, so that binding runs and Qt sees no ambiguous shortcut.

## Acceptance

- AC1: Core: the change-navigation transition selects the first or last
  group from a change, an unchanged line and no selection; on the first or
  last change it keeps the selection; with no changes it selects nothing.
  The command projection disables First/Last change with no active
  workbench and, in Review, with no open comparison or no changes, and
  offers them otherwise. A negative control (the transition stepping one
  change instead of reaching the end) fails a test.
- AC2: Qt: the window's View menu and the desktop bar's View menu carry
  `Fi&rst change` and `Last c&hange` with Alt+Shift+Up and Alt+Shift+Down,
  in the same order, no other View mnemonic uses R or H, and the keys
  delivered to the window dispatch the outermost change intent. Disabled
  state follows the projection.
- AC3: Qt: with the tour editor's outline focused, Alt+Shift+Up and
  Alt+Shift+Down reorder the stop and do not navigate; with an editable text
  widget or the suggestion editor focused, the keys do not navigate.
- AC4 (human): The owner accepts the frozen candidate: "⌥⇧↑/⌥⇧↓ jump to
  the first/last change in a file".
