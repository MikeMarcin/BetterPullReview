---
schema: "implement-rfc.card.v1"
id: "BPR-0104"
title: "Implement core appearance and typography policy"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0103"]
---

# Implement core appearance and typography policy

Add the deterministic core policy for appearance mode selection and typography settings: System, Light, Dark, and high-contrast resolution, and the UI and code font settings with their valid ranges.

The core decides which profile variant applies and whether a settings value is legal. It holds no `QFont`, `QColor`, Qt type, or KSyntaxHighlighting type, and the dependency scan must keep proving that.

## Implementation

`appearance_model` supplies the Qt-free core policy: appearance-mode
resolution, typography legality, and the closed semantic role vocabulary.

A colour is four named `std::uint8_t` channels in straight sRGB rather than a
packed word, because a packed representation forces every consumer to agree on
an undocumented byte order and alpha convention. Named channels make that class
of bug unrepresentable at the same size and cost. The type carries no
behaviour: deciding what a colour does is presentation work.

Roles are a closed 45-member enumeration whose count is pinned to the last
enumerator by `static_assert`, and a palette is a `std::array` over that count.
That makes a theme a *total* mapping over a fixed vocabulary — it can neither
omit a role nor hold a key outside one — which is exactly the property BPR-0108
needs to reject unknown fields rather than silently tolerate them.

Typography quantities are integer fixed point, so every legal value is exactly
representable and exactly comparable, and no floating point enters the core. A
rejection is the triple target, field, reason with no catch-all member, so every
refusal names an exact bound or an exact structural rule. `bounds_of` is both
the published source of the bounds and the function the validator enforces
with, so a settings surface built from it cannot offer a value the core would
then reject.

Resolution returns the variant plus orthogonal source facts, because the
variant alone cannot say why: a dark variant looks identical whether an
explicit mode chose it or an observation did. `unknown` is a distinct observed
scheme so a defaulted resolution is distinguishable from an observed one.

## Three judgement calls, all kept

The worker raised three open questions rather than deciding them silently, and
each is kept.

`high_contrast_system` goes one enumerator beyond a literal reading of "System,
Light, Dark, plus high-contrast variants". Following the operating system's
light/dark scheme and requiring high contrast are independent needs, and a
platform that reports a scheme but no contrast preference would otherwise leave
that combination unreachable. Omitting it would be an accessibility gap created
by enum minimalism.

The rejection triple is kept over a flat enumeration. A flat form would need
roughly twenty members to carry the same information, and the triple's
non-vacuity break targets exactly the typed-reason claim, so the evidence
stands either way.

`diff_hunk_header_text` is kept as a role distinct from its background. A hunk
header's text is not code and cannot take a syntax colour, so folding it into
one role would force a consumer to invent the second meaning.

## Direct verification

- `core_appearance_policy` passes; the full Qt-free core suite passes 36/36 in
  77.6 seconds; both architecture boundary suites and both core dependency
  boundary suites pass. The format gate and strict `clang-tidy` with
  `--warnings-as-errors` pass.
- The RFC's hard constraint is directly checked: no `QFont`, `QColor`,
  `QString`, `Qt::`, or KSyntaxHighlighting identifier appears in either new
  core file, and the dependency scan rejects filesystem, process, clock,
  thread, and network usage anywhere under the core.
- Non-vacuity, scheme following: making the resolution consult the observation
  unconditionally fails with "an explicit mode ignores the observed scheme that
  system mode follows". Reproduced independently, with the build confirmed to
  have recompiled the changed object rather than merely succeeded.
- Non-vacuity, typed reason: returning `above_maximum` from the below-minimum
  branch — still a rejection, still exactly once, so only the typed-reason
  claim is falsified — fails with "one step under the published minimum is
  rejected with its exact typed reason".

## Retained risks

- The syntax role granularity is a forward commitment. Adding a role later is
  additive; renaming or removing one changes identifiers that BPR-0108's
  exchange format will treat as a compatibility contract.
- The published bounds are policy rather than physics. BPR-0107 is the first
  slice that will feel them.
- `Family` deliberately has no default, so a default-constructed record is
  illegal on exactly the two family fields. The core will not invent a platform
  font identifier it cannot know. That is asserted rather than only documented.

## Result

Appearance mode resolution, typography legality, and the closed role vocabulary
are decided in a deterministic core that holds no presentation type. BPR-0104
is complete.
