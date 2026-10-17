---
schema: "implement-rfc.card.v1"
id: "BPR-1567"
title: "Help lists the keyboard shortcuts, from the catalogue the bindings are made from"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-16","owner_note":"Can we add a help page to list keyboard shortcuts.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0005","RFC-BPR-0033","RFC-BPR-0052"],"related_cards":["BPR-0796","BPR-1250","BPR-1328","BPR-1336","BPR-1531","BPR-1532","BPR-1535","BPR-1536"],"reservation":"docs/issues/_reservations/BPR-1567--BPR-1567--f15b85932ade.json"}
---

# BPR-1567: Help lists the keyboard shortcuts

The owner asked on 2026-10-16: "Can we add a help page to list keyboard
shortcuts." Many keys landed in the last weeks (the change and file arrows of
[[BPR-1328]], zoom of [[BPR-1336]], ⌥A of [[BPR-1531]], First and Last change
of [[BPR-1532]], ⌘⇧H of [[BPR-1535]]), some scoped to one widget, and on
macOS the Option+character keys step aside while the keyboard is in a text
field ([[BPR-1536]]). Today the only lists are the menus, the tooltips and the
table in the [workbench specification](../../ux/workbench-specification.md).

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
review workbench and its keys, the window's menu bar and Help menu
([[BPR-1250]]), and the resident desktop menu bar that mirrors the window's
menus disabled ([[BPR-0796]], [RFC-BPR-0033](../../rfcs/RFC-BPR-0033-profile-chooser-and-the-last-window.md)).

## Acceptance

- AC1: Help → Keyboard Shortcuts, and ⌘/ (`Ctrl+/`), open one Keyboard
  Shortcuts window for the workbench window; a second press raises it. Esc
  and Close close it. The resident desktop menu bar offers the item disabled.
  `Ctrl+/` is bound to nothing else in the assembled window, is no Qt standard
  key on the platform, and no text widget claims it.
- AC2: The window lists the shortcuts grouped by area, in the platform's
  notation (`QKeySequence::NativeText`: ⌘⌥⇧ glyphs on macOS, `Ctrl+Alt+…`
  elsewhere), with a note for each key scoped to one place ("In the tour
  outline") and for the keys that step aside while typing, and states the
  macOS Option rule. A search field filters by command, place or key, and
  says when nothing matches.
- AC3: Single source of truth. The keys of the window's and resident menu
  bar's actions, widget shortcuts and buttons are created from one catalogue,
  and the page is generated from that catalogue. A test enumerates every
  `QAction`, `QShortcut` and explicit button shortcut with a key in the
  assembled window, the resident menu bar and the Review Cycle view and fails
  if one is missing from the catalogue, carries other keys, or a catalogue
  entry has no live binding. A negative control (an extra bound action, a
  changed key, a removed binding) is reported.
- AC4: The window is keyboard navigable and accessible (named search field,
  list and rows with spoken key names), resizable, follows palette, font and
  theme changes, and renders at 100%, 125% and 200%. Pure Qt presentation: no
  core, host or effect change.
- AC5 (human): The owner accepts the frozen candidate: "Help → Keyboard
  Shortcuts (⌘/) lists the app's shortcuts, grouped and searchable", with
  renders in Light and Dark.

## Implementation

Qt presents: a static catalogue in the Qt adapter (keys, group, title, place,
the objects bound with them), the binding sites taking their keys from it,
and a `keyboard_shortcuts_window` reading it. No core, application, provider
or storage change. Never drive or capture the owner's desktop; the owner
checks the frozen build themselves.
