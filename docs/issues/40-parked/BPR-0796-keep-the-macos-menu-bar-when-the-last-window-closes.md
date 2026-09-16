---
schema: "implement-rfc.card.v1"
id: "BPR-0796"
title: "Keep the macOS menu bar when the last window closes"
status: "Parked"
rfcs: ["RFC-BPR-0033"]
depends_on: []
platforms: ["mac"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"attended review e966c88f3320638208adf4fcf0651e31, candidate bpr-0779-empty-desktop-dock, rejected 2026-09-16","acceptance":["B1","B2"]}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 - On real macOS the last close leaves the application resident with the menu bar RFC-BPR-0033 names, not merely a menu bar, and the Dock icon present; if the RFC's parenthetical is what should change, change it and say why","outcome":"pass","revision":"1c40d04b","evidence":{"kind":"command","detail":"Read from AppKit in the process under test by application_profile_empty_desktop_native_menu_bar on the cocoa platform, at zero pending build actions, against executable sha256 1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d. With a window and after the last close, [NSApp mainMenu] reads the same line: Apple[Preferences...;Services;Hide;Hide Others;Show All;Quit] + ~File[] + Profiles[Default;New Window;Manage Profiles...] + View[Line numbers;One pane], with Line numbers and One pane not enabled. The course asserts three separate equalities - the visible top-level titles, the items of the application menu AppKit owns, the items under View - and then the whole description, so the close is required to subtract nothing a reviewer can see. Before the fix the same course printed 'windowed top-level menus: Apple, Profiles, View' against 'windowless top-level menus: Apple, Profiles'. THE RFC'S PARENTHETICAL WAS NOT CHANGED, and the card says why: File, Profiles and View are all in the bar afterwards; AppKit hides the emptied File title identically with a window and without, so nothing about File changes at the close. A visible File menu carrying New Window was considered and rejected - it would make the windowless bar carry a menu the windowed bar does not, which is the owner's complaint in the other direction. LIMITATION: the Dock TILE itself is human-only; what the course asserts is [NSApp activationPolicy] == NSApplicationActivationPolicyRegular, which is what buys the tile, plus zero AppKit windows on screen. Whether AppKit REPAINTS the bar of a windowless active application was human-only for d8 and d9 and is now ANSWERED, on an unlocked console, by photograph rather than by inference: with the application active, the menu bar strip captured with a window open and the same strip captured after the last window closed are the same PNG, sha256 5bb204b1972d673125b18cc80beda8c454a682f104bc6050feddb6080d7d7858 (scratchpad/d10/cap-a-windowed-bar.png and cap-b-windowless-bar.png, taken with screencapture against the pinned bundle). AppKit repaints it, and it reads Apple, BetterPullReview (tests), Profiles, View in both states. Logs: scratchpad/d9-native-baseline-verbose.log (before, unchanged source), scratchpad/d9-course-fails-first.log (the failure), scratchpad/d9-course-after-fix-2.log (after)."}},{"criterion":"AC2 - application_profile_empty_desktop_native_menu_bar asserts the new contract on [NSApp mainMenu] and fails before the change; the existing course is extended, not replaced","outcome":"pass","revision":"1c40d04b","evidence":{"kind":"command","detail":"The existing course was extended in place; its 0 ms gap assertion, its Settings pin, its four reopening gestures and its Quit are all still there. Added: a snapshot of the windowed bar (visible titles, application-menu items, View items, full description) and the requirement that the windowless bar equal it; a probe that reads those three lists from AppKit (main_menu_visible_titles, main_menu_items, main_menu_item_is_enabled); gap sampling across each reopening as well as each close. IT FAILED FIRST, on unchanged src/ at zero pending build actions, at exactly the defect: 'FAILED: closing the last window takes no menu off the bar', having printed 'windowed top-level menus: Apple, Profiles, View' and 'windowless top-level menus: Apple, Profiles' (scratchpad/d9-course-fails-first.log). After the fix it passes in 6.4 s (scratchpad/d9-course-after-fix-2.log, scratchpad/d9-decisive-1.log). No new test registration was added and no LOOPBACK_PORT_COUNT changed, so test_network_isolation_manifest was not re-run; d8's run of it stands. d10 EXTENDED THE SAME COURSE AGAIN, on the first console that could run all of it. Two things. (1) It now pins the half of the owner's sentence the main menu is blind to: a close watched while this process is the active application must leave it active, counted over every close the course watches, so a person clicking another application mid-run cannot fail it while a close that really resigned activation cannot pass. Not vacuous, twice over: injecting a hide 40 ms into the first close fails at that exact message, and making every close resign activation fails at the summary one (scratchpad/d10/d10-course-nonvacuity.log, d10-nonvacuity2.log). (2) The hide-and-unhide arm, which had NEVER RUN because every earlier session was screen-locked, ran for the first time and FAILED at unhiding activates it again - and the assertion was wrong, not the application: -[NSApplication unhide:] restores windows and does not take activation, and a windowless application has no windows to restore. A plain AppKit application measured beside it stays inactive after unhide: four runs out of four, and stays inactive after activateIgnoringOtherApps: as well (scratchpad/d10/appkit-reference.log). The arm now asks for activation and records whether macOS granted it, which it does intermittently. Green five runs out of five, 3.99-6.05 s (scratchpad/d10/d10-stab1..5.log), with the app binary still sha256 1f60b0a2..., and focus() now falls back to the QPA delivery when the console is taken away mid-run instead of failing on it."}},{"criterion":"AC3 - Activation, Command-N and the Profiles menu each still reopen the most recently used profile at Home exactly once under one runtime owner; the Dock tile, a literal Dock click and AppKit repaint are named, never claimed","outcome":"pass","revision":"1c40d04b","evidence":{"kind":"command","detail":"On the fixed source, at AppKit level: applicationShouldHandleReopen:hasVisibleWindows:NO delivered to the real application delegate, Command-N synthesized as an NSEvent through -[NSApplication sendEvent:] with no key window, and performActionForItemAtIndex: on the native Profiles - Default item each reopen Default at Home, with exactly one window under one runtime owner, and each further close returns the resident bar. Quit from the merged application-menu item quits the desktop. Each reopening is now sampled in both directions and reports a 0 ms gap, which is new: the desktop's bar is retired when a window becomes active, so the handover is a real transition rather than a no-op. Offscreen, application_profile_management_public_path proves the same three gestures including the Qt::ApplicationActive activation signal, and now also pins that the desktop's menu bar steps aside once a window is active. NOT RUN, and recorded as not run: the native hide-and-unhide activation gesture - this console session was screen-locked for every d9 run (CGSSessionScreenIsLocked = 1, frontmost loginwindow), exactly as it was for d8, so the course delivers the two key-window transitions through the QPA entry point the cocoa plugin itself uses and prints 'not run: native application activation (hide and unhide) needs a session that activates this process'. d10 ran all of it on an unlocked console against the pinned bundle and photographed the result: a literal Dock-tile click reopens exactly one window at Home, Command-N does, the native Profiles item does, Quit from the application menu quits, and the windowless bar is repainted and identical. The hide-and-unhide gesture is NOT the same as activation and is recorded separately below. What stays human-only is now only what the owner did on their own console."}},{"criterion":"AC4 - The windowed menu bar is unchanged: a parentless menu bar that exists while windows are open must not take the menu bar away from a window that has one; the course pins this with Settings focused","outcome":"pass","revision":"1c40d04b","evidence":{"kind":"command","detail":"THIS WAS A REAL DEFECT AND IT IS FIXED. The course pinned Settings focus only BEFORE the first close, when the desktop had never built its bar, so the pin could not fail. Moved to after a reopening - the arrangement this criterion actually describes - it failed: Qt hands a parentless menu bar to any focused window carrying none of its own (QCocoaMenuBar::updateMenuBarImmediately takes the global bar and only overrides it when the focused window has its own), so the desktop's bar replaced the workbench's the moment the menubar-less Settings window took focus, offering that window's own commands greyed while the window was right behind it. Pre-existing since BPR-0777 and more visible once the bar had View in it. profile_desktop now retires the desktop's menu bar when a profile window becomes ACTIVE - that moment rather than when the window opens, because that is when the window's own bar takes the macOS menu bar - and present_desktop_menu() builds it again when the desktop empties. Both pins now hold: with Settings focused before any close, and with Settings focused after a reopening, [NSApp mainMenu] is the workbench window's own bar. No workbench menu was added, removed or reworded; qt_workbench_window at all four scales, qt_settings_window at all four scales and qt_settings_effect_boundary pass unchanged."}},{"criterion":"AC5 - The owner accepts the result on a real macOS build through the attended-review workflow; the rejection of 2026-09-16 cannot be cleared any other way and BPR-0782 B5 stays unmet until it is","outcome":"pending","revision":"1c40d04b","evidence":{"kind":"observation","detail":"REJECTED a second time, on this build, and adjudicated. The owner opened bpr-0796-empty-desktop-menu-bar, launched the pinned binary at 07:05:48 PDT and checked its digest at 07:06:08 (their own shell history records both commands verbatim against the candidate's path), and at 14:11:41Z rejected it with: When I close the last window it stays running in the bottom dock but not in the top menu bar. The session is reconciled with their export and now holds 18 of 18 decided at sequence 81. WHAT THAT NOTE TURNED OUT TO DESCRIBE, settled on an unlocked console rather than argued: the fix is right and their eyes were right, about different things. [NSApp mainMenu] is this process's property and reads the same whoever is active; the bar drawn across the top of the screen belongs to the ACTIVE application alone. With the application active, the close changes nothing a person can see - the menu bar strip photographed with a window open and after the close is the same PNG, sha256 5bb204b1972d673125b18cc80beda8c454a682f104bc6050feddb6080d7d7858. Close the last window while the application is NOT active, which a click on an inactive window's red close button does without activating anything, and the report is exact: the process is alive, it has no window, its Dock tile is there, and the menu bar on screen is the other application's (scratchpad/d10/i2-after-clickthrough.png). NOT A REGRESSION OF THIS PACK and not peculiar to this application: a plain AppKit application built for the comparison behaves identically. What is now prepared is a replacement candidate that carries those photographs, tells the owner how to tell whether this application is the front one, and asks the one question only they can answer - whether it was. This criterion, RFC-BPR-0033 B5 and therefore BPR-0782 stay open until they answer it."}}]
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

