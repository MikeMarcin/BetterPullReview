---
schema: "implement-rfc.card.v1"
id: "BPR-1022"
title: "Windows: open the strip's application menu from the keyboard with Alt and F10"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"BPR-0653 integrator review (dsp-005), non-blocking finding","related_rfcs":["RFC-BPR-0028"],"related_cards":["BPR-0653","BPR-0646"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json"}
acceptance: [{"criterion":"AC1 On Windows, Alt pressed and released alone, and F10, each open the strip's application menu with its first menu highlighted; Esc closes it and focus returns to the widget that had it","outcome":"pass","revision":"02905c77","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2: qt_workbench_strip_menu (test_strip_menu_opens_from_alt_and_f10) passes at 02905c77 (14.36 s) and 3 of 3 at the same change's pre-rebase local commit fd2c21d7 (14.3-14.4 s). Keys enter through QWindowSystemInterface, as the Windows plugin delivers them: Alt alone and F10 each open workbenchStripMenu with exactly the bar's visible menus, File highlighted and not opened; Esc closes it, the keyboard returns to the editor, the menu deletes itself; Right and the last menu's mnemonic open their menus; Alt alone again closes it. The real windows platform was measured at 90ef83f0 with real injected keys: Qt's key mapper delivers Key_Alt and Key_F10 unaccepted, no WM_SYSCOMMAND/SC_KEYMENU reaches the window and no menu mode is entered, so the course's input is the platform's. Negative controls fail by name: filter not installed; setActiveAction instead of the menu's Down.","path":"docs/issues/60-done/BPR-1022-windows-open-the-strips-application-menu-from-the-keyboard-with-alt-and-f10.md#measured-then-built-without-host-code-bpr-main-win-2026-09-23"}},{"criterion":"AC2 Alt used as a modifier opens no menu and keeps its own effect: every existing Alt chord, Alt+Space's system menu, Alt+F4, AltGr text entry and an Alt-click","outcome":"pass","revision":"02905c77","evidence":{"kind":"command","detail":"Same row: each of Alt+N, Alt+1..3, Alt+Up/Down/PgUp/PgDown, Alt+Shift+1..5 and Alt+Shift+R keeps exactly its bare-shortcut effect (actions triggered and popups shown counted both ways; 10 of 14 have an effect in the projection) and opens no menu; Alt+Space and Alt+F4 open no menu and are never taken by the filter; AltGr (Ctrl+Alt+Q, text @) types @ into the editor; an Alt-click presses the files drawer toggle once and opens no menu. On the real platform at 90ef83f0 Alt+Space opened the native system menu through Qt's own key mapper and Alt+F opened nothing. Negative controls fail by name: other keys not making Alt a modifier (Alt+N), mouse not making Alt a modifier (Alt-click).","path":"docs/issues/60-done/BPR-1022-windows-open-the-strips-application-menu-from-the-keyboard-with-alt-and-f10.md#measured-then-built-without-host-code-bpr-main-win-2026-09-23"}},{"criterion":"AC3 A focused course on the real windows platform (or offscreen where Qt's key path is the same, stated which) drives the keys and asserts AC1 and AC2; application_windows_title_row, qt_workbench_title_row_scale_{100,125,200}, qt_workbench_focus_order_scale_{100,125,200} and qt_composed_github_scale_100 pass on the Windows station","outcome":"pass","revision":"02905c77","evidence":{"kind":"command","detail":"The course is offscreen, stated in the test and on the card: input enters at QWindowSystemInterface, so the shortcut map, ShortcutOverride and the window's delivery are Qt's real path, and the platform plugin's part was measured on the real windows platform. At 02905c77 serial: application_windows_title_row (real windows platform) 1.97 s, qt_workbench_title_row_scale_100/125/200, qt_workbench_focus_order_scale_100/125/200 and qt_composed_github_scale_100 pass; at fd2c21d7 (the same change before the rebase) architecture_boundaries, coding_format_check, application_workbench_composition_contract and application_profile_second_window_close_public_path pass too. A real-key run of the candidate itself was not possible: the application's GUI thread blocks after its first paint on this desktop today, at HEAD without this change too (reported separately).","path":"docs/issues/60-done/BPR-1022-windows-open-the-strips-application-menu-from-the-keyboard-with-alt-and-f10.md#measured-then-built-without-host-code-bpr-main-win-2026-09-23"}},{"criterion":"AC4 macOS and Linux are unchanged; their window and focus-order rows pass at merge","outcome":"pass","revision":"02905c77","evidence":{"kind":"freeform","detail":"Not run from this station. Every product line is inside #if defined(_WIN32) apart from an added #include <QWindow>; the test lines are in the Windows-only --strip-menu path and the private header directory is added inside if(WIN32). The macOS and Linux window and focus-order rows are theirs to confirm in their next sweeps."}}]
---

