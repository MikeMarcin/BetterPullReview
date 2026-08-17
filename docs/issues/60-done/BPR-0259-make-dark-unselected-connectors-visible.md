---
id: BPR-0259
title: Make dark unselected connectors visible
status: Done
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

## Implementation checkpoint

Repair commit `8b1108cc957225ef56402870daf2a9975feba8b6` changes only
the connector paint roles: ordinary ribbons use the live window-text role at
bounded translucency, while selected ribbons retain their highlight fill and
add a window-text edge. Connector paths, endpoints, semantic identities,
selection, ordering, and navigation are unchanged.

The exact six-test selector passed 6/6 in 103.31 seconds. The post-commit real
UIforETW capture passed in 84.96 seconds. Its actual composed map pixels measure
5.33:1 median contrast for the unselected Dark connector and 6.57:1 for the
unselected High-contrast Dark connector; 840/1008 samples clear 3.00:1 in each
profile. The light profiles also clear the floor. Selected connectors retain
250 above-floor edge samples and a distinct dominant raster. An independent
read-only closeout review found no blocking implementation issue and confirmed
that the immutable v004 pixels fail the new actual-raster gate.

Fresh immutable v005 is
`bpr:/attended-review/betterpullreview/0075b116efe8d055d1ddc0f5`. Its sole
subject is
`criterion:/betterpullreview/bpr-0259/dark-connector-visibility` at
`sha256:b1cd509bea7eb23a25626c0b819d3eec9a4506ffa42f0c79b2d5d598185b40f6`.
The package carries no issue, release, or RFC closeout claim. This card is
parked in Backlog while the exact exported human verdict is pending; v003 and
v004 remain immutable rejections.

## Attended result

The exact v005 export was validated against its immutable batch manifest and
recorded at
`docs/verification/reviews/uiforetw-intraline-acceptance-v005/`. The product
owner accepted
`criterion:/betterpullreview/bpr-0259/dark-connector-visibility` at
`sha256:b1cd509bea7eb23a25626c0b819d3eec9a4506ffa42f0c79b2d5d598185b40f6`
without notes. This accepts only the digest-bound visual criterion: selected
and unselected connectors are clear in the exact v005 profiles while the
compact row, arrows, endpoints, and focused comparison remain coherent.

The returned export explicitly leaves BPR-0257, BPR-0258, and BPR-0259 open
and makes no release or RFC-closeout claim. The accepted subject therefore
moves this card out of the attended-wait lane, but does not by itself close
any issue or broader gate. The v003 and v004 rejections remain immutable.

## Terminal closure — 2026-08-11

The integrator independently reconciled the accepted v005 subject with the
card's required final state and current committed tree. After rebuilding the
developer graph so the executable matched capture schema v4, the exact six-test
selector passed 6/6 in 109.68 seconds. Documentation validation reported
`DOCUMENTATION_OK`, and `git diff --check` passed. This tracker transition is
the issue closeout that the attended verdict deliberately did not perform; it
makes no BPR-0257, BPR-0258, release, or RFC closeout claim.
