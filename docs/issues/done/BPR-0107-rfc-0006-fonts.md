---
id: BPR-0107
title: Implement UI and code font settings
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0106]
---

# Implement UI and code font settings

Implement separate UI and code font family, size, weight, line height, tab width, ligature, bold, and italic settings with immediate preview and persistence.

A font change must invalidate the Qt-local measured axis and ribbon geometry while preserving semantic selection, then reproject the one shared comparison position without changing mapping or confidence. Prove the selection and mapping survive.

## Implementation

UI and code typography reach Qt presentation through free conversion functions
and a selection API that returns what was accepted and what was rejected.

No bound is restated. Every numeric value presentation composes passes through
a check that reads `core::bounds_of`, the same function `validate_typography`
enforces with, so presentation cannot offer a value the core would refuse.
Legality is decided only by the core: an offer carrying any rejection applies
nothing and clamps nothing, because a clamped value would tell a settings
surface its rejected input was accepted. A durable record with one illegal font
falls back per font, so an illegal code font cannot discard a legal UI font.

## The invariant, and how it was observed without widening scope

The acceptance criterion is that a font change invalidates the measured axis
and ribbon geometry *while preserving semantic selection*, then reprojects the
shared position without changing mapping or confidence. The enforcing code
lives in the workbench, outside this slice's file scope, so the invariant is
driven entirely through public surfaces: a real composed workbench, a published
comparison, a pinned position, and a code point-size change delivered only as a
Qt font event.

Geometry is required to move: workbench and link-map rebuild counts rise with
failures flat, the measured comparison extent grows, and the same ribbon
identity repaints at a larger extent. Semantics are required not to move: the
accepted result pointer is identical, the selection record compares equal, and
the line mapping is compared **byte for byte** through a serialiser covering
every identity and category code, with confidence asserted directly alongside.
Reprojection is distinguished from discard: the retained comparison and segment
are unchanged while the position and both panes' projected offsets move, so the
anchor was re-evaluated in new coordinates rather than dropped. Reverting the
font closes the round trip on the same mapping bytes.

## Direct verification

- The appearance-profile suite passes; the full Qt suite passes 25/25; both
  architecture boundary suites, the format gate, and strict `clang-tidy` with
  `--warnings-as-errors` pass. The tidy gate was run here because the
  delivering slice correctly noted it had not run it.
- Non-vacuity, bounds: applying a record the core rejected fails with "a point
  size one centipoint above the published maximum is rejected by field and
  reason". Reproduced independently with the recompile confirmed.
- Non-vacuity, geometry: suppressing the font event to the composed workbench,
  while still applying the font to the panes, fails with "the font change
  invalidates the Qt-local measured axis and rebuilds it" — and the preview
  cases stay green, so the failure isolates the composed-comparison claim.
- Non-vacuity, semantics: injecting a genuine selection change between the font
  application and the semantic block fails "the font change preserves the exact
  semantic selection". The byte comparison was then shown to be independently
  live rather than shadowed by that assertion: weakening only the selection
  check to a presence test made the run fail on "every byte of the line mapping
  is identical across the font change" instead. A byte-comparison that only
  ever runs behind a stricter assertion proves nothing on its own.

## Two settings left short, tracked in BPR-0116

Code line height is computed and unit-tested but never applied, because Qt's
plain-text document layout ignores block line height and honouring it needs the
surface that owns the document. The slice deliberately did not fake-apply it —
a setting that appears to work but does not is worse than one that visibly does
not, since it defeats the next reader's search for the bug.

A tab-width-only change does not invalidate measured geometry, because setting
an unchanged font emits no event and the workbench listens only for font and
viewport facts. Both need a typography publication the workbench subscribes to,
which is BPR-0116, and neither should be closed by inventing a synthetic font
change: tab width is not a font property, and forcing invalidation that way
would make the recorded reason for the rebuild untrue.

## Result

UI and code typography apply and preview immediately, bounded by the core's
published range with no second copy, and a font change moves geometry without
moving semantics — proven by byte-identical mapping rather than by an
invalidation counter alone. Two settings remain short and are tracked rather
than claimed. BPR-0107 is complete.
