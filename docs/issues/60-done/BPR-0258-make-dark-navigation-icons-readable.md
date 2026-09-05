---
schema: "implement-rfc.card.v1"
id: "BPR-0258"
title: "Make dark navigation icons readable"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0246"]
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

## Implementation checkpoint

Repair commit `e10f553f54072b6761b99140cc204b5ebbf9590b` preserves each
native `QStyle` arrow silhouette and recolors its explicit normal and disabled
modes from the live toolbar palette. The focused selector passed 6/6 in
104.47 seconds across scales 1.0, 1.25, and 2.0, all four appearance profiles,
the real UIforETW capture path, and workbench composition. The post-commit
UIforETW capture passed in 78.98 seconds. Dark measures 13.84:1 normal and
4.45:1 disabled; High-contrast Dark measures 21.00:1 and 5.24:1. An independent
read-only closeout review found no blocking implementation issue.

The fresh immutable attended package is
`bpr:/attended-review/betterpullreview/9cdaa26a715d8f0168dc3049`. Its sole
subject is
`criterion:/betterpullreview/bpr-0258/dark-navigation-icon-readability` at
`sha256:778c9d4b6386274686e6abe37310142dcb1ddf8d56519d0786aa17549ea11463`.
It carries no closure or release claim. This card is parked in Backlog while
the exact exported human verdict is pending; the rejected v003 batch remains
unchanged.

## Attended result

The exact v004 export was validated against its batch manifest and recorded at
`docs/verification/reviews/uiforetw-intraline-acceptance-v004/`. The product
owner rejected the criterion with this note:

> The arrows look good now but the dark mode mode swoosh connectors are too
> hard to see except for the selected one.

This confirms the bounded arrow repair visually, but it does not accept the
combined compact-review criterion or close this card. BPR-0259 owns the newly
exposed unselected-connector contrast defect. The v004 rejection remains
immutable and no acceptance is inferred from the positive sub-observation.

## Fresh batch prepared — 2026-08-18

Done [[BPR-0259]]'s connector repair was accepted in isolation in v005, so the
defect that rejected v004 is answered in current bytes. The new immutable
combined batch
`bpr:/attended-review/betterpullreview/73e0d5752e89264d9731fe8a` at
`docs/verification/reviews/uiforetw-intraline-acceptance-v006/` binds captures
from commit `0e34519` and presents this card's
`criterion:/betterpullreview/bpr-0258/dark-navigation-icon-readability`
subject with fresh measured contrast: Dark 13.84:1 normal and 4.45:1 disabled,
High-contrast Dark 21.00:1 and 5.24:1. The focused matrix passed 6/6 in
116.30 seconds at those bytes. The exact exported verdict remains the only
open dependency.

## Attended result — accepted, 2026-08-18

The product owner accepted the exact
`criterion:/betterpullreview/bpr-0258/dark-navigation-icon-readability`
subject in the v006 batch. The complete verdict validates against batch
`bpr:/attended-review/betterpullreview/73e0d5752e89264d9731fe8a` and is
recorded at `docs/verification/reviews/uiforetw-intraline-acceptance-v006/`.
The v004 rejection remains immutable history; its cause was repaired by done
BPR-0259 and the readability criterion is now accepted at current bytes, so
the card moves to `60-done/`.
