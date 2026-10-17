---
schema: "implement-rfc.card.v1"
id: "BPR-1569"
title: "Plain comment loses its key; ⌘0 in the open label menu is Actual Size"
status: "Done"
rfcs: []
depends_on: ["BPR-1566"]
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-decision","found_by":"Owner note on BPR-1566's Accept (session 1086a4c4, decision 2e35abcc), 2026-10-16, and the owner's correction through the coordinator","governing_rfc":"RFC-BPR-0048","amendment":"RFC-BPR-0048 owner amendment 2 (2026-10-16)","related_cards":["BPR-1080","BPR-1083","BPR-1336","BPR-1535","BPR-1566","BPR-1567"],"reservation":"docs/issues/_reservations/BPR-1569--BPR-1569--0054f0b80620.json","acceptance":["BPR-1569:key","BPR-1569:gates"]}
acceptance: [{"criterion": "BPR-1569:key", "outcome": "pass", "revision": "801b3dc14", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_feedback_label_controls$'", "detail": "test_command_0_is_actual_size_in_the_open_menu, keys through the window system (tests::window_system_key): with the menu open on a labelled comment Command+0 runs the window's Actual Size once and closes the menu, label and draft unchanged; on a plain comment likewise; with Actual Size disabled it only closes the menu; outside the menu, window active, Command+0 is Actual Size; with the window active and the menu open it is Actual Size exactly once and keeps the label; Plain names no key and is reached by arrows and Return and by click. Negative control: the old Command+0 = Plain fails at '[[BPR-1569]] Command+0 in the menu closes it and leaves the label'. qt_conversation_cards_view, qt_composed_github/inline drawer, qt_workbench_window x3, application_diff_zoom_public_path and application_label_authoring_contract pass."}}, {"criterion": "BPR-1569:gates", "outcome": "pass", "revision": "801b3dc14", "evidence": {"kind": "command", "command": "full dev ctest on the merged tree; clang-format 22; clang-tidy; verify_documentation.py", "detail": "clang-format 22 and clang-tidy clean on feedback_label_controls.cpp; DOCUMENTATION_OK; the full dev ctest on the tree merged with main before landing (recorded in the landing commit)."}}]
---

# BPR-1569: Plain comment loses its key; ⌘0 in the open label menu is Actual Size

## Owner decision

Owner, accepting [[BPR-1566]] on 2026-10-16, verbatim: "I'm really not sure
why plan comment gets a hotkey, I think we can remove that." Then, through the
coordinator: once the binding is removed, ⌘0 should behave like ⌘0 everywhere
else, View › Actual Size, even with the label menu open; closing the menu is
fine; it must never pick Plain. Recorded as
[RFC-BPR-0048 owner amendment 2](../../rfcs/RFC-BPR-0048-feedback-authoring-native-github-integration.md#owner-amendment-2-2026-10-16-plain-has-no-key-0-is-actual-size).
No visual review: the coordinator relayed that this merges once tests prove
it.

## Design

- Plain comment stays the menu's first item, without "⌘0" beside it, in its
  tooltip ("No label") or in its accessible description ("Removes the label").
- The menu is a popup that holds the keyboard, so whether a window shortcut
  is reached past it depends on the window system. The menu therefore still
  claims ⌘0, closes, and triggers its window's enabled action bound to ⌘0
  (the workbench's Actual Size). A disabled Actual Size (already at Code
  size) does nothing beyond closing the menu, as ⌘0 does elsewhere.
- [[BPR-1567]] (Keyboard Shortcuts, on `bpr-1567-keyboard-shortcuts`, waiting
  for the owner) lists ⌘L for the label menu and ⌘0 as Actual Size, and no
  ⌘0 = Plain entry, so its catalogue needs no change for this card.

## Acceptance

- **BPR-1569:key** Keys delivered through the window system to the open
  menu: ⌘0 runs the window's Actual Size once and closes the menu, with the
  label and draft unchanged on a labelled and on a plain comment (negative
  control for the removed key); a disabled Actual Size does nothing; outside
  the menu ⌘0 is Actual Size; Plain is reached by arrow keys and Return and
  by click; Plain names no key.
- **BPR-1569:gates** Focused rows, clang-format 22, clang-tidy, the
  documentation gate and the full dev ctest before landing.

## Evidence

Both criteria pass (front matter). No owner review by the owner's instruction (no visual change beyond Plain's key text). Landed after one full dev ctest on the tree merged with main.
