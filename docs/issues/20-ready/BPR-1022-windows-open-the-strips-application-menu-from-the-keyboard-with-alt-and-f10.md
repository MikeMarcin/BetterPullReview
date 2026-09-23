---
schema: "implement-rfc.card.v1"
id: "BPR-1022"
title: "Windows: open the strip's application menu from the keyboard with Alt and F10"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"BPR-0653 integrator review (dsp-005), non-blocking finding","related_rfcs":["RFC-BPR-0028"],"related_cards":["BPR-0653","BPR-0646"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json"}
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
