---
schema: "implement-rfc.card.v1"
id: "BPR-1712"
title: "Find in files results open in a new tab and copy as a selection"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1712--BPR-1712--5d3870d015e1.json","kind":"ux","origin":"owner request 2026-10-21","owner_note":"In find in files results I want to be able to open result in new tab with right click context menu. I also want to be able to multi select results so I can copy them to the clipboard","related_cards":["BPR-1132","BPR-1504","BPR-1688","BPR-1698"],"related_rfcs":["RFC-BPR-0052","RFC-BPR-0077"],"frontend_design":"frontend-design:frontend-design loaded before the result menu's and the list's wording"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"887ddcff1b91f28859675e4c786c9ad4a281179d","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_source_search_panel_scale_(1|2)|application_source_pr_find|application_source_pr_find_scale_(1_25|2_0))$'","detail":"Panel, with clicks and keys through the window system (qt_handleMouseEvent, qt_handleKeyEvent): a right-click on a result opens Open, Open in New Tab, a separator, Copy (showing the catalogue's key, the platform's Copy) and Copy Path; with no new-tab handler Open in New Tab is disabled. Chosen with a click, Open in New Tab asks for the clicked result in a new tab and Open for it in the active tab; a file row's menu goes to the file's first result; right-click inside the selection keeps it and outside it selects that row; Copy Path writes one path, Copy Paths each file once in the list's order. The menu key and Shift+F10 open the current row's menu below it and act on it; F10 alone opens none. Shipped composition, offscreen (source-find-v1): with src/other.cpp in the only tab, the right result on line 1,103 of src/find.cpp's 1,107 lines, chosen from its menu with a click, opens a second tab right after the first, makes it active, selects 'offscreen needle' on the right with that line in the pane's viewport, puts the window's keyboard on the right pane, keeps src/other.cpp in the first tab, keeps the same results without another scan (Statistics.Scans unchanged) and makes the result current. Failing-before control: with the product sources (the panel, keyboard_shortcuts, pr_search_controller and github_application) at the parent 6e8d0834f and these tests, plus a bare ActivateInNewTab member so the panel test compiles, qt_source_search_panel_scale_1 and _2 fail at 'the results take an extended selection of rows' and application_source_pr_find at 'right-click on a result opens its menu'. macOS offscreen at 1x, 1.25x and 2x."}},{"criterion":"AC2","outcome":"pass","revision":"887ddcff1b91f28859675e4c786c9ad4a281179d","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_source_search_panel_scale_(1|2)$'","detail":"A plain click selects and opens one result. Shift-click to the first result opens nothing and copies 'src/search/find_controller.cpp:12: const auto needle = locate(scope);', ':14: return needle_for(scope, options);', ':1204: // unchanged: the needle is shared' and 'src/search/added_view.cpp:1: auto view = make_needle_view(model);', the range over the added file's row written once. Command-click adds and removes single results and opens nothing. The list accepts Copy's ShortcutOverride; Command-C writes the selected lines in the list's order; Return opens the current result alone; Command-A selects all 8 rows and Copy then writes the 5 results once each, the last 'src/renamed.cpp:1: static int needle = 1;' (the right path of a rename); a plain Down selects one result and selects its match; Shift+Up extends to two and the accessible table reports 2 selected cells. A move of the current result from outside the list (F4's projection) replaces the selection. The list's accessible description reads 'Return opens the current result. Shift or Command with a click, or Shift with an arrow, selects more; Command-A selects every result and Command-C copies the selected ones. Shift+F10 or the menu key shows the result's actions.' The existing panel cases (marker clicks, Up, Return, disclosure, side chips, details, empty and filtered states) pass unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"887ddcff1b91f28859675e4c786c9ad4a281179d","evidence":{"kind":"command","command":"ctest --preset dev -R '^(architecture_boundaries|command_correctness_registry_contract|application_review_round_trip_composition_contract|qt_keyboard_shortcuts_scale_(100|125|200)|qt_document_tabs_scale_(100|125|200)|application_document_tabs_restoration_public_path|application_commit_filter_public_path|application_source_pr_search_scope|application_source_pr_search_retry)$'; cmake --build --preset dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <the six changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"18 of 18 focused tests pass on the candidate rebased onto 6e8d0834f, with the panel, Find in files and keyboard cases rerun after the last panel edit. The catalogue's two new read entries ('Copy the selected results', 'Select every result', in Find in files' results, the platform's Copy and Select All) pass test_shortcut_catalogue_is_what_the_window_binds and the Keyboard Shortcuts page; no window-wide entry shares their keys. coding-format-check (clang-format 22.1.8) passes on 1,723 files; clang-tidy (LLVM 22) with warnings as errors passes on source_search_panel.cpp, keyboard_shortcuts.cpp, pr_search_controller.cpp, github_application.cpp, source_search_panel_tests.cpp and source_pr_find_public_path_tests.cpp. tests/command_correctness_registry.json reseals github_application.cpp's six input pins (711801d15243 -> c5667c728ac9); the round-trip contract's full_application (-> c5667c728ac9) and production_app_manifest (5297f2c3deb1 -> 8e86a920fb5c) pins are resealed with a note at its top, and mutation 16d135419 stays exact. Both documentation gates pass with the card under a stand-in allocated ID; with the provisional ID the gate refuses only the card's ID format. macOS, the dev preset's Debug configuration built -O0 -g0."}}]
---

