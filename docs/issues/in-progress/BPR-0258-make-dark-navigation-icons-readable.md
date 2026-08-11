---
id: BPR-0258
title: Make dark navigation icons readable
status: In Progress
rfc: RFC-BPR-0006
depends_on: [BPR-0246]
---

# Make dark navigation icons readable

The product owner rejected BPR-0257's immutable v003 attended batch because
the dark-mode Previous/Next file and change icons are black against the dark
toolbar. The compact row, order, semantic routes, and focused allocation are
otherwise retained. This card repairs only the navigation-icon rendering and
creates a new immutable attended decision; the v003 rejection remains history.

## Required final state

- Previous file, Next file, Previous change, and Next change use palette-aware
  foreground pixels with readable contrast against the toolbar in Dark and
  High-contrast Dark, while remaining legible in both light profiles.
- Normal and disabled icon states remain distinguishable and refresh after
  palette, style, theme, or font changes.
- Files, Conversations, Review details, and Private progress remain visually
  distinct; the exact one-row order and 39-pixel allocation do not regress.
- Tooltips, accessible names/descriptions, focus behavior, shortcuts,
  projection-driven enabled state, and exact stop-versus-wrap routing remain
  unchanged.
- The real UIforETW course captures a new immutable batch across all four
  appearance profiles with mechanical navigation-icon contrast evidence. No
  v003 verdict carries forward.

## Non-goals

- Do not change diff semantics, selected material, connectors, intraline
  colors, drawer behavior, or navigation policy.
- Do not replace semantic actions with tests-only widget manipulation.
- Do not overwrite or reinterpret the v003 rejection.

## Verification

```sh
ctest --preset dev -R '^(qt_workbench_window_scale_(100|125|200)|qt_appearance_profile|application_uiforetw_selected_diff_contract|application_workbench_composition_contract)$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

The new attended package must bind the committed icon implementation and all
four new full-window captures. The card remains open until its exact exported
verdict is validated and recorded.
