---
schema: "implement-rfc.card.v1"
id: "BPR-1535"
title: "Home moves to Command-Shift-H (Ctrl+Shift+H) and Actual Size to Command-0 (Ctrl+0)"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Move Home from Ctrl+0 (Command-0) to Ctrl+Shift+H (Command-Shift-H), and move Actual Size (reset code zoom) from Ctrl+Alt+0 (Command-Option-0) to Ctrl+0 (Command-0). Command-backtick was considered and rejected: macOS uses it to cycle windows.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0032","RFC-BPR-0052"],"related_cards":["BPR-0650","BPR-1336","BPR-1080"],"reservation":"docs/issues/_reservations/BPR-1535--BPR-1535--e5f2afb941c7.json"}
---

# BPR-1535: Home moves to Command-Shift-H and Actual Size to Command-0

The owner asked on 2026-10-15 to move Home from Command-0 (`Ctrl+0`) to
Command-Shift-H (`Ctrl+Shift+H`), and to give Command-0 to Actual Size, the
comparison's zoom reset ([[BPR-1336]]), which had Command-Option-0
(`Ctrl+Alt+0`) because Command-0 was Home. Command-backtick was considered
for Home and rejected: macOS cycles an application's windows with it.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
AC-06, the Home button in the strip ([[BPR-0650]]), recorded in the
[workbench specification](../../ux/workbench-specification.md); Home's
return row from [RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md);
and the zoom commands of [[BPR-1336]] in the specification and
[appearance and accessibility](../../ux/appearance-and-accessibility.md).
Inside the open label menu Command-0 stays Plain comment ([[BPR-1080]]).

## Acceptance

- AC1: Command-Shift-H (`Ctrl+Shift+H`) delivered through the window system
  shows Home from Review and from the other review surfaces, also with the
  keyboard in a comment editor, and Command-0 no longer shows Home. Home's
  tooltip and the Back to the review row name the new key in the
  platform's notation. A check against the old binding fails.
- AC2: Command-0 (`Ctrl+0`) is Actual Size in the window and on the
  resident desktop menu bar, resets a moved zoom, and does nothing at 100%.
  The window has no Command-Option-0 binding afterwards (replace, not add).
  The Diff zoom row in Settings names Command-0. Inside the open label menu
  Command-0 is still Plain and outside it is the window's.
- AC3: No other action, widget shortcut or standard text-editing key in the
  assembled window takes `Ctrl+Shift+H` or `Ctrl+0`; neither is a macOS
  system or menu key the application would collide with.
- AC4 (human): The owner accepts the frozen candidate: "⌘⇧H goes Home and
  ⌘0 resets zoom".

## Implementation

Qt presents: the bindings live in `workbench_window`, the resident
`desktop_menu_bar`, the Home landing's return row and the appearance
panel's zoom hint. No core, host, provider or storage change. Historical
cards, checkpoints, design frames and release notes keep the keys they
shipped with. Never drive or capture the owner's desktop; the owner checks
the frozen build themselves.
