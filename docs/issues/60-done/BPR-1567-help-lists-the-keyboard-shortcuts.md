---
schema: "implement-rfc.card.v1"
id: "BPR-1567"
title: "Help lists the keyboard shortcuts, from the catalogue the bindings are made from"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-16","owner_note":"Can we add a help page to list keyboard shortcuts.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0005","RFC-BPR-0033","RFC-BPR-0052"],"related_cards":["BPR-0796","BPR-1250","BPR-1328","BPR-1336","BPR-1531","BPR-1532","BPR-1535","BPR-1536"],"reservation":"docs/issues/_reservations/BPR-1567--BPR-1567--f15b85932ade.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"676f61c3a","evidence":{"kind":"command","detail":"qt_keyboard_shortcuts_scale_100/125/200: Help's first item on Ctrl+/ for the window; opened from the menu with search focused; Cmd-/ through the window system reopens the same page; Esc and Close hide it; resident bar item disabled with the same key; no other binding, Qt standard key or text widget takes Ctrl+/."}},{"criterion":"AC2","outcome":"pass","revision":"676f61c3a","evidence":{"kind":"command","detail":"Same rows: every catalogue entry with a key, grouped in order; native text (glyphs on macOS, spoken names); place and Not-while-typing notes; footer rule; search by group, place, portable and native key; no-match message."}},{"criterion":"AC3","outcome":"pass","revision":"676f61c3a","evidence":{"kind":"command","detail":"Drift test enumerates every QAction, QShortcut and explicit button key in the assembled window, resident bar and a Review Cycle view against the catalogue both ways; negative control reports a stray, an unnamed, a changed, a lost and a removed binding and nothing else."}},{"criterion":"AC4","outcome":"pass","revision":"676f61c3a","evidence":{"kind":"command","detail":"Same rows at 100/125/200%: accessible names, Down Arrow into the list, resizable to minimum without clipping, caps tinted from Base/Text in Light, Dark and both High Contrast themes; Qt-only change; full dev suite on the landed tree with only pre-existing/load failures (see card closeout)."}},{"criterion":"AC5","outcome":"pass","revision":"d311422c9","evidence":{"kind":"observation","detail":"Owner Accept, no note, attended session dbc5e14feee4eb197858d4a731274d9f, decision 848abf22-2143-4390-970b-8e132ea00a47 (2026-10-17T04:43:20Z, event 4), candidate BPR-1567-keyboard-shortcuts-v001 fingerprint 3c95542f, frozen root bpr1567-shortcuts-v001.g94p9bpc."}}]
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

## Checkpoint — candidate v001, 2026-10-16

Product candidate `d311422c9ba31304ff43adcca64e89bcf6a0be02` on branch
`bpr-1567-keyboard-shortcuts` (worktree
`.claude/worktrees/bpr-1567-shortcuts`), main merged in at `354b021d4`. Not
merged to main until the owner accepts. Paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1567/handoff.json).

