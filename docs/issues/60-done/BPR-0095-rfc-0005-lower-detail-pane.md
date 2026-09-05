---
schema: "implement-rfc.card.v1"
id: "BPR-0095"
title: "Implement the full-width line-over-line lower detail pane"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0094"]
---

# Implement the full-width line-over-line lower detail pane

Render the lower detail pane across the central width, showing the selected group line over line. It re-resolves the canonical RFC-BPR-0004 mapping rather than keeping its own copy, and it agrees with the link map and pane selection on every stable semantic identity.

## What was already satisfied

The acceptance criterion needed no new pane. RFC-BPR-0004's comparison surface
already owns the detail pane, and BPR-0094 composed that surface into the
centre. Because the enclosing splitter is vertical and every layout down from
the central surface uses zero horizontal margins, the pane's left edge and
width are exactly the centre's. Its rows come from `core::detail_for_selection`
over the one canonical semantic index, so there is no second mapping that could
disagree.

What was missing was evidence, not behaviour. The criterion is now asserted
directly rather than assumed from composition.

## Accessibility defect found and fixed

Retiring the stale placeholder was not cosmetic. `workbench_window` still built
a `workbenchDetailPanePlaceholder` whose accessible name was
`Selected change detail` — byte-identical to the accessible name the real
composed pane sets. The window therefore exposed two widgets claiming one
accessible identity, and one of them was an empty `QWidget`. Assistive
technology would have had no way to tell which was the detail pane, in an RFC
whose acceptance criteria include accessibility.

The placeholder is removed, its layout slot with it, and the stale
`detail_pane_placeholder()` accessor is replaced by `detail_pane()`, which
forwards to the composed surface and adds no second detail authority. The
accessible name now has exactly one claimant and the placeholder identifier has
none.

## Direct verification

- `qt_workbench_window_scale_100`, `_125`, and `_200` pass offscreen and
  serial, plus both architecture boundary suites and the full 24-test Qt suite.
  Format and strict `clang-tidy` with `--warnings-as-errors` pass.
- The new case walks every canonical group through the real next-change action
  and compares the pane's rows against `core::detail_for_selection` for the
  live selection, resolving text from the immutable decoded document at the
  canonically mapped line numbers. It asserts nothing against hardcoded
  strings, so it cannot pass on plausible-looking content.
- Width is measured at all three scales across five drawer stages: both open,
  files collapsed, files reopened, conversations collapsed, conversations
  reopened. The pane is strictly wider than the comparison sub-region and never
  overlaps an open drawer.
- Non-vacuity, central width: adding a 40px right margin to the centre only
  when the conversations drawer collapses fails with "the lower detail pane
  spans the whole central width (conversation drawer collapsed)". Keyed to the
  conversations drawer specifically so an existing files-collapse assertion
  could not fail first and mask it. Reproduced independently after delivery.
- Non-vacuity, canonical mapping: shifting the reference lookup by one line
  fails with "the lower pane shows the canonically mapped earlier rows", so the
  assertion binds to the exact mapped line rather than to any plausible source
  text.

## Retained risks

- The line-over-line ordering assertion has teeth on paired-line groups. For
  pure insertion or deletion groups it asserts the present side's canonical
  rows and that the absent side is a canonical opposing boundary, without
  asserting vertical order, to avoid coupling the test to boundary wording.
- Evidence is deterministic offscreen layout. Native appearance integration for
  this surface remains the owner-attended BPR-0100.

## Result

The lower pane spans the central width across every drawer stage and shows the
selected group line over line from the one canonical mapping, now with direct
evidence. An accessible-name collision introduced by the earlier placeholder is
removed. BPR-0095 is complete.
