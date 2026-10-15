---
schema: "implement-rfc.card.v1"
id: "BPR-1515"
title: "Add a keyboard shortcut for Accept File"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Need a hotkey for accept file","related_cards":["BPR-1507"],"milestone_plan":{"id":"bpr-1515-accept-file-hotkey","path":"docs/rfcs/plans/bpr-1515-accept-file-hotkey/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9d87846a9eb309b9e7916e45e28a48ce9ffe01df","evidence":{"kind":"observation","detail":"Direct offscreen --review-state-icon passes actual QWindowSystemInterface key delivery to the existing QAction, with the exact projected current file and material and no opened menu."}},{"criterion":"AC2","outcome":"pass","revision":"9d87846a9eb309b9e7916e45e28a48ce9ffe01df","evidence":{"kind":"observation","detail":"The retained menu action and button tooltip show the native key; all assembled QAction/QShortcut bindings were checked for collisions. Editable QLineEdit/QPlainTextEdit/QTextEdit focus removes the binding, and ordinary Select All remains functional. Light/dark menu sheet inspected."}},{"criterion":"AC3","outcome":"pass","revision":"9d87846a9eb309b9e7916e45e28a48ce9ffe01df","evidence":{"kind":"observation","detail":"The focused key test covers no file, excluded, explicitly disabled projection, authority clearing/replacement, replacement file/material and unavailable ledger. Repeated presses after accepted projection dispatch no Revoke; auto-repeat is disabled. Explicit button Revoke remains available."}},{"criterion":"AC4","outcome":"pass","revision":"fce3377fb9da2e54d0bd1529ed2a29839c1f005b","evidence":{"kind":"command","detail":"Coordinator built app_better_pull_review successfully and ran six focused CTest rows on stable combined product fce3377fb (only metadata changed after, at5a105579c). application_review_ledger_composition_contract passed7.76s and explicitly requires REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK for actual window-system Accept key, durable receipt and repeat safety. application_diff_workbench_public_path passed4.44s; implementer inspected its full-window split-diff render through Git/cache actors. review-state1.08s, title3.85s, roundtrip3.23s and navigation-entry32.28s also passed. Total6/6,52.67s; coordinator log /tmp/bpr-coordinator-hotkey-layout-tests.log."}}]
---

# BPR-1515: Add a keyboard shortcut for Accept File

Provide a discoverable keyboard shortcut for accepting the current file. Reuse the same semantic action and eligibility as the file Accept control. Choose a shortcut consistent with existing platform conventions after checking collisions.

## Acceptance

- AC1: An eligible current file can be Accepted with a keyboard shortcut, without opening the file action menu.
- AC2: The shortcut is discoverable in the existing action/menu or shortcut presentation and does not collide with existing bindings or steal normal text editing input.
- AC3: No-current-file, excluded/unavailable, stale authority and other ineligible states follow the existing Accept action policy. Repeated presses must not inadvertently revoke acceptance unless the established shortcut contract explicitly calls for a toggle.
- AC4: Verify actual key delivery through the composed window, matching current file/epoch acceptance and focused shortcut collision/eligibility cases.

## Coordination

BPR-1507 owns the one-click Accept control; assign this related card to its implementer and share the canonical action. Keep card acceptance/evidence distinct. Load frontend-design:frontend-design before any UX edits per AGENTS.md. Never operate or capture the owner’s live desktop.

## Plan and design

Mandate `owner-2026-10-14`, baseline `583eb4689`; BPR-1507 composition
reconciled from coordinator through `01b55a14c`. Exclusive worktree
`/Users/jaeger/.codex/worktrees/bpr1515-accept-hotkey/BetterPullReview`, branch
`bpr-1515-accept-file-hotkey`. The detached initial plan/claim commits were
immediately retained on this branch before product edits.

