---
schema: "implement-rfc.card.v1"
id: "BPR-1343"
title: "Cross-platform guards: a Qt adapter local that hides a QWidget data member, and a WINDOWS_UNCONTAINED row without a pin"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"bpr-main-win, 2026-10-08: two MSVC C4458 build breaks on main (BPR-1090, BPR-1336) and 15 unpinned WINDOWS_UNCONTAINED rows in one day","reservation":"docs/issues/_reservations/BPR-1343--BPR-1343--6902e820d213.json","related_cards":["BPR-1090","BPR-1336","BPR-1324","BPR-1302"],"decision":"coordinator, 2026-10-08: both guards approved"}
acceptance: [{"criterion":"BPR-1343:shadowing-check","outcome":"pass","revision":"2979e0879","evidence":{"kind":"command","path":"cmake/VerifyArchitectureBoundaries.cmake","detail":"architecture_boundaries refuses a data or d_ptr local, parameter, range-for variable or capture in src/adapters/qt and accepts the tree, model data() overrides, .data()/->data, comments, strings, qualified names, aData and metadata; architecture_boundary_contract (148 tests) pins both directions. Live-tree control: BPR-1336's data local put back is refused with the new message. bpr-main-win, windows-dev."}},{"criterion":"BPR-1343:pin-registration-check","outcome":"pass","revision":"2979e0879","evidence":{"kind":"command","path":"scripts/verify_test_network_isolation.py","detail":"Every WINDOWS_UNCONTAINED row carries bpr_windows_uncontained_registration on every platform; pin_registration_failures refuses an unpinned row on every backend and an unmeasured pin only on Windows; test_network_isolation_structure_contract (69 tests) pins each case. Live control: the first manifest run named BPR-1327's and BPR-1340's unpinned rows with the new message; both were measured contained and pinned, and the manifest passes. tests/AGENTS.md carries the same-commit rule."}},{"criterion":"BPR-1343:windows-green","outcome":"pass","revision":"2979e0879","evidence":{"kind":"command","path":"tests/AGENTS.md","detail":"On Windows: architecture_boundaries, architecture_boundary_contract, test_network_isolation_structure_contract and test_network_isolation_manifest pass, and the two newly pinned rows pass uncontained; the merged tree passes the boundary verifier. macOS and Linux runs are owed to their stations."}}]
---

# BPR-1343: Cross-platform guards: a Qt adapter local that hides a QWidget data member, and a WINDOWS_UNCONTAINED row without a pin

## Observation

On 2026-10-08 main stopped compiling on Windows twice. In both cases a local
in a widget's member function was named `data`: [[BPR-1090]]'s
`writing_hooks_panel::selected_hook()` and [[BPR-1336]]'s
`appearance_settings_panel::code_zoom()`. The local hides `QWidget::data`, so
MSVC reports C4458, and the build treats warnings as errors. Clang and GCC do
not warn, so the implementer's own platform passed.

The same day, the Windows station pinned 15 rows that macOS and Linux
implementers had registered `WINDOWS_UNCONTAINED` without a Windows pin. On
those platforms the registration changes nothing, so the omission shows only
in Windows' `test_network_isolation_manifest`.

## Outcome

1. **A shadowing check in `architecture_boundaries` (every platform).** A
   local, parameter or capture in `src/adapters/qt` named after a data member a
   Qt base class carries is refused. MSVC's C4458 fires only for hidden *data*
   members: locals named `size`, `width` or `rect` hide member *functions* and
   do not warn, and the tree has many. So the list is short and justified:
   - `data` (`QWidget::data`), both breaks;
   - `d_ptr` (`QObject::d_ptr`, protected).

   A method named `data(...)`, such as a model's override, is not a local and is
   allowed. Comments and string literals do not count.
