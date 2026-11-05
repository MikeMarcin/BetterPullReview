---
schema: "implement-rfc.card.v1"
id: "BPR-1863"
title: "New Window in the File menu"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"ux","found_by":"owner request, 2026-11-04: \"I should have new window as an option from the file menu\"","reservation":"docs/issues/_reservations/BPR-1863--BPR-1863--e26d68e54b14.json","related_cards":["BPR-0777","BPR-0796","BPR-1195","BPR-1342","BPR-1567"],"related_rfcs":["RFC-BPR-0024","RFC-BPR-0033"],"owner_decision":"2026-11-04, relayed by the coordinator: New Window leaves the Profiles menu and lives only in File › New Window; no owner review needed.","frontend_design":"loaded: frontend-design:frontend-design for the placement. New Window is the first item of File, as every desktop application places it, before a separator; no new look.","shortcut_decision":"The brief proposed Command-Shift-N / Ctrl+Shift+N. The command already had a key: the windowless bar's New Window is bound to QKeySequence::New (Command-N, Ctrl+N), which RFC-BPR-0033 B2 adopts as the reopen gesture, and the catalogue keeps one key per command. The window's item takes that same key instead of a second one; Ctrl+N is bound nowhere else, and Command-N is the macOS New Window convention (Safari, Finder, Terminal)."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d2b197e1d","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^qt_profile_management_window$'","detail":"newBprWindowAction is the first item of the window's File menu, File is the bar's first menu, Profiles shows only the profiles and Manage Profiles… (3 visible), the item's keys are shortcut_keys(new_window) == QKeySequence::New, and triggering it dispatches new_bpr_window_intent for the window's current profile. The resident bar's File is New Window, separator, Quit, Open Project…, Close Tab; its Profiles is the profiles then Manage Profiles…; New Window still reopens the most recent profile."}},{"criterion":"AC2","outcome":"pass","revision":"d2b197e1d","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R 'qt_keyboard_shortcuts_scale_100|qt_workbench_window_scale_100'","detail":"test_shortcut_catalogue_is_what_the_window_binds: the window, the resident bar and a Review Cycle view bind exactly the catalogue, with new_window naming newBprWindowAction and desktopNewBprWindowAction; the assembled window's keys are unique, so Ctrl+N clashes with nothing."}},{"criterion":"AC3","outcome":"pass","revision":"d2b197e1d","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R 'application_profile_management_public_path|application_profile_empty_desktop_native_menu_bar'","detail":"macOS: the native main menu offers File › New Window and not Profiles › New Window with a window and without; File › New Window performed through AppKit creates the sibling window; the windowless desktop's Profiles lists three profiles and management."}},{"criterion":"AC4","outcome":"pass","revision":"d2b197e1d","evidence":{"kind":"command","command":"python3 _coding-standard/scripts/format.py <changed C++ files>; python3 tests/scripts/settings_inventory_comparison_tests.py; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22 clean; inventory comparisons pass. docs/ux/workbench-specification.md says File opens with New Window and Profiles holds the profiles and Manage Profiles…, in a window and with none. The Windows and Linux settings and activation courses now reach File > New Window; they were not run on those hosts."}}]
---

# BPR-1863: New Window in the File menu

## Goal

The owner asked for New Window in the File menu. It was only under
Profiles, as Profiles › New Window, in a window's bar and in the resident
bar macOS keeps with no window open.

## Decision

- **File › New Window**, first in File and followed by a separator, in a
  window's bar and in the resident bar. It is the same action, object name
  and `new_bpr_window_intent` for the current profile as before; only its
  menu changes. Home's Open Review in New Window is still offered exactly
  when it is.
- **Profiles** keeps the profiles and Manage Profiles…. The owner decided
  New Window leaves it (2026-11-04). A per-profile "New Window in
  <profile>" was considered and not added: choosing a profile already opens
  or activates it, and opening a second window of another profile was not
  asked for.
- **Key.** `Ctrl+N` (⌘N on macOS), the platform New key. The windowless
  bar already bound it to this command (RFC-BPR-0033 B2), and the
  catalogue gives each command one key. The window's item now carries it
  as well, instead of the ⌘⇧N first proposed, which would have given New
  Window two keys. Nothing else binds Ctrl+N; the catalogue and the
  assembled-keys tests check that.

## Acceptance

- **AC1**: In a window and in the resident bar, File's first item is New
  Window, Profiles holds only the profiles and Manage Profiles…, and New
  Window opens a window of the current profile through the existing intent.
- **AC2**: New Window's key in a window is the catalogue's and clashes with
  no other binding.
- **AC3**: On macOS the native main menu offers File › New Window, with and
  without a window, and performing it opens the sibling.
- **AC4**: The spec describes the menus; format and documentation gates
  pass.