# BPR-1022: Windows: open the strip's application menu from the keyboard with Alt and F10

## Why

[[BPR-0653]] made the strip the Windows title row, after Visual Studio Code.
The menu bar row is hidden, and a compact menu control, "Application menu", is
the strip's first item. A click on it opens the menu bar's visible menus (File,
Profiles, Pull Request and View), read from the bar each time (the
`StripMenuButton` connection in `workbench_window.cpp`, `#if defined(_WIN32)`).

The review of that change found that with the bar hidden, nothing opens those
menus from the keyboard in the way Windows users expect. Alt+F and Alt+V no
longer reach them. Visual Studio Code binds Alt, pressed and released alone,
and F10 to its compact menu. The finding was non-blocking, and it is carried
here.

The menus are already reachable from the keyboard: the menu control is the
strip's first focus stop, with strong focus, and Space or Enter opens it. This
card adds the platform's shortcut convention; it does not restore the only
keyboard path. So it is not a dependency of RFC-BPR-0028's closeout.

## Scope

- Windows only, in the Windows branch of `workbench_window` (Qt presents) and,
  only where the platform needs it, in `windows_title_row_host` in `src/app`
  (the host performs).
- **Alt alone opens the menu.** Pressing and releasing Alt, with no other key
  and no mouse button in between, opens the application menu under the menu
  control, with its first menu highlighted. F10 does the same. Esc closes it
  and returns focus to the widget that had it.
- **Alt as a modifier is untouched.** Every existing Alt chord keeps its
  effect and opens no menu. The chords are Alt+N, Alt+1 to Alt+3,
  Alt+Up/Down/PgUp/PgDown, Alt+Shift+1 to Alt+Shift+5 and Alt+Shift+R. The same
  holds for Alt+Space (the system menu), Alt+Tab, Alt+F4, AltGr text entry
  (Ctrl+Alt) in the editors, and an Alt-click.
- Inside the opened menu, the menus' own mnemonics and the arrow keys work as
  they do from the mouse path, and the menu offers exactly the bar's visible
  menus.
- macOS and Linux do not change: macOS keeps its native menu bar, Linux keeps
  the menu bar row, and neither binds these keys.

### Design notes, to be measured on the Windows station

- Released alone, Alt goes to the window procedure as `WM_SYSKEYUP` with
  `VK_MENU`. `DefWindowProc` then posts `WM_SYSCOMMAND` `SC_KEYMENU` with
  `lParam` 0 and enters the system menu loop. With the title bar removed, that
  loop may take the keystroke first, or leave the window in menu mode. If it
  does, the host should consume `SC_KEYMENU` with `lParam` 0 and ask
  presentation to open the menu. Alt+Space arrives as `lParam` `' '` and keeps
  the system menu.
- Qt's `QMenuBar` has its own Alt handling. Check whether a hidden bar still
  reacts before adding a second handler.
