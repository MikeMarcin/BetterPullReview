---
schema: "implement-rfc.card.v1"
id: "BPR-0273"
title: "Keep toolbar hover from resizing the workbench"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0191"]
---

# Keep toolbar hover from resizing the workbench

The shipped workbench gives its status label height-for-width behavior. When
Qt temporarily replaces that label with a toolbar action's status tip on
pointer hover, the native status bar recomputes its height and moves the
central review surface. Leaving the action restores the earlier geometry, so
ordinary pointer travel makes the whole workbench jump vertically.

## Required final state

- Entering or leaving any primary toolbar action leaves the status bar and
  central review surface at exactly the same geometry.
- Action status tips remain visible, and the retained workbench status returns
  unchanged after the pointer leaves the action.
- Status presentation remains native, selectable, accessible, and derived from
  the existing Qt-local wording.
- A Qt regression test drives the real temporary-status-message path and fails
  if either status-bar height or central-surface geometry changes.

## Non-goals

- Do not change core command legality, host projections, status wording,
  toolbar order, shortcuts, icons, or focus behavior.
- Do not add QSS, a forced style, a custom status bar, or fixed pixel metrics.
- Do not broaden the change into drawer, epoch, diff, or detail-pane layout.

## Verification

```sh
cmake --build --preset dev --target test_better_pull_review_qt_workbench_window_tests
ctest --preset dev --output-on-failure -R '^qt_workbench_window_scale_(100|125|200)$'
cmake --build --preset dev --target coding-format-check
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Closeout

The workbench status label is now an explicitly single-line, horizontally
compressible status-bar child. Qt can still replace it with every action's
temporary status tip, but neither presentation changes the status bar's height
or the central review surface's geometry. The regression drives the real
`QStatusTipEvent` enter/leave course, observes the temporary message, and
requires the retained status and both geometries to remain exact.

The test was first run against the old wrapped-label behavior and failed on the
geometry assertion. After the repair, all three 100, 125, and 200 percent
workbench-window tests passed. The 373-file formatting gate, 227-translation-
unit strict tidy gate, documentation verifier, and diff check also passed.
There is no core, host, authority, persistence, command, wording, or public
documentation impact; this is Qt-local layout behavior owned by RFC-BPR-0005.
