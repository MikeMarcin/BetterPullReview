---
id: BPR-0259
title: Make dark unselected connectors visible
status: In Progress
rfc: RFC-BPR-0006
depends_on: [BPR-0246]
---

# Make dark unselected connectors visible

The product owner's exact v004 verdict confirms that BPR-0258 repaired the
Previous/Next arrows, but rejects the compact-review criterion because the
unselected swoosh connectors are too hard to see in Dark. Only the selected
connector remains readily visible. This card repairs that contrast defect
without changing connector semantics, endpoints, selection, or layout, then
creates a new immutable attended batch. The v003 and v004 rejections remain
history.

## Required final state

- Every grounded unselected connector remains readily visible against the
  gutter in Dark and High-contrast Dark.
- The selected connector remains clearly distinguishable from unselected
  connectors without making either state disappear.
- Light and High-contrast Light remain coherent, and connector endpoints still
  meet the exact left/right target rows at their pane edges.
- Selection, mapping, change navigation, intraline roles, compact row, drawer
  behavior, dimensions, accessibility, and arrow rendering do not regress.
- Direct rendered-pixel evidence measures the actual connector strokes in the
  composed capture for selected and unselected states across all four profiles;
  a non-null pen or palette value alone is insufficient.
- The real UIforETW course produces a new immutable attended batch. No v003 or
  v004 verdict carries forward.

## Non-goals

- Do not change diff alignment, change groups, connector geometry, endpoint
  ownership, or selected-change semantics.
- Do not weaken the selected connector to make unselected strokes look
  relatively stronger.
- Do not change the palette-aware navigation arrows accepted as visually good
  in the v004 note.
- Do not overwrite or reinterpret either prior rejection.

## Verification

```sh
ctest --preset dev -R '^(qt_diff_workbench_scale_(100|125|200)|qt_appearance_profile|application_uiforetw_selected_diff_contract|application_workbench_composition_contract)$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

Before implementation, confirm the exact Qt owner of selected and unselected
connector paint roles and the nearest existing rendered-raster assertion. The
card remains open until a fresh digest-bound verdict is validated and recorded.
