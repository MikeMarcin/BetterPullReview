---
schema: "implement-rfc.card.v1"
id: "BPR-1531"
title: "Accept file's shortcut becomes Option-A (Alt+A), replacing Command-Shift-A"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Replace the Accept file shortcut Ctrl+Shift+A (Command-Shift-A on macOS) with Alt+A (Option-A on macOS). Replace, not add.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0007","RFC-BPR-0074"],"related_cards":["BPR-1507","BPR-1515"],"reservation":"docs/issues/_reservations/BPR-1531--BPR-1531--c72af4c619ef.json"}
---

# BPR-1531: Accept file's shortcut becomes Option-A (Alt+A), replacing Command-Shift-A

The owner asked on 2026-10-15 to replace Accept file's key, Command-Shift-A on
macOS and Ctrl+Shift+A elsewhere ([[BPR-1515]]), with Option-A on macOS and
Alt+A elsewhere. The owner chose replace, not add: the old key stops
accepting.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
review bar, whose Accept command ([[BPR-1507]]) carries the key, recorded in
the [workbench specification](../../ux/workbench-specification.md) and the
[review ledger architecture](../../architecture/review-ledger.md). The
receipt semantics of [RFC-BPR-0007](../../rfcs/RFC-BPR-0007-local-review-ledger-actionability.md)
are unchanged: the key still reaches the one retained Accept action, never
Revoke, and does not repeat. The editor rule follows
[RFC-BPR-0074](../../rfcs/RFC-BPR-0074-header-source-navigation.md)'s Option-O:
editable fields keep their native Option/Alt text input.

On a US macOS layout Option-A types `å`. Command-Shift-A typed nothing, so the
old key could only collide with commands; the new one collides with typing.
The key must therefore step aside wherever the reviewer can type, and still
fire everywhere else.

## Acceptance

- AC1: With an eligible current file and the keyboard outside any text
  input, Option-A (Alt+A) delivered through the window system accepts exactly
  the current file and material without opening a menu, including the macOS
  delivery Cocoa makes (shortcut offer with the key `A`, the Option
  modifier and the text `å`). Command-Shift-A (Ctrl+Shift+A) no longer
  accepts. A check against the old binding fails.
- AC2: Every text input the reviewer can type in inside the window keeps
  Option-A: editable single-line fields, multi-line comment, reply, note and
  composer editors, editable combo boxes and the Review Cycle suggestion
  editor. There the key is unbound, it accepts nothing, and the field
  receives `å`. Read-only text keeps the key. A check with the old
  QLineEdit/QTextEdit-only guard fails on the suggestion editor or combo box.
- AC3: Discoverability and collisions: Accept's menu item and the direct
  button's tooltip name the platform key (`⌥A` on macOS, `Alt+A` elsewhere);
  no other action, shortcut or mnemonic in the assembled window, including
  Windows and Linux menu-bar and button mnemonics, uses Alt+A. Eligibility,
  no-repeat and accept-only behaviour from [[BPR-1515]] are unchanged.
- AC4 (human): The owner accepts the frozen candidate: "⌥A accepts the file
  and ⌥A still types å in comment boxes".

## Implementation

Qt presents: the binding and its editor guard live in
`workbench_window::update_accept_file_shortcut`. No core, host, provider or
storage change. Update the ledger course in the application composition, the
Qt key test, and the live documents that name the key. Historical cards,
checkpoints and release notes keep the key they shipped with. Never drive or
capture the owner's desktop; the owner checks the frozen build themselves.
