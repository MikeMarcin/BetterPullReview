---
schema: "implement-rfc.card.v1"
id: "BPR-0796"
title: "Keep the macOS menu bar when the last window closes"
status: "Ready"
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

d8 built the native course this criterion always needed and ran it. **The bar
is not lost.** What is lost is most of its titles: after the close the reviewer
is left with the Apple menu, the application menu and `Profiles`, where
RFC-BPR-0033 promises "File, Profiles, View and the rest". The evidence is
below; read it before choosing a fix.

The widget tree is what let this ship: [[BPR-0777]] AC2 inferred the native
behaviour from the fact that `qt::desktop_menu_bar` is parentless, which is
Qt's documented recipe but is not an observation.

## Acceptance

- On real macOS, closing the last window of the last open profile leaves the
  application resident with the menu bar RFC-BPR-0033 names — not merely *a*
  menu bar — and the Dock icon present. If the RFC's parenthetical is the thing
  that should change, change it and say why, rather than quietly meeting a
  weaker bar.
- `application_profile_empty_desktop_native_menu_bar` asserts the new contract
  on `[NSApp mainMenu]`, and fails before the change. The course already exists
  and already asserts the 0 ms gap; extend it, do not replace it.
- Application activation, ⌘N and choosing a profile from the Profiles menu each
  still reopen the most recently used profile at Home exactly once, under one
  runtime owner. The Dock tile, a literal Dock click and whether AppKit repaints
  a windowless active application's bar stay human-observable and are named,
  never claimed.
- The windowed menu bar is unchanged: a parentless menu bar that exists while
  windows are open must not take the menu bar away from a window that has one.
  The course pins this with Settings focused.
- The owner accepts the result on a real macOS build through the attended-review
  workflow. Their rejection of 2026-09-16 cannot be cleared any other way, and
  [[BPR-0782]] B5 stays unmet until it is.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run** at
card authoring.

### What d8 established, natively

d8 built the instrument this criterion always needed and ran it on the owner's
own machine: `application_profile_empty_desktop_native_menu_bar`
(`tests/end_to_end/profile_empty_desktop_native_menu_bar_tests.cpp` with the
AppKit probe beside it), which runs the real desktop composition on the cocoa
platform and reads `[NSApp mainMenu]` on every event-loop turn of the close.
It passes at zero pending build actions against unchanged source
(`git log ed2b3f14..HEAD -- src/` is empty), so what follows describes the build
the owner rejected.

**The lazy-construction hypothesis is refuted.** `present_desktop_menu()` does
construct the parentless bar only after `Runtimes.erase()` deletes the window
and its `QMenuBar`, and Qt's own log shows the predicted husk -
`~QCocoaMenuBar` running `updateMenuBarImmediately()` with `findGlobalMenubar()`
still null. But both happen inside one `poll()` call, with no event-loop turn
between them, so AppKit is never asked to draw it. **Longest stretch without the
Profiles menu during the close: 0 ms.** The settled main menu is exactly
`desktop_menu()->toNSMenu()`.

**What is actually wrong is what the bar contains.** Read straight off AppKit in
the same run:

| | `[NSApp mainMenu]` |
|---|---|
| with a window | `Apple[Preferences…;Services;Hide…;Quit…]` \| `~File[]` \| `Profiles[Default;New Window;Manage Profiles…]` \| `View[Line numbers;One pane]` |
| after the last close | `Apple[Services;Hide…;Quit…]` \| `~File[]` \| `Profiles[Default;New Window;Manage Profiles…]` |

`~` marks a menu AppKit hides. So **File is never visible on macOS at all**, with
a window or without: its only item is Quit, and AppKit merges Quit into the
application menu and leaves the File title empty. And closing the last window
takes **View** away, and `Preferences…` with it.

RFC-BPR-0033 says, in its own words: "the menu bar remains (**File, Profiles,
View and the rest**), the Dock icon remains." What actually remains is the Apple
menu, the application menu and **Profiles**. One menu where the RFC promises
four. A reviewer who closes their last window watches the bar lose most of its
titles, and "closing last window closes menu bar" is a fair description of that.

**So the owner's rejection is accounted for, and B1 is not met as written.** The
defect is not that the bar is lost; it is that the bar the composition offers a
windowless application is not the menu bar RFC-BPR-0033 specifies.

### What this card has to decide

Two resolutions, and they are not equivalent:

1. **Grow the resident bar to what the RFC names.** A visible File menu (New
   Window belongs there as much as under Profiles; Quit will still merge), and
   View "and the rest" present with their commands disabled, which is what the
   workbench specification already says a window with nothing open shows. This
   is what the RFC's text requires, and the RFC is the accepted design.
2. **Narrow the RFC's parenthetical** to the bar the desktop actually needs, and
   say why one menu is the right answer for a windowless application.

The RFC's authority points at (1). d8 did not implement it: it is a design
change to what a reviewer sees, it cannot be attended-verified in the dispatch
that makes it, and this was a closeout milestone. Whoever takes it should carry
the owner a rendered before-and-after rather than a diff.

### What is still not observable here

The console session was **screen-locked** while the course ran
(`CGSSessionScreenIsLocked = 1`, frontmost `loginwindow`), so no window could
become key and no activation API would activate the process. The course detects
that, delivers the two key-window transitions through the same QPA entry point
the cocoa plugin uses, and records the hide-and-unhide activation gesture as
**not run**. Everything downstream of that - Qt's cocoa arbitration and AppKit's
`mainMenu` - is real. Two things remain genuinely human-only: whether AppKit
*repaints* the menu bar of a windowless active application, and the Dock tile
and a literal Dock click. `activationPolicy == Regular` is checked, which is
what buys the tile.

Note for whoever runs this course on an unlocked session: it calls
`activateIgnoringOtherApps:` and will take focus from whatever the owner is
doing.

### Evidence paths

- Native run, unfixed source: `scratchpad/d8-native-unfixed-source-run2.log`
  (decisive lines 118-137); integrator's own re-run
  `scratchpad/d8-int-native-menubar.log`, 1/1 passed at 0 pending build actions.
- Full Qt `qt.qpa.menus` trace of the close: `scratchpad/d8-native-before-fix.log`.
- Isolation manifest, run because this adds a registration:
  `scratchpad/d8-isolation-manifest.log` - fails on the `.claude/worktrees`
  checkout's duplicate `add_test` ownership and on `gh_cli_git_smart_transport`,
  as before, and names nothing from this course.
- Reviewed executable `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`,
  sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b`,
  source `ed2b3f14`.

### A candidate that was considered and not applied

Calling `present_desktop_menu()` before `Runtimes.erase()` at
`src/app/src/profile_desktop.cpp:833` would remove even the intra-tick husk.
Constructing the bar eagerly at desktop start was **rejected**: a parentless bar
takes the menu bar whenever a menubar-less window is focused, and the course
proves today's behaviour correctly keeps the workbench bar while Settings has
focus. Neither was applied, because no observable either would change was
demonstrated.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new
preference is authorized by this card. The core decides, the host performs, Qt
presents: the empty-desktop outcome stays a policy decision and the menu bar
stays presentation. Windows and Linux quit behaviour is [[BPR-0798]] and
[[BPR-0799]], not this card.