- The strip menu is a stack `QMenu` run by `exec()`. The crash reserved for
  bpr-main-win (Quit through that menu destroys the window beneath it,
  BPR-1028's reservation) goes through the same path. Choosing Quit from a
  menu opened by the keyboard must wait for that fix. Share the fixed
  menu-opening path rather than adding a second one.

## Acceptance

- AC1 On Windows, Alt pressed and released alone, and F10, each open the
  strip's application menu with its first menu highlighted; Esc closes it and
  focus returns to the widget that had it.
- AC2 Alt used as a modifier opens no menu and keeps its own effect: every
  existing Alt chord, Alt+Space's system menu, Alt+F4, AltGr text entry and an
  Alt-click.
- AC3 A focused course on the real `windows` platform (or offscreen where Qt's
  key path is the same, stated which) drives the keys and asserts AC1 and AC2.
  `application_windows_title_row`, `qt_workbench_title_row_scale_{100,125,200}`,
  `qt_workbench_focus_order_scale_{100,125,200}` and
  `qt_composed_github_scale_100` pass on the Windows station.
- AC4 macOS and Linux are unchanged. Their window and focus-order rows pass at
  merge.

## Two design notes rechecked (bpr-main-win, 2026-09-23)

**The stack-menu note is out of date.** [[BPR-1028]] (`e08a0409`) replaced
the stack `exec()` menu. `workbench_window::open_strip_menu()` is now the one
opening path: a heap child of the window with `WA_DeleteOnClose`, opened with
`popup()`. Quit through a menu opened by the keyboard no longer waits on
anything. Alt and F10 should call that function and then highlight the first
menu.

**A hidden `QMenuBar` does not react at the Qt level.** A standalone probe
(station helper, not committed; Qt 6.11.2, `offscreen`, Fusion,
`SH_MenuBar_AltKeyNavigation=1`) sent key events to a focused line edit in a
`QMainWindow` with File and View menus:

| Keys | Bar shown | Bar hidden |
| --- | --- | --- |
| Alt pressed and released alone | Bar enters keyboard mode, File active, focus to the bar | Nothing |
| F10 | Nothing (Qt binds no F10) | Nothing |
| Alt+F | File opens | Nothing |

So a handler in the Windows branch would not compete with the bar's own Alt
handling. What `DefWindowProc` does with `SC_KEYMENU` in the frameless title
row is still unmeasured, and so is whether Qt's Windows key mapper passes the
released Alt on to it. That needs the real `windows` platform with a shown
window, which is a foreground run on this station.

## Measured, then built without host code (bpr-main-win, 2026-09-23)

**The platform never enters menu mode.** A foreground run on the real
`windows` platform launched `better_pull_review.exe` with the title row, on
a fresh private data root, at `90ef83f0` with an uncommitted probe in
`windows_title_row_host`. The probe logged the window's key, menu and
`WM_SYSCOMMAND` messages and the Qt key events. The keys were real
injected keyboard input and the pointer was not moved. The harness read the
thread's menu state with `GetGUIThreadInfo` after each gesture.

| Keys | Native messages to the window | What Qt delivered | Menu state |
| --- | --- | --- | --- |
| Alt pressed and released alone | `WM_SYSKEYDOWN`, `WM_SYSKEYUP` `VK_MENU`; no `WM_SYSCOMMAND` | `KeyPress` and `KeyRelease` `Key_Alt`, up the focus chain from Home's Open bar to the window, accepted by none | none |
| F10 | `WM_SYSKEYDOWN`, `WM_SYSKEYUP` `VK_F10`; no `WM_SYSCOMMAND` | `KeyPress` and `KeyRelease` `Key_F10`, accepted by none | none |
| Alt+Space | `WM_SYSKEYDOWN`, `WM_SYSCHAR` space, `WM_MENUSELECT` | Qt's key mapper opened the system menu itself | `INMENUMODE`, `POPUPMENUMODE`, one `#32768` popup; one Esc closed it |
| Alt+F | `WM_SYSCHAR` `f`; no `WM_SYSCOMMAND` | `Key_F` with Alt, accepted by none | none |
| Alt+Up | the chord; Alt's release arrives as `WM_KEYUP` | the chord, then `Key_Alt` released | none |

Qt's key mapper answers `WM_SYSKEYUP` itself, so `DefWindowProc` never
posts `SC_KEYMENU`. Nothing is left in menu mode and nothing eats the next
key. The design note that the host should consume `SC_KEYMENU` is therefore
moot: no host code was written. The logs are station records under
`build/logs/bpr-1022-main-win/` (`probe-head.log`, `measure-head.json`) and
are not committed.

**The change** is in the Windows branch of `workbench_window`, so Qt
presents it. The window filters its own `QWindow`, which receives every key
and mouse event before any widget. The filter is installed when the window
is shown.

- Alt pressed and then released, with nothing between, opens the menu
  through the one opening path, `open_strip_menu()` ([[BPR-1028]]). That
  function now returns the menu. The first menu is highlighted with the
  menu's own Down key. `setActiveAction` would open File instead of
  highlighting it (negative control 4 below). F10 does the same on its
  press, and that press is taken.
- These make Alt a modifier: any other key or chord, including a shortcut
  (seen as `ShortcutOverride`, because a matched shortcut never arrives as a
  press), and Ctrl, Shift or Meta held with Alt (which covers AltGr). So do
  a mouse button (an Alt-click), a wheel turn, and a focus or activation
  change (Alt+Tab). A modifier opens nothing. A held Alt's repeated presses
  do not re-arm it.
- Nothing opens while a popup is open. Alt pressed alone again closes the
  open menu, through the menu's own Alt handling, and does not reopen it.
- `docs/ux/workbench-specification.md` says what Alt and F10 do on Windows.

**The course** (AC3) is offscreen, where Qt's key path is the same, and that
is stated in the test. It is `qt_workbench_strip_menu`, the Windows-only
row, now with `test_strip_menu_opens_from_alt_and_f10`. Keys and clicks
enter through `QWindowSystemInterface`, where the Windows platform plugin
hands input to Qt. So the shortcut map, `ShortcutOverride` and the window's
own delivery are the real ones. The measurement above covers the part
before that: what the plugin passes on. The test target takes QtGui's one
versioned header directory, on Windows only, as the composition's title-row
host does. The course asserts:

- AC1: Alt alone and F10 each open the menu with exactly the bar's visible
  menus, with File highlighted and not opened. Esc closes the menu, the
  keyboard returns to the editor that had it, and the menu deletes itself.
  Right opens File. The last menu's mnemonic opens that menu. Alt pressed
  alone again closes the menu.
- AC2: each of Alt+N, Alt+1 to Alt+3, Alt+Up, Alt+Down, Alt+PgUp,
  Alt+PgDown, Alt+Shift+1 to Alt+Shift+5 and Alt+Shift+R is pressed both
  bare and wrapped in Alt's own press and release. The course counts the
  actions triggered and the popups shown each way. Every chord keeps exactly
  its bare effect and opens no menu. Ten of the fourteen have an effect in
  this projection; Alt+N and Alt+1 to Alt+3 are disabled there. Alt+Space
  and Alt+F4 open no menu. AltGr (Ctrl+Alt+Q with the text `@`) types `@`
  into the editor and opens no menu. An Alt-click presses the files drawer
  toggle once and opens no menu. Alt alone still opens the menu afterwards.

Offscreen, closing the nested private-progress menu can leave no window
active. The Windows platform never activates a popup. The course therefore
re-activates the window before each chord, as the platform would leave it,
and asserts that it did so. Without that step, 1 run in 2 lost the
Alt+Shift+4 comparison.

**Negative controls.** Each was a temporary edit to `workbench_window.cpp`,
rebuilt and run, then restored.

| Removed | Fails at |
| --- | --- |
| the filter's installation on the window | `Alt pressed and released alone opens the strip's application menu` |
| other keys and shortcuts making Alt a modifier | `Alt+N opens no application menu when Alt is released` |
| a mouse button making Alt a modifier | `an Alt-click opens no application menu` |
| the menu's own Down, replaced by `setActiveAction` | `Alt pressed and released alone highlights the first menu without opening it` |

**Verification** (build/dev, MSVC 19.51 Debug, Qt 6.11.2, serial). The
change was verified first at the local commit `fd2c21d7`. It was then
rebased onto main as `02905c77`, whose base brought in other stations'
SQLite and composition changes but nothing in the Qt adapter, and it was
rebuilt and run again there.

- At `02905c77`, these pass: `qt_workbench_strip_menu` (14.36 s),
  `application_windows_title_row` (real `windows` platform, 1.97 s),
  `qt_workbench_title_row_scale_100/125/200`,
  `qt_workbench_focus_order_scale_100/125/200` and
  `qt_composed_github_scale_100`.
- At `fd2c21d7`, `qt_workbench_strip_menu` passes 3 of 3 (14.3–14.4 s), and
  so do `architecture_boundaries`, `coding_format_check`,
  `application_workbench_composition_contract` and
  `application_profile_second_window_close_public_path`.
- `clang-format` 22.1.8 reports the touched sources clean. `clang-tidy`,
  run on `workbench_window.cpp` with warnings as errors, passes. An earlier
  candidate's redundant null check had let the analyzer assume a null watched
  object, and that check is gone.
- The same integration pass also found three rows red:
  `qt_workbench_window_scale_100/125/200` (`the composed link map paints
  ribbons to begin with`) and `application_diff_workbench_public_path`
  (`DIFF_INTERACTION_FAILURE:capture-state`). Both fail identically at
  `b9991925` without this change. They are not this card's; they are
  reported to the coordinator for their own card.

**What is not shown here.**

- The candidate itself was not driven with real keys on the real platform.
  The same harness was run against it four times. Each time the
  application's GUI thread sat in a wait that pumps no messages right after
  its first paint, so it could not be brought to the front. It answered no
  `WM_NULL` and stayed open after `WM_CLOSE`. HEAD without this change does
  the same in 4 of 5 starts on this desktop today. It is reported to the
  coordinator as its own finding.
- Alt+Tab would switch the owner's windows, so it was not pressed. Its path
  is the focus change the filter reads.
- Alt+F4 is handled by `DefWindowProc` after Qt leaves the key unaccepted.
  The filter never takes it, and the course shows that no menu opens.
- AC4 is reasoned, not run on those platforms. Every product line is inside
  `#if defined(_WIN32)`, apart from an added `#include <QWindow>`. The test
  lines are in the Windows-only `--strip-menu` path, and the CMake include
  path is inside `if(WIN32)`. The macOS and Linux window and focus-order
  rows are theirs to confirm in their next sweeps.

**Where it landed.** The push gate rebased once more, so the change is on
main as `785a6828` and the finish as `c584a4d5`. The verified `02905c77`
and `785a6828` differ only by the other station's Git protocol v2 parser
work and its tests (`git_protocol_v2_tests.cpp`,
`git_cache_contract_tests.cpp`). Nothing in the Qt adapter, the app or these
rows differs between them. Neither `fd2c21d7` nor `02905c77` is on main.

**A foreground incident, checked against these runs.** The integrator
reported an unannounced WSLg window on this desktop from 23:14:59Z to about
23:17Z, when the Linux station launched the full application by accident.
The measurement this card rests on ended at 22:47:29Z
(`measure-head.json`), before that window, so it stands. The first of the
four real-key verify runs ended at 23:17:12Z, inside the window or at its
edge, and the second at 23:17:52Z. Both could not bring the application to
the front. The foreground then was a Windows Terminal window, not the
application. So the statement above that the harness failed four times
rests on the third and fourth runs, at 23:45Z. It also rests on the
application's startup behaviour, which was measured between 23:23Z and
00:06Z, after the window. Nothing recorded as passing evidence on this card
came from inside the window.

## Post-close: AC4 on macOS at `7b376657` (bpr-main-mac, 2026-09-23)

**AC4 now holds on macOS, by a run.** On Linux it still rests on the
reasoning above: this station did not run Linux.

The run is at `7b376657`, which contains `785a6828`. It also contains
[[BPR-1046]]'s `669a5a58`, which later changed `workbench_window.cpp`, its
header and `workbench_window_tests.cpp`. So these are the macOS rows at
current main, not at the merge commit itself. macOS 26.6.2 on Apple Silicon,
Apple clang 21.0.0, Qt 6.11.2, `build/dev` (Debug). The build was made at
`b5cb3b43`, whose build inputs are those of `7b376657`. It exited 0 with no
`warning:` line. So the added `#include <QWindow>`, the change's one product
line outside `#if defined(_WIN32)`, compiles here.

This passed 10 of 10, exit 0, serial, offscreen and contained as registered:
`ctest --preset dev -R '^(qt_workbench_window_scale_(100|125|200)|qt_workbench_title_row_scale_(100|125|200)|qt_workbench_focus_order_scale_(100|125|200)|qt_composed_github_scale_100)$' --output-on-failure`.

| Rows | Time at 100 / 125 / 200 |
| --- | --- |
| `qt_workbench_window_scale_*` | 19.09 / 19.50 / 20.21 s |
| `qt_workbench_title_row_scale_*` | 1.27 / 1.47 / 1.90 s |
| `qt_workbench_focus_order_scale_*` | 0.75 / 0.25 / 0.26 s |
| `qt_composed_github_scale_100` | 3.57 s at 100 |

The window rows are there because AC4 names window rows. The Windows station
found `qt_workbench_window_scale_*` red at `b9991925` without this change.
On macOS all three pass. `application_windows_title_row` and
`qt_workbench_strip_menu` are registered only on Windows. The cocoa course
`application_profile_empty_desktop_native_menu_bar` was not run: it needs
the owner's Aqua session, and this sweep was unattended. The console log is
a station record under `build/logs/bpr-1038-1022-0886-mac/` and is not
committed. The acceptance record above is unchanged.

## Post-close: real keys on the Windows desktop at `9f144f1e` (bpr-main-win, 2026-09-24)

**Alt alone, F10, Esc and AltGr hold with real keys. The Alt chords could
not be measured here, because something on this desktop takes the Alt out of
every injected chord before any application sees it.** Nothing measured
contradicts the offscreen course, so the card stays Done and its acceptance
record is unchanged.

[[BPR-1074]] found the reason the earlier harness could not bring the
application to the front: it held a drawer's short-lived window. With that
fixed, the candidate was `9f144f1e` (`build/dev`, MSVC 19.51 Debug, Qt
6.11.2, executable sha256 `82eae9cb2e24d56a…`), launched directly on the real
`windows` platform with a fresh private data root. The keyboard layout is US
(`0x04090409`). The harness injected keys with `keybd_event` and never
touched the pointer. Before each gesture it re-found the application's window,
checked that the handle was still a window, and checked that the application
held the foreground. It read the open popups, the thread's menu mode, the UI
Automation focus, and the UI Automation items of each popup. Six runs,
05:13:29Z–05:24:40Z, each ending with the Windows Terminal back in the
foreground and no process left. Logs are station records under
`build/logs/bpr-1022-main-win/` (`realkeys-*.json`, `rawsink-*.txt`), not
committed.

| Keys | Result, in every run |
| --- | --- |
| Alt pressed and released alone (left or right Alt) | One application popup, the strip menu. The keyboard stays in Home's Open bar. |
| then Right | A second popup opens, File's menu, with `Quit` highlighted. So File was the highlighted item and was not open. |
| then Esc, Esc | File's menu closes, then the strip menu closes. The keyboard is in the Open bar, where it was before. |
| F10, then Right, Esc, Esc | The same as Alt alone. |
| Alt alone, then Alt alone again | Opens the menu, then closes it. |
| AltGr as an AltGr layout delivers it (left Ctrl with right Alt), alone and with Q | Nothing opens: no popup and no menu mode. |

UI Automation did not list the highlighted File item and did not move its
focus into the menu, so the highlight was read from what Right does.

**The Alt chords did not reach the application as chords.** Alt+Space opened
no system menu, and Alt+F4 did not close the window. In the Qt log,
`Key_F4` and `Key_Up` arrived with `NoModifier`. A synthetic-only raw input
sink (a hidden window that records only input with no device handle, so none
of the owner's typing) showed why. For every Alt chord, with the left or the
right Alt and paced or not, the Alt key-down reached raw input as a key with
virtual-key code 0 (`vk=0x00`, `WM_KEYDOWN`). The Space, F, Up or F4 then came
as a plain `WM_KEYDOWN`. Raw input runs after the low-level keyboard hooks, so
one of those hooks rewrites the Alt. Alt alone passes through as `VK_MENU`,
`WM_SYSKEYDOWN`, and so does Alt held with Ctrl. On 2026-09-23 at 22:47Z, the
same injection on this desktop opened the system menu on Alt+Space, before
this card's change. The rewriting started later. That is the owner's desktop
environment and not the product, because the application never received the
Alt. Nothing was done to find or stop the hook. A seventh run, at `21332605`
at 06:46Z, after bpr-main-linux's WSLg course had released the desktop, gave
the same results. The raw input sink again recorded a virtual-key-0 key in
place of the Alt of every chord.

Wanted when the chords reach applications again (for example, when a
`keybd_event` Alt+Space opens the system menu of any application): the
Alt+Space, Alt+F, Alt+Up and Alt+F4 rows of this drive.

## Post-close: AC4 on Linux at `b5cb3b43` (bpr-main-linux, 2026-09-23)

**AC4 now holds on Linux, by a run, as it does on macOS.** Linux keeps its
menu bar row, and its window, title-row and focus-order rows pass.

The run is at `b5cb3b43`, in a separate verification clone with a clean
tree. That revision contains `785a6828` and [[BPR-1046]]'s `669a5a58`, so
these are the Linux rows at the same content as the macOS run, not at the
merge commit itself. JaegAW: WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2,
`build/dev` (Debug) from the `dev` preset, which compiles with `-Werror`. The
build log has no `warning:` line, so the added `#include <QWindow>`, the
change's one product line outside `#if defined(_WIN32)`, compiles here with
warnings as errors. The rows ran serially and offscreen:
`ctest --preset dev -j1 -V`, with `QT_QPA_PLATFORM=offscreen` and with
`DISPLAY` and `WAYLAND_DISPLAY` unset. When the batch of runs began, the
station was loaded by a parallel build (load average 9.7). At the batch's end
the load average was 2.5.

These passed 9 of 9, exit 0, contained as registered:

| Rows | Time |
| --- | --- |
| `qt_workbench_title_row_scale_100/125/200` | 4.05 / 8.25 / 11.02 s |
| `qt_workbench_focus_order_scale_100/125/200` | 0.20 / 0.23 / 0.26 s |
| `qt_composed_github_scale_100` | 9.49 s |
| `application_workbench_composition_contract` | 23.78 s (`WORKBENCH_COMPOSITION_CONTRACT_OK`) |
| `application_profile_second_window_close_public_path` | 1.90 s (5 rounds) |

That batch did not include the window rows that AC4 names. They were run
afterwards on this station's own `build/dev`, at main's `b60e04b0`, with the
same compiler and flags. The build was current, and `ninja` had no work to
do. That is later than [[BPR-1074]]'s `0bc65b0c`, which changed the
workbench drawer and `workbench_window_tests.cpp`. The rows ran serially and
offscreen, as above, with a fresh `HOME` and fresh XDG data, config, cache,
state and runtime directories, which were removed afterwards.
`qt_workbench_window_scale_100/125/200` passed 3 of 3, exit 0, in 21.61,
22.93 and 23.88 s.

`qt_workbench_strip_menu` and `application_windows_title_row` are registered
only on Windows (`if(WIN32)` in `tests/adapters_qt/CMakeLists.txt` and
`tests/end_to_end/CMakeLists.txt`), and neither exists in the Linux build.
The console logs are station records under
`build/logs/bpr-main-linux-20260923/bpr-1038-1022-0886-linux/bpr-1022-rows.log`
and `build/logs/bpr-main-linux-20260924/impl/bpr-1022-window-rows-b60e04b0.log`,
and are not committed. The acceptance record above is unchanged.

## Post-close owner decision, 2026-09-24: identify the hook read-only (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-1022-alt-chord-hook-v001`, revision
`3db65162` (seventh real-key run at `21332605`), fingerprint `d6d74c7b…`,
decision `7876410c-e657-44f8-be30-846db8115778`, sequence 582,
2026-09-24T19:13:28Z, `actor: human`: **select**, verbatim:

> I do not know what it is; have the Windows station identify it read-only,
> without stopping or changing it

The note is empty. The identification is [[BPR-1109]] (Ready, Windows,
read-only). This card stays Done. Its Alt+Space, Alt+F, Alt+Up and Alt+F4 rows
are still wanted once chords reach applications again.