**The owner rejected the fix too**, on the same day, with a second sentence:
"When I close the last window it stays running in the bottom dock but not in the
top menu bar." That one is not about the menus. Read
[d10](#d10-the-second-rejection-and-what-it-turned-out-to-describe) before this
card's earlier sections, which were written while the menu bar could not be
photographed and the difference between "our bar" and "the bar on screen" had no
instrument: only the *active* application's menu bar is drawn, and everything
below was measured on `[NSApp mainMenu]`, which is true of this process whoever
is active.

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

**d10 had that unlocked session, and this list is now shorter than it reads.**
The repaint is photographed and identical with and without a window; the Dock
tile was clicked for real and reopens one window at Home; the hide-and-unhide
gesture ran for the first time and turned out to be asserting something untrue of
AppKit rather than something untrue of this application. What remains human-only
is not a property of the build at all — it is what the owner did on their own
console, which nothing here can recover.

### The one prepared attended candidate — performed by the owner, and rejected

Prepared in d9 and **deliberately not performed** by it: a dispatch that changes
what a reviewer sees does not get to accept it, and the mandate asked for
preparation rather than performance. **The owner performed it on 2026-09-16 and
rejected it**; the record below is what they were given, kept as it was written,
and the rejection and its adjudication are in
[d10](#d10-the-second-rejection-and-what-it-turned-out-to-describe). Two things
in it were wrong and are not repeated in the replacement: it told the owner a
screenshot was impossible, which stopped being true the moment the console was
unlocked, and step 8 asked them to check that View's items were **enabled** with
Settings focused, when at Home they are disabled with any window focused,
because nothing is open for them to act on.

- **Candidate** `bpr-0796-empty-desktop-menu-bar`, fingerprint
  `f9c7cd40eca353118cfb94779ace743f57f35e21386d079f7ce3a629cb550b07`
  (the second fingerprint; see the correction below).
- **Session** `e966c88f3320638208adf4fcf0651e31`, the pack's existing one, state
  directory `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review`,
  sequence 80. 18 candidates, 17 decided, **1 pending** — this one. The seventeen
  verdicts of 2026-09-16 are preserved untouched, including the rejection this
  candidate answers, which stays bound to the id and digest it was given on.
- **Bound to** revision `1c40d04b / app sha256 1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`,
  and it protects `desktop_menu_bar.cpp`/`.hpp`, `profile_desktop.cpp`, the course
  and the probe, plus the built bundle as a versioned resource, so a rebuild
  conflicts rather than silently moving what was reviewed.
- **To open it, serve the session:**
  `python3 .agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review serve`
  and open the complete loopback URL it prints.
- **Offline fallback, already written:**
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review/bpr-0796-review.html`,
  64 KB, self-contained. Decide in the page, use its Copy JSON or Download JSON,
  then feed it back with `import --file FILE` (or `--file -` for stdin) on the
  same state directory. Browser storage is a convenience; the import is the
  record.
- **What it asks.** One question: with the last window closed, does the menu bar
  still carry everything it carried with a window open — Profiles and View, and
  `Preferences…` in the application menu — and do a Dock click, ⌘N and the
  Profiles menu each reopen the most recent profile at Home exactly once? The
  body carries the AppKit before-and-after as text, a nine-step script ending in
  the Settings-focus check, the launch command with every path absolute, and an
  explicit instruction to reject and name the step.
- **What it excludes, in its own words.** Windows and Linux ([[BPR-0798]],
  [[BPR-0799]]), and [[BPR-0797]]'s tests-enabled composition defects, so a
  verdict cannot be confounded by something this card did not touch.
- **Why there is no screenshot.** The macOS menu bar is AppKit's, this console
  has been locked for every run, and a rendering of our own `QMenuBar` would be a
  mockup of the exact thing in question. The candidate says so rather than
  leaving the absence to be read as an oversight.
- **Preparation evidence recorded on the candidate:** the course re-run through
  `ctest --preset dev` exiting 0, and the launch binary's digest checked against
  the declared revision with `ninja -n` reporting no work to do.

### Corrections after an independent review

An `rfc-reviewer` was asked to falsify eleven claims about this dispatch and
upheld all of them, one with a caveat. It found four things worth acting on, and
three of them were wrong in text a person will read:

1. **The attended candidate contradicted itself, in the script the owner
   follows.** Its body says File is hidden with a window and without; step 8 then
   told the owner to check "File, Profiles, View with its items ENABLED". A
   reviewer following that literally would look for a File title that macOS never
   draws and could reject on it. Corrected: step 8 now names Profiles and View,
   says File stays hidden there too, and the candidate was released, re-written,
   re-prepared and re-observed. Nothing had been decided, so no verdict moved; the
   fingerprint changed from `8365ee84…` to `f9c7cd40…`.
2. **"B1's macOS half is met" claimed slightly more than the evidence.** B1's own
   words are "stays resident with its menu bar **and Dock icon**". What the course
   proves is `[NSApp activationPolicy] == NSApplicationActivationPolicyRegular`
   and zero AppKit windows on screen. That is what buys the tile, and it is as
   close as a machine gets, but seeing the tile and clicking it are the owner's.
   The summary sentences on [[BPR-0782]] and in the RFC index now say so where
   they previously read unconditionally; AC1's own record already disclosed it.
3. **The formatting sentence implied coverage it did not have** for the
   Objective-C probe. Corrected above.
4. **A hardening observation, deliberately not acted on.** `retire_desktop_menu()`
   is reached from one branch: the `QEvent::WindowActivate` arm, under
   `!Focusing && current(context)`. A profile window that became visible without
   ever delivering `WindowActivate` would leave the desktop's parentless bar
   alive, which is the arrangement this fix exists to prevent. The reviewer could
   construct no reachable case and neither can I: a window that opens while the
   application is active becomes key, and one that opens while it is not active
   cannot show a menu bar at all until the application is activated, at which
   point it becomes key. The offscreen course pins the common path with
   `wait_for(desktop->desktop_menu() == nullptr)` after every reopening. It was
   left alone on purpose: changing `src/` now would rebuild the executable the
   prepared candidate is bound to, and a hardening change with no reachable case
   is not worth moving what the owner is about to look at. Whoever takes the
   owner's verdict should decide it with that verdict in hand.

### d10: the second rejection, and what it turned out to describe

The owner reviewed the fixed build and rejected it again, on 2026-09-16 at
14:11:41Z:

> When I close the last window it stays running in the bottom dock but not in the
> top menu bar.

Their shell history settles the first question before any theory is needed. At
**07:05:48** they ran the candidate's launch command — the pinned binary, path for
path — and at **07:06:08** they ran `shasum -a 256` on it. They reviewed the build
the candidate names. Nothing rebuilt `build/dev`: the executable's own timestamp is
still 02:05:53 that morning and its digest is still
`1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`.

**What settled the rest was an unlocked console.** Every earlier dispatch reported
`screen locked: yes, frontmost: loginwindow` and had to say the menu bar could not
be photographed. This one could. With the application active, the close changes
nothing a person can see: the menu-bar strip captured with a window open and the
strip captured after the last window closed are **the same PNG**, sha256
`5bb204b1972d673125b18cc80beda8c454a682f104bc6050feddb6080d7d7858`, reading
`Apple · BetterPullReview (tests) · Profiles · View` in both. Not the same menus by
name — the same pixels. The walk was done the whole way on that console: a literal
Dock-tile click, ⌘N and the native Profiles item each reopen exactly one window at
Home, and Quit from the application menu quits.

**And the owner's sentence is exactly reproducible, on the same build, in one
gesture.** `[NSApp mainMenu]` is this process's property and reads the same whoever
is active; the bar drawn across the top of the screen belongs to the **active**
application alone. Close the last window while this application is *not* the active
one — which a click on an inactive window's red close button does, because macOS
lets the close button through without activating anything — and the report is
word for word what happened: the process is alive, it has no window, its Dock tile
is there, and the menu bar on screen belongs to whichever application is active
(`scratchpad/d10/i2-after-clickthrough.png`).

So both are true, of different things. The automated measurement was not wrong; it
was blind. Every assertion this course made was about a menu, and the owner's
sentence was about activation.

**None of it is peculiar to this application.** A plain AppKit application, built
for the comparison and running beside it, keeps activation when its last window
closes and loses the menu bar the moment anything else is activated
(`scratchpad/d10/appkit-reference.log`). `Preview` with no windows open behaves the
same way: hold the bar while active, lose it when not.

**Two things were wrong, and neither was in `src/`.**

1. The course could not see the difference between "this application owns the bar"
   and "this application is not the active one". It now pins the second, over every
   close it watches rather than over one of them, so a person clicking another
   application mid-run cannot fail it and a close that really resigned activation
   cannot pass. Both halves of that are demonstrated by injection rather than
   asserted.
2. The **hide-and-unhide arm had never run**, because a locked screen skipped it,
   and the first time it ran it failed — on an assertion that was wrong about
   AppKit. `-[NSApplication unhide:]` restores an application's *windows*; it does
   not take activation, and an application with no window has none to restore. The
   plain AppKit reference stays inactive after `unhide:` four runs out of four, and
   stays inactive after a following `activateIgnoringOtherApps:` as well. Current
   macOS reserves activation for the person. The arm now asks for activation and
   records whether macOS granted it — sometimes it does — instead of asserting
   something the application does not decide. `retire_desktop_menu()`'s single call
   site was re-examined with the verdict in hand and still has no reachable case;
   it stays as it is.

**What did not change.** `src/` is byte-identical to `1c40d04b`. The fix stands as
written, and the executable the rejected candidate was pinned to is still the one
in `build/dev`, preserved besides at `scratchpad/d10/pinned/`.

**What is still the owner's.** Whether *they* closed that window while the
application was in front. Nothing on this machine can recover that, and guessing at
it is how the first rejection got answered with a menu when the question was about
activation. The replacement candidate asks it directly, carries both photographs,
and tells them how to tell which application owns the bar.

#### The replacement attended candidate, prepared and not performed

- **Candidate** `bpr-0796-empty-desktop-menu-bar-activation`, fingerprint
  `4907795c8fc49b43a7f0d23d73eaec94c782407e4bf887ad8f7bd32e4fffb3f0`, in the pack's
  existing session `e966c88f3320638208adf4fcf0651e31`, state directory
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review`, sequence 88.
  **19 candidates, 18 decided, 1 pending** — this one.
- **Bound to** `1c40d04b / app sha256 1f60b0a2… / courses d10`. The application binary
  is the same one the owner launched; only the course, its probe and the probe's
  header moved, and the bundle is protected again as a versioned resource.
- **The rejected candidate keeps its verdict.** `bpr-0796-empty-desktop-menu-bar` was
  released from protection so the same bundle could be claimed again, which leaves its
  rejection, its note and its fingerprint exactly where they were. This is a new
  candidate id, so no verdict was carried onto different material.
- **To open it,** serve that state directory:
  `python3 .agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review serve`
  and open the complete loopback URL it prints.
- **Offline fallback, already written:**
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review/bpr-0796-activation-review.html`,
  237 KB, self-contained, with all three photographs embedded. Decide in the page, use
  Copy JSON or Download JSON, then `import --file FILE` against the same state
  directory. The import is the record; browser storage is only a cache.
- **It carries screenshots**, which the last one said were impossible. Three, taken on
  this console against this bundle: the bar with a window, the bar after the close, and
  the bar when the close happened while another application was in front.
- **What it asks.** That the close changes nothing while BetterPullReview is the front
  application, and that the Dock tile, ⌘N, the Profiles menu and the Settings-focus
  check all behave — plus one question only the owner can answer: at the moment they
  closed that last window, was BetterPullReview's name next to the Apple menu, or
  another application's?
- **What it fixes in the asking.** Step 2 now tells them to click the window so the
  application is in front, and says how to tell. The old step 8's expectation that
  View's items would be **enabled** with Settings focused is gone: at Home they are
  greyed with any window focused, because nothing is open for them to act on, and a
  reviewer following that literally could have rejected on it.
- **Preparation evidence recorded on the candidate:** the launch binary's digest at zero
  pending build actions, the provenance and digests of the three photographs, and
  `ctest --preset dev -R ^application_profile_empty_desktop_native_menu_bar$` exiting 0,
  1/1 in 3.34 s.

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

d9, all at zero pending build actions:

- The diagnosis re-run on unchanged source, before anything was believed:
  `scratchpad/d9-native-baseline.log`, `scratchpad/d9-native-baseline-verbose.log`.
- The course failing first, on the real defect: `scratchpad/d9-course-fails-first.log`.
- The course passing after the fix: `scratchpad/d9-course-after-fix-2.log`;
  integrator's own re-run of the three decisive courses
  `scratchpad/d9-decisive-1.log`.
- The 14-course desktop-lifecycle, management and settings selection, 14/14 in
  184 s: `scratchpad/d9-focused-2.log`.
- The pack's integration pass, 48 courses, **100% passed, 48/48, 880.39 s**, with
  `ninja -n` reporting "no work to do" immediately before and immediately after:
  `scratchpad/d9-int-pass-01.log`.
- Documentation gate `DOCUMENTATION_OK` (`scratchpad/d9-doc-gate-2.log`) and 64
  documentation tests OK (`scratchpad/d9-doc-tests.log`).
- Formatting: every C++ file this card touched is clean under the repository's
  own `clang-format` except `profile_desktop.cpp`, whose one divergence is a
  member alignment block that already diverged at `HEAD` before this card and
  was left alone (`scratchpad/d9-format-check.log`). `native_main_menu_probe_mac.mm`
  is **not covered**: this `.clang-format` has no Objective-C section and the
  tool refuses the file, which is also why `format_check.py` never lists it.
- Fixed executable, the one the attended candidate is bound to:
  sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`,
  source `1c40d04b`.

d10, on an unlocked console, against the same executable:

- The two menu-bar photographs, byte-identical:
  `scratchpad/d10/cap-a-windowed-bar.png` and `scratchpad/d10/cap-b-windowless-bar.png`,
  both sha256 `5bb204b1972d673125b18cc80beda8c454a682f104bc6050feddb6080d7d7858`.
- The owner's report reproduced by closing an inactive window:
  `scratchpad/d10/i2-after-clickthrough.png`.
- The walk on the real screen — Dock click, ⌘N, the Profiles menu, Settings
  focused, Quit: `scratchpad/d10/h1-after-correct-dockclick.png`,
  `j2-after-cmdn.png`, `k1-after-profiles-menu.png`, `l1-settings-focused-bar.png`.
- The plain AppKit reference and its source:
  `scratchpad/d10/appkit-reference.log`, `scratchpad/d10/appkit_ref2.m`.
- The course failing on the never-run arm, before it was corrected:
  `scratchpad/d10/d10-course-unlocked.log`.
- The two non-vacuity injections: `scratchpad/d10/d10-course-nonvacuity.log`
  (a hide 40 ms into the first close) and `scratchpad/d10/d10-nonvacuity2.log`
  (every close resigns activation).
- The corrected course, five consecutive green runs:
  `scratchpad/d10/d10-stab1.log` … `d10-stab5.log`, and the seven-course focused
  selection `scratchpad/d10/d10-focused-1.log`.
- The bundle the rejected candidate was pinned to, preserved before any rebuild:
  `scratchpad/d10/pinned/better_pull_review.app`, sha256 of its executable
  `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`.

### A candidate that was considered and not applied

Calling `present_desktop_menu()` before `Runtimes.erase()` at
`src/app/src/profile_desktop.cpp:833` would remove even the intra-tick husk.
Constructing the bar eagerly at desktop start was **rejected**: a parentless bar
takes the menu bar whenever a menubar-less window is focused, and the course
proves today's behaviour correctly keeps the workbench bar while Settings has
focus. Neither was applied, because no observable either would change was
demonstrated.

**d9 kept both decisions, and the second one turned out to matter more than it
looked.** The intra-tick husk is still unobservable: the gap is 0 ms through the
close and, now that it is sampled, 0 ms through each reopening too, so moving
`present_desktop_menu()` earlier would still change nothing a reviewer can see.
Eager construction was right to reject, and for exactly the reason given — a
parentless bar takes the menu bar whenever a menubar-less window is focused. What
that note could not know is that this was already happening after the first
reopening, because the bar the desktop built was never retired. The fix goes the
other way from eager construction: the bar exists only while the desktop has no
window.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new
preference is authorized by this card. The core decides, the host performs, Qt
presents: the empty-desktop outcome stays a policy decision and the menu bar
stays presentation. Windows and Linux quit behaviour is [[BPR-0798]] and
[[BPR-0799]], not this card.