# BPR-1712: Find in files results open in a new tab and copy as a selection

## Owner note

The owner, 2026-10-21, verbatim:

> In find in files results I want to be able to open result in new tab with
> right click context menu. I also want to be able to multi select results so
> I can copy them to the clipboard

## Before

- Find in files' results ([[BPR-1504]]) took one row at a time. A click or
  Return opened the result in the active tab; nothing opened it in a tab of
  its own, and the list had no menu.
- The list's selection was single, and Qt's own Copy put the current row's
  accessible text (`Left line 12: …`) on the clipboard.

## Change

- **Multi-select.** The results take an extended selection of rows: Shift or
  Command (Ctrl elsewhere) with a click, and Shift with an arrow, select more
  and open nothing; Command-A (Ctrl+A) selects every row. A plain click, an
  arrow, Return and F4 still act on the one current result; F4 and Shift+F4,
  which move the current result from outside the list, select it alone
  whatever modifier is held. Unlike Files, Command-click here selects rather
  than opening a tab, as the owner asked.
- **Copy.** Command-C (Ctrl+C) and the menu's Copy write the selected results
  as plain text, one line each in the list's order: `path:line: text`, with the
  path and one-based line of the side the row shows (Right when both carry the
  match, so a rename's right path) and the snippet as the row draws it. A
  selected file row stands for all of its shown results, once each; with
  nothing selected, the current result. The list claims Copy and Select All in
  `ShortcutOverride`, so no window shortcut takes them.
- **The result menu.** Right-click, the menu key or Shift+F10 opens **Open**,
  **Open in New Tab**, **Copy** and **Copy Path** (**Copy Paths** across
  several files). Right-clicking inside the selection keeps it; outside, it
  selects that row. On a file row, Open and Open in New Tab go to the file's
  first shown result. The list's accessible description names the keys.
- **Open in New Tab.** The controller opens the result through the existing
  path with a tab choice. The composition applies the core's
  `open_in_new_tab_intent` (a background tab after the active one, as Files'
  Open in New Tab is) and then `activate_tab_intent` on the tab it added, so the
  result can be shown; at the tab limit nothing opens. A tab a result opened
  keeps the search bound to the tab it was bound to, as the first tab a result
  opened already did, so the results stay without another search and the
  pending reveal survives the tab change. The reveal is the existing one: it
  waits while the surface stages a long comparison
  (`reveal_source_match` refuses while `geometry_pending()`), so the line is
  shown after the file loads, unlike the one-shot pick [[BPR-1698]] fixed.
- **Keys.** The shortcut catalogue gains Find's "Copy the selected results"
  and "Select every result", read in Find in files' results.

The clipboard write is the panel's one effect, a Qt presentation effect. The
choice of tab and the tab order are the core's document-tab transitions; no
core or application-host code changes.

## Acceptance

- **AC1.** A result's menu (right-click, the menu key, Shift+F10) offers Open,
  Open in New Tab, Copy and Copy Path; Open in New Tab opens the result's file
  in a new active tab after the one it came from, at the result's line, with
  the results kept. The cases fail before the change.
- **AC2.** Shift, Command (Ctrl) and Select All select several results; Copy
  writes them one `path:line: text` line each in the list's order, a selected
  file row standing for its results; Return, a plain click and arrows still
  act on one result. The cases fail before the change.
- **AC3.** The keyboard shortcut catalogue lists the two keys without
  conflict; the existing Find in files, document-tab and commit-filter courses
  pass; format, tidy and documentation gates pass.
