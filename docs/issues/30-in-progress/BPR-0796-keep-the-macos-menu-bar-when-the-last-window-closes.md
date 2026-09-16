---
schema: "implement-rfc.card.v1"
id: "BPR-0796"
title: "Keep the macOS menu bar when the last window closes"
status: "In Progress"
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

### What this card decided, and why the RFC's parenthetical did not change

Two resolutions were on the table, and they were not equivalent:

1. **Grow the resident bar to what the RFC names.** A visible File menu (New
   Window belongs there as much as under Profiles; Quit will still merge), and
   View "and the rest" present with their commands disabled, which is what the
   workbench specification already says a window with nothing open shows. This
   is what the RFC's text requires, and the RFC is the accepted design.
2. **Narrow the RFC's parenthetical** to the bar the desktop actually needs, and
   say why one menu is the right answer for a windowless application.

**(1), and the specification the fix was written against is the windowed bar
itself.** RFC-BPR-0033's promise is that the bar *remains*; the sharpest reading
of "remains" a course can hold is that the close subtracts nothing — the same
visible titles, the same items in the application menu AppKit owns, the same
items under View. That is stronger than the parenthetical and it is testable, so
the course asserts it and the parenthetical is left alone.

**A visible File menu was considered and not added.** d8 suggested moving New
Window there so the title would show. That would make the *windowless* bar
carry a menu the *windowed* bar does not, and a File menu appearing on the close
is the same complaint the owner made, in the other direction. File stays as it
is — present in the bar, its only item Quit, its title hidden by AppKit with a
window and without — so nothing about it changes at the close. The RFC's list is
met: File, Profiles and View are all in the bar afterwards, exactly as they were
before.

**What is offered and not available is named, not hidden.** `Settings…` is
offered and disabled with no window, which keeps `Preferences…` in the
application menu where a close used to remove it. It cannot be enabled here:
Settings is a window's window, and there is no profile runtime open to back one.
That is a real gap in what a windowless application can do, it is not something
RFC-BPR-0033 asks for, and no card is opened for it here; it is written down so
the next person finds it rather than rediscovers it.

### What d9 measured, changed and re-measured

**d8's diagnosis was re-run before it was believed.** The unchanged course on
unchanged source, at zero pending build actions, against the executable the
owner reviewed (sha256 `d500ef7a…`): 1/1 passed in 5.46 s, and it printed the
two bars. With a window
`Apple[Preferences...;Services;Hide…;Hide Others;Show All;Quit…] | ~File[] | Profiles[Default;New Window;Manage Profiles…] | View[Line numbers;One pane]`;
after the last close, the same line **without View**, and **without
`Preferences...`** in the application menu — the second loss d8's table did not
name separately. Longest stretch without the Profiles menu: **0 ms**, as d8
reported. `scratchpad/d9-native-baseline-verbose.log`.

**The course fails first on that, and on nothing else.** It now snapshots the
windowed bar and requires the windowless bar to equal it, which is the
comparison the owner's sentence is about:

```
native-menu-bar: windowed   top-level menus: Apple, Profiles, View
native-menu-bar: windowless top-level menus: Apple, Profiles
FAILED: closing the last window takes no menu off the bar
```

on unchanged `src/`, at zero pending build actions
(`scratchpad/d9-course-fails-first.log`). Three assertions stand behind it: the
visible top-level titles, the items of the application menu AppKit owns (which
is how `Preferences…` is checked without hard-coding a title Qt owns), and the
items under View; then the whole `[NSApp mainMenu]` description, equal before
and after.

**The fix.** `qt::desktop_menu_bar` gained the window menu's View — Line
numbers, One pane, a separator and `Settings…` with `QAction::PreferencesRole` —
in the window menu's order, every one disabled and connected to nothing. The
menu bar is presentation, so that is the whole of it in `src/adapters/qt/`.

**Growing it exposed the second defect this card's fourth acceptance bullet
names, and nothing had been able to reach it.** The bar pinned "with Settings
focused" before the close, when the desktop had never built its own bar. Once
the pin was moved after a reopen — the arrangement the bullet actually describes
— it failed: Qt hands a parentless menu bar to any focused window carrying none
of its own, so the desktop's bar replaced the workbench's the moment Settings
took focus, offering that window's own commands greyed while the window was
right behind it. Pre-existing since [[BPR-0777]], and more visible once the bar
had View in it. `src/app/` now retires the desktop's bar when a profile window
becomes **active** — that moment rather than when the window opens, because that
is when the window's own bar takes the macOS menu bar; the course samples
`[NSApp mainMenu]` through both transitions and both report a **0 ms** gap.

**What the fixed bar reads, from AppKit:** identical before and after the close,

```
Apple[Preferences...;Services;Hide…;Hide Others;Show All;Quit…] | ~File[] |
Profiles[Default;New Window;Manage Profiles…] | View[Line numbers;One pane]
```

with `Line numbers` and `One pane` not enabled. `scratchpad/d9-course-after-fix-2.log`.

**Checks, all at zero pending build actions and on the binary that was built.**
`application_profile_empty_desktop_native_menu_bar`, `application_profile_management_public_path`
and `qt_profile_management_window` pass (`scratchpad/d9-decisive-1.log`), inside a
14-course desktop-lifecycle, management and settings selection, 14/14 in 184 s
(`scratchpad/d9-focused-2.log`). The management course had held the desktop's
menu bar across reopenings; the new lifetime makes that a dangling pointer and
it crashed, which is how the lifetime got pinned there too — it fetches the bar
after each close and asserts it steps aside once a window is active. Reviewed
executable `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`,
sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`,
carrying `desktopViewMenu`, `desktopToggleLineNumbersAction` and
`desktopOpenSettingsAction` as UTF-16 literals, so the binary tested is the
binary built.

**The isolation manifest was not re-run**, because this dispatch adds no test
registration and changes no `LOOPBACK_PORT_COUNT`; d8's run of it stands
(`scratchpad/d8-isolation-manifest.log`).

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

**d9 found the same session state and the same limitation.** Every run in this
dispatch reported `screen locked: yes, frontmost: loginwindow`, so the
hide-and-unhide activation gesture is **not run** here too — not merely
inherited as not run. What this leaves open is exactly what d8 left open, and
all of it is the owner's to see rather than a machine's:

- Whether AppKit **repaints** the menu bar of a windowless active application.
  Nothing in this sandbox can answer it: the course reads what `[NSApp mainMenu]`
  *is*, and the owner's complaint was about what they *saw*. It is the first
  question the prepared attended candidate asks.
- The **Dock tile** and a **literal Dock click**. `activationPolicy == Regular`
  is asserted, and `applicationShouldHandleReopen:hasVisibleWindows:NO` — the
  message the Dock sends — is delivered to the real delegate and reopens the
  profile once. The click itself stays with the Dock and with the owner.
- **Native application activation** (hide, then unhide) as an AppKit gesture.
  The course runs it only when the session activates the process and prints
  `not run: native application activation (hide and unhide) needs a session that
  activates this process` otherwise, which is what it printed here.

An unlocked console session on this machine would close the third of these
without an attended verdict. None of it is a reason to hold the fix, and none of
it may be inferred from the course.

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
