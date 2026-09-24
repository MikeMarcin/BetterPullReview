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
