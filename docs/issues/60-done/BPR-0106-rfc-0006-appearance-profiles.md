---
schema: "implement-rfc.card.v1"
id: "BPR-0106"
title: "Implement selectable appearance profiles with immediate preview"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0105"]
---

# Implement selectable appearance profiles with immediate preview

Implement System, Light, Dark, and high-contrast profiles in the Qt adapter with immediate preview. A fresh installation defaults to System.

System mode follows the operating-system color scheme; an explicit mode does not. The static fresh-install default is unattended; the live scheme-change behaviour needs an owner-attended toggle and belongs to its own card.

## Implementation

The four built-in profiles the RFC names — Light, Dark, High Contrast Light,
High Contrast Dark — are each a total `appearance_palette` over the closed role
vocabulary. Because the palette is an array over a `static_assert`-pinned role
count, a profile that forgot a role would not compile, so totality is a
structural property rather than a test obligation.

The adapter turns a resolved variant plus a palette into applied presentation.
It does not re-decide the mode: `resolve_appearance` in the core owns that, and
this layer consumes the result. Semantic colours are supplied without
installing a forced style or QSS, preserving the repository's existing contract
that the application inherits the native style and interface font.

A fresh installation resolves to System. That is a default-value property
proven here without any appearance change; the live operating-system scheme
transition is deliberately not claimed and belongs to the owner-attended
BPR-0114.

## Direct verification

- The appearance-profile suite passes; the full Qt suite passes 25/25; both
  architecture boundary suites, the format gate, and strict `clang-tidy` with
  `--warnings-as-errors` pass.
- Non-vacuity, fresh-install default: setting the default mode to Dark fails
  with "a fresh installation with no persisted value selects System".
- Non-vacuity, profile distinctness: returning the ordinary light palette for
  the high-contrast light variant fails with "a high-contrast profile is not a
  copy of its counterpart". A high-contrast profile that silently duplicated
  its ordinary counterpart would satisfy a totality check while providing no
  accessibility benefit at all, so distinctness needed its own assertion.

## A misleading red worth recording

The first attempt at the distinctness break returned `BUILD-EXIT=1` — the
unused `high_contrast_light_profile` tripped `-Werror` — and the suite then
reported a failure whose message belonged to the *previous* break, "a fresh
installation with no persisted value selects System". That is the stale-binary
trap producing a misleading red rather than a misleading green, which is the
more insidious direction: a red is easy to accept as confirmation that the
check worked. The break was rewritten into a compiling form that consumes the
symbol, and only then did the intended assertion fail. Checking `BUILD-EXIT`
before reading a failure matters exactly as much as before reading a pass.

## Retained risks

- Colour choices are policy. The built-in palettes are legible and conventional
  but are not yet checked against a documented minimum contrast ratio; BPR-0111
  owns that and will be the first slice to test them numerically.
- Presentation application is proven offscreen. Native appearance integration
  remains outside this slice.

## Result

Four total built-in profiles apply through a core-resolved variant without
forcing a style, preview immediately, and default to System on a fresh
installation. BPR-0106 is complete.
