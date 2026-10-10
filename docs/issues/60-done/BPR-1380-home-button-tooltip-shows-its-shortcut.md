---
schema: "implement-rfc.card.v1"
id: "BPR-1380"
title: "Home button tooltip shows its shortcut"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"The Home button displays its existing shortcut in native notation","outcome":"pass","revision":"157a03f6e with workbench_window.cpp SHA-256 4ad17ca595b7a7852f5e6f24140e75a0f7b58189b96d061da1d50461fef56c34","evidence":{"kind":"observation","detail":"Offscreen actual-workbench probe sends QHelpEvent to the Home toolbar button; rendered QLabel contains ⌘0. Original tooltip negative control fails the rendered-text assertion."}},{"criterion":"Home retains its shortcut and navigation, and tooltip projections agree","outcome":"pass","revision":"157a03f6e with workbench_window.cpp SHA-256 4ad17ca595b7a7852f5e6f24140e75a0f7b58189b96d061da1d50461fef56c34","evidence":{"kind":"observation","detail":"Actual button/action/status tip/accessibility description agree; shortcut remains Ctrl+0. qt_workbench_window_scale_100 and architecture_boundaries pass; clang-format 22.1.8, git diff --check and documentation gate pass."}}]
---

# Home button tooltip shows its shortcut

The owner asked on 2026-10-10 to work directly and show the Home button's
existing key combination in its tooltip. Use the action's native shortcut
text, as the adjacent toolbar actions do.

## Acceptance

- The Home button's displayed tooltip includes its action's current shortcut
  in the platform's native notation.
- Home retains its current shortcut and navigation behavior; the status tip
  and accessible description use the updated tooltip.

## Scope and verification

One Qt presentation wording change. Check the actual toolbar button and its
offscreen rendered tooltip, run the focused workbench checks, and format the
touched source with clang-format 22. No human judgment criterion is needed.
The required frontend-design skill was loaded; the existing native tooltip
appearance and action vocabulary govern this change.

## Evidence — 2026-10-10

- Direct execution, with no delegated agents. Authored plan discovery returned
  `absent`; this card defines the slice's outcome and acceptance.
- The Home action inserts `shortcut().toString(QKeySequence::NativeText)` in
  its existing explanation. Its shortcut remains `Ctrl+0`, presented as ⌘0
  by Qt on macOS. Existing status-tip and accessible-description projection
  uses the updated text.
- A disposable probe linked to the actual workbench target sends a tooltip
  event to its visible Home button. The offscreen rendered label reads
  `Show Home (⌘0): sign-in or the review queue, and the way back to the review.`
  The action, button, status tip and accessible description agree. Restoring
  the original tooltip makes the same rendered-text assertion fail.
  The tooltip image was inspected; probe sources, images and logs remain in
  the ignored build tree.
- `cmake --build --preset dev --target test_better_pull_review_qt_workbench_window_tests -j 6`:
  pass. The stale build cache required a dependency rebuild; the initial
  two-job build was deliberately stopped through its own process ID before
  resuming with six jobs.
- `ctest --preset dev -R '^qt_workbench_window_scale_100$' --output-on-failure`:
  pass (26.30 s). `architecture_boundaries`: pass (50.13 s).
- clang-format 22.1.8 on the touched source, `git diff --check`, and
  `python3 scripts/verify_documentation.py --root .`: pass.
- The workbench specification records the shortcut in the Home tooltip;
  the RFC-BPR-0005 ledger records the presentation-only documentation impact.

The runtime observation is macOS arm64 offscreen. Other platforms use the
same action's native formatter; no new native Windows or Linux observation
is claimed.
