---
schema: "implement-rfc.card.v1"
id: "BPR-0796"
title: "Keep the macOS menu bar when the last window closes"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: []
platforms: ["mac"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"attended review e966c88f3320638208adf4fcf0651e31, candidate bpr-0779-empty-desktop-dock, rejected 2026-09-16","acceptance":["B1","B2"]}
---

# BPR-0796: Keep the macOS menu bar when the last window closes

## Outcome

The owner reviewed the shipped macOS build on 2026-09-16 and rejected the
empty-desktop candidate with one sentence: **"Closing last window closes menu
bar."** RFC-BPR-0033 B1 requires the opposite — "on macOS the application stays
resident with its menu bar and Dock icon and no window" — and B2 builds on it,
because a menu bar that is not there cannot offer ⌘N or the Profiles menu.

Make the application's own menu bar survive the close on real macOS, and prove
it with a course that observes the native menu bar rather than the widget tree.
The widget tree is what let this ship: [[BPR-0777]] AC2 inferred the native
behaviour from the fact that `qt::desktop_menu_bar` is parentless, which is
Qt's documented recipe but is not an observation.

## Acceptance

- On real macOS, closing the last window of the last open profile leaves the
  application resident with a usable menu bar carrying File and Profiles, and
  the Dock icon present.
- A course fails before the fix and passes after it, asserting the native
  macOS menu bar — not the existence of the C++ object.
- Application activation, ⌘N and choosing a profile from the Profiles menu each
  still reopen the most recently used profile at Home exactly once, under one
  runtime owner. Whatever part of that is only observable by a person clicking
  the Dock is named rather than claimed.
- The windowed menu bar is unchanged: a parentless menu bar that exists while
  windows are open must not take the menu bar away from a window that has one.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run** at
card authoring.

### What d8 established before this card was written

The suspected mechanism is an ordering one, and it is a hypothesis, not a
finding. In `src/app/src/profile_desktop.cpp`, `present_desktop_menu()` —
which constructs the parentless `qt::desktop_menu_bar` for the first time,
lazily — is only reached from `desktop_emptied()` after `Runtimes.erase()`, and
only once `profile_shutdown_complete()` is true. The window and its
window-parented `QMenuBar` (`src/adapters/qt/src/workbench_window.cpp`,
`PrimaryMenuBar = new QMenuBar{ this }`) are therefore destroyed strictly
*before* any application-level menu bar exists for Qt to fall back to.

Nothing sets `Qt::AA_DontUseNativeMenuBar`, so the native menu bar is genuinely
in use, and `main.cpp` sets `setQuitOnLastWindowClosed(false)`, so the process
is meant to stay resident. Qt is 6.10.3.

The reviewed binary was
`build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`,
sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b`,
source revision `ed2b3f14`.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new
preference is authorized by this card. The core decides, the host performs, Qt
presents: the empty-desktop outcome stays a policy decision and the menu bar
stays presentation. Windows and Linux quit behaviour is [[BPR-0798]] and
[[BPR-0799]], not this card.
