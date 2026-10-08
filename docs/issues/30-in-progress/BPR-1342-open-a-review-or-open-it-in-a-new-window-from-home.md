---
schema: "implement-rfc.card.v1"
id: "BPR-1342"
title: "Open a review, or open it in a new window, from Home and its row menu"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1342--BPR-1342--e04e479ee80b.json","related_cards":["BPR-1338","BPR-1017","BPR-0987","BPR-0776","BPR-0736"],"related_rfcs":["RFC-BPR-0024","RFC-BPR-0026","RFC-BPR-0033"]}
---

# BPR-1342: Open a review, or open it in a new window, from Home and its row menu

## Owner notes

Owner, chat, 2026-10-08, verbatim: "I should be able to open a review and open
a review in a new window from the home, including from right click on a pr".

## What exists (inventory at `7e0c3e1a5`)

- **Queue rows.** A click, or Return/Enter on the current row
  (`home_list::eventFilter`, [[BPR-0736]]), opens the row's pull request in
  *this* window on Overview (`workbench_window::open_pull_request_item`). It
  replaces whatever pull request the window had; a row naming the pull request
  the window already shows returns to it without reading again ([[BPR-0987]]).
  Return with any modifier opens the same way. Middle-click and ⌘-click had no
  gesture of their own.
- **Recently viewed rows.** Activation or Return opens on Overview in this
  window; Shift+Return resumes the saved reading
  (`workbench_window::dispatch_recent_review`).
- **Row menus** ([[BPR-1338]]). A queue row's right click shows one item, Open
  on GitHub. A Recently viewed row's menu (`Qt::ActionsContextMenu`) shows
  Resume and Open on GitHub. Neither offers opening the review, and no menu
  item shows a key.
- **New windows.** Profiles › New Window (and ⌘N on the empty desktop's bar)
  dispatches `application::new_bpr_window_intent` to the profile desktop, which
  asks the profile management host for a fresh window of the same profile
  under its one runtime owner (RFC-BPR-0024). The new window opens Home
  ([[BPR-1017]]: already signed in, with the queue). Nothing carries a pull
  request into it; there are no tabs (RFC-BPR-0026's tabs were superseded by
  the Home direction).
- **The same pull request in two windows.** RFC-BPR-0024 allows it: "Two
  windows in one profile may show the same PR at different epochs or
  positions", and "an explicit New Window request always creates another
  window".

## Scope

- Home's row menu, for queue rows and Recently viewed rows, holds **Open
  Review** (the default, in this window), **Open Review in New Window**, Resume
  (Recently viewed only), then Open on GitHub. Each item shows its key.
- Keys on a current row: Return opens; ⌘Return (Ctrl+Return off macOS) opens in
  a new window; ⇧Return still resumes a Recently viewed row. ⌘-click and
  middle-click on a row open it in a new window.
- The new window is the existing New Window path in the same profile: the same
  intent, the same management host reservation and the one runtime owner. The
  request carries one admitted destination; the new window opens Home and then
  opens that pull request through its Home bar's own path, on Overview.
- **Same pull request already open elsewhere: a second view.** Open Review in
  New Window always opens another window, even when this or another window
  shows that pull request, because RFC-BPR-0024 says an explicit New Window
  always creates one and two windows may show the same pull request at
  different positions. Open Review in this window keeps [[BPR-0987]]: a window
  already showing it returns to it without reading again.

## Acceptance

- `BPR-1342:exists`: the card records how Home opened a pull request before.
- `BPR-1342:menu`: a queue row's and a Recently viewed row's menu offer Open
  Review (default) and Open Review in New Window with their keys, before Open
  on GitHub; each names the row it was opened on.
- `BPR-1342:keys`: Return opens, ⌘Return opens in a new window, ⌘-click and
  middle-click open in a new window, on queue and Recently viewed rows.
- `BPR-1342:composition`: through the real profile desktop, a new window of the
  same profile opens the row's pull request; no second profile owner, the first
  window keeps its view; a row on another host or account is refused.
- `BPR-1342:same-pr`: with the pull request open in the first window, Open
  Review in New Window opens a second view of it and leaves the first as it was.
- Each test fails against the code before the change. The round-trip
  composition pins are resealed.
- `BPR-1342:owner-look`: the owner accepts "open review from Home" in one short
  attended session on a frozen build.