Loaded implement-rfc, manage-agent-skills and frontend-design. MASK registration
was reviewed/applied in the coordinator's serialized slot; preflight reports
`ready_pinned` for the configured Codex implementer route, without claiming host
loading from configuration. No RFC is being reopened; RFC-BPR-0007's exact
material receipt and durable completion contracts remain authoritative.

The bounded design preserves the existing native palette, type and review-bar
layout. One existing QAction owns both button/menu and key. Command+Shift+A
(Ctrl+Shift+A on other platforms) uses the platform command modifier plus the
initial of Accept; the assembled shortcut inventory has no existing binding.
The shortcut appears only while that action means Accept, is absent in editable
single/multi-line fields, and does not auto-repeat. Revoke remains explicit.
No new persistence, authority, core policy or remote operation is introduced.

Verification targets actual platform key delivery in the rendered workbench,
text focus and collision checks, plus the existing production ledger course
through Git/cache actors and SQLite. No human-only acceptance criterion exists.

## Candidate and verification

Product candidate `9d87846a9eb309b9e7916e45e28a48ce9ffe01df`; final combined product
`fce3377fb9da2e54d0bd1529ed2a29839c1f005b`. AC1–AC4 pass in
[acceptance.json](../_checkpoints/BPR-1515/acceptance.json). The coordinator
explicitly owned the combined app build to avoid a duplicate full app build in
this disk-limited session. No human-only acceptance criterion exists.

On macOS 26.6.2 arm64, Qt 6.11.2:

- Configured `dev` offline with the designated shared CPM cache and Debug
  C/C++ flags `-g0`; built `test_better_pull_review_qt_workbench_window_tests`
  with parallelism 2. A CMake regeneration after the test harness change
  required one incremental rebuild, which also completed successfully.
- Direct offscreen `--review-state-icon` passes. CTest `qt_review_state_icon`
  passes (1.20 s), including window-system delivery of Command+Shift+A,
  exact file/material, accepted-repeat safety, menu/tooltip discovery,
  assembled collisions, all three text editor types, ordinary Select All,
  excluded/loading/replacement/explicitly-disabled/unavailable eligibility.
- `qt_workbench_focus_order_scale_100` passes (0.31 s).
- `architecture_boundaries` passes (19.77 s).
- Documentation gate passes; 67 documentation tests pass. Clang-format
  22.1.8 checks all four touched C++ files; `git diff --check` passes.
- Inspected the offscreen light/dark progress/menu sheet and full Qt window.
  The menu visibly names Command+Shift+A for Accept. These are deterministic
  component renders; production source-diff rendering remains with AC4.

Self-review extracted shortcut-only focus refresh so it cannot re-enable a
canonical receipt action or erase a public-preview refusal. The same retained
QAction owns click, menu and key; there is no added semantic path. Revoke has
no key, and holding a key cannot auto-repeat. Both test-only Qt private header
use and key delivery are excluded from tests-disabled composition.

The coordinator's stable combined app build passed, then all six focused
integration rows passed (52.67 s total): ledger composition 7.76 s, full public
diff path 4.44 s, review-state 1.08 s, title row 3.85 s, review round trip
3.23 s and navigation entry 32.28 s. The ledger contract requires
`REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK` from the actual composed key/repeat
course. The implementer inspected the newly generated full-window split diff:
review.cpp, selected lines 39/42, Accept control and state menu in the integrated
layout. Product source `fce3377fb9da2e54d0bd1529ed2a29839c1f005b` remained stable through these tests;
`5a105579c` only imported the separate sibling draft-sharing regression card.
That unrelated regression is tracked as BPR-1521 and is not attributed to this
shortcut; the shortcut's local/public checks passed.

Coordinator authorized importing that metadata-only allocation commit as
`cc83e040e` to satisfy the shared-ref counter guard. This implementer authored no
NEXT_ID or reservation change and made no manual lane move. Final canonical
finish records this accepted slice; no pending human/native/independent gate,
protected candidate, review server, waiter or background command remains.