2. **Pin registration consistency (every platform).** The test wrapper marks
   every `WINDOWS_UNCONTAINED` registration with the label
   `bpr_windows_uncontained_registration` on every platform.
   `test_network_isolation_manifest` then refuses any such row without an entry
   in `WINDOWS_UNCONTAINED_TESTS`.
   - An entry `"unmeasured (registered on <platform>)"` is accepted off
     Windows. On Windows it still fails, until the station measures the row
     contained and records the measured refusal.
   - `tests/AGENTS.md`: registering `WINDOWS_UNCONTAINED` requires a pin entry
     in the same commit.

## Acceptance

- `architecture_boundaries` refuses a `data` or `d_ptr` local in
  `src/adapters/qt` and accepts the current tree, model `data()` overrides,
  `.data()` calls, comments and strings. The contract test pins both
  directions.
- The manifest refuses an unpinned `WINDOWS_UNCONTAINED` row on any backend,
  and an unmeasured pin only on Windows. The structure contract pins each case.
- Windows runs both checks green on the current tree. macOS and Linux runs are
  owed to their stations.

## Implementation (bpr-main-win, 2026-10-08)

**The shadowing check** (`cmake/VerifyArchitectureBoundaries.cmake`). Every
`.cpp` and `.hpp` under `src/adapters/qt` is read with comments, character
literals and string literals removed, and with a statement keyword before a
name treated as a use. A declaration naming `data` or `d_ptr` is refused, with
a message pointing at `src/adapters/qt/AGENTS.md`. The forms caught are:

- a typed or `auto` local, a parameter, or a range-for variable;
- a lambda capture.

A method named `data(...)`, `.data()` and `->data`, a qualified name, `aData`
and `metadata` pass. The list is justified in the comment: C4458 warns only for
hidden *data* members, and the tree's many locals named `size`, `width` or
`rect` hide member functions and build clean on MSVC.

**The pin-registration check.**

- `cmake/BprTestIsolation.cmake` adds the label
  `bpr_windows_uncontained_registration` to every `WINDOWS_UNCONTAINED` row,
  on every platform.
- `scripts/verify_test_network_isolation.py`'s `pin_registration_failures`
  runs on every backend. It refuses a labelled row without an entry in
  `WINDOWS_UNCONTAINED_TESTS`. On Windows it also refuses an entry beginning
  `unmeasured (registered on`.
- `tests/AGENTS.md` carries the rule: the pin comes in the same commit.

**Evidence** (bpr-main-win, the `windows-dev` tree, reconfigured).

- `architecture_boundaries` passes on the tree.
- `architecture_boundary_contract` passes: 148 tests, among them the new
  cases. Seven declaration forms are refused, and the eight allowed forms are
  accepted.
- `test_network_isolation_structure_contract` passes: 69 tests, among them the
  new cases.
  - An unpinned registration fails on both backends.
  - A measured pin passes on both.
  - An unlabelled row is ignored.
  - An unmeasured pin passes off Windows and fails on Windows.
- `test_network_isolation_manifest` passes once the two rows below are pinned.

**Controls on the live tree.**

- With the BPR-1336 `data` local put back in
  `appearance_settings_panel::code_zoom()`, `architecture_boundaries` refuses
  it: "Qt adapter declaration hides a Qt base class data member:
  src/adapters/qt/src/appearance_settings_panel.cpp names a local, parameter or
  capture `data`…". The file was restored afterwards.
- On its first run, the manifest's new check named two real unpinned rows:
  "application_large_review_prepared_open is registered WINDOWS_UNCONTAINED
  without an entry in WINDOWS_UNCONTAINED_TESTS…", and the same for
  `application_epoch_update_line_public_path`.

Those two rows were then measured contained and pinned `dos-volume-name`:

- [[BPR-1327]]'s prepared-open benchmark stops at "private data root parent".
- [[BPR-1340]]'s Updates-line course refuses startup at the registry.

Uncontained, both pass.

**Limits.** macOS and Linux runs of the two checks are owed to their stations.
Off Windows, an unpinned row now fails `test_network_isolation_manifest` there
too.