Design. Help → **Keyboard Shortcuts** is Help's first item, on ⌘/
(`Ctrl+/`): the shortcut-list key of Slack and Google's editors. ⌘? is
macOS's Help-menu search, so it was not taken. A non-modal window per
workbench window: search field (focused on open), a grouped list of eleven
areas, each row the command in words with its keys as key caps in native
notation, "or" between alternatives, a muted note for a place ("In the tour
outline") or "Not while typing", and a footer with the Option rule. Caps are
tinted from the list's own Base and Text, so they read in all four themes.
Esc and Close hide it; ⌘/ raises the same one. The resident desktop menu bar
offers the item disabled.

Single source. `src/adapters/qt/src/keyboard_shortcuts.cpp` holds every key
once; all binding sites take their keys from it (window, resident bar, epoch
control, tour editor, strip, Overview, Settings, thread finder, comment card,
Review Cycle view and sheets), and the page is generated from it. Keys read
inside widgets' own key handlers are catalogue entries the enumeration cannot
see; the UX specification says so.

| Acceptance | Evidence at `d311422c9` | Result |
|---|---|---|
| AC1 | `qt_keyboard_shortcuts_scale_{100,125,200}`: Help's first item, Ctrl+/ for the window, opened from the menu with the search focused, ⌘/ through the window system reopens the same page, Esc and Close hide it; the resident bar's item is disabled with the same key; no other binding, no Qt standard key, and neither `QLineEdit` nor `QPlainTextEdit` claims Ctrl+/ | Pass |
| AC2 | Same rows: every catalogue entry with a key here, in order, under group headings; native text (⌘⌥ glyphs on macOS, spoken "Option-Command-Up Arrow"); "In the tour outline" and "Not while typing" notes; footer rule; search by group, place, portable and native key; "No shortcut matches" | Pass |
| AC3 | Same rows: every `QAction`, `QShortcut` and explicit button key in the assembled window, resident bar and a Review Cycle view matches the catalogue and every bound entry is live; negative control reports a stray, an unnamed, a changed, a lost and a removed binding, and nothing else | Pass |
| AC4 | Same rows at 100/125/200%: accessible names, Down Arrow into the list, resizable to its minimum without clipping, caps tinted from Base/Text in Light, Dark and both High Contrast themes; Qt-only change | Pass |
| AC5 human | Session `dbc5e14feee4eb197858d4a731274d9f`, candidate `BPR-1567-keyboard-shortcuts-v001`, fingerprint `3c95542f…`, ready and protected at cursor 3; frozen root `bpr1567-shortcuts-v001.g94p9bpc` | Pending |

Gates: full `scripts/run_test_suite.py --preset dev` at `354b021d4`: 766
passed, 1 failed (`qt_profile_management_window`, the resident Help menu's
new first item; updated in `d311422c9`, passes), 1 skipped
(`application_large_review_cache_bytes`, exit 77). clang-format 22.1.8 clean
and clang-tidy 22 `--warnings-as-errors` passed on all touched translation
units; documentation gate `DOCUMENTATION_OK`. `src/app` is unchanged.

Next: on hand-back, an Accept merges current main into the branch, reruns the
focused rows, lands and finishes; a Reject's note becomes v002.

## Closeout — 2026-10-17

**Owner decision, 2026-10-17T04:43:20Z: Accept**, no note (decision
`848abf22-2143-4390-970b-8e132ea00a47`, event 4 of session
`dbc5e14feee4eb197858d4a731274d9f`), on candidate
`BPR-1567-keyboard-shortcuts-v001` at `d311422c9`, fingerprint
`3c95542f…`. Handed back at event 5; event cursor consumed: 5. The review
server was stopped, the candidate released (event 6) and the frozen root
removed.

Landed on main as `676f61c3a`, after BPR-1560 and BPR-1566 (`e30c62f25`);
both merges were clean. BPR-1566 adds no key binding, so the catalogue needed
no change. It does not list the open label menu's ⌘0 = Plain (a key the menu
reads itself), so the separate card removing that binding changes nothing
here.

Verification on the landed tree `676f61c3a` (`dev`, Debug):
`qt_keyboard_shortcuts_scale_{100,125,200}`, `qt_profile_management_window`,
`qt_workbench_window_scale_{100,125,200}`, `qt_feedback_label_controls`,
`qt_composed_github_scale_100` and `architecture_boundaries` pass 10/10. The
full `scripts/run_test_suite.py --preset dev` gives 767 passed, 2 failed and
1 skipped (`application_large_review_cache_bytes`, exit 77). Of the two
failures, `application_large_review_history` (an epoch-refresh benchmark
budget) passes when run alone. `application_label_authoring_contract`
intermittently runs out its 40 s budget at its base-side draft step
(phase 31, `base-line=2 draft=-`, no draft forms on the base line). That
flake is already on main without this card: at `e30c62f25` it failed 5 of 20
runs, and 4 of 12 with BPR-1567. It is pre-existing and needs its own card.
The composition contract passes (`src/app` is unchanged), and the
documentation gate prints `DOCUMENTATION_OK`.
