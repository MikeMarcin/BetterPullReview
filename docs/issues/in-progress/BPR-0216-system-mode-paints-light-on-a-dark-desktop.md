---
id: BPR-0216
title: System mode paints the light palette on a dark desktop
status: In Progress
rfc: RFC-BPR-0006
depends_on: [BPR-0183, BPR-0186]
---

# System mode paints the light palette on a dark desktop

Found by the product owner on 2026-08-08, in the second attended launch of the
composed shell. macOS in Dark, mode `system`, and the entire application content
rendered light. Only the native window frame and the unified toolbar area were
dark, because macOS draws those and not the widget palette.

Every marker was green.

## Measured

```
APPEARANCE_SETTINGS:system:4
APPEARANCE_OBSERVED:dark:dark:46
APPEARANCE_APPLIED:#7fb0e8:#ffffff:from-profile
```

The observation is `dark`. The resolved variant is `dark`. The applied selection
`#7FB0E8` and base `#FFFFFF` are the **light** palette. And **46** roles — every
role in the vocabulary — are recorded as customised, on a machine whose
`appearance_settings` table is empty.

## The mechanism

Two baselines that were never the same one.

```cpp
// default_appearance_settings(), baseline A
settings.Palette = builtin_appearance_palette(
    core::resolve_appearance(settings.Mode, core::appearance_observation{ }).Variant);
```

An intentionally empty observation, so `system` resolves to **light**. A fresh
record therefore carries the light palette whatever the desktop is doing.

```cpp
// appearance_profile's constructor, baseline B
const auto& builtin = builtin_appearance_palette(
    core::resolve_appearance(Mode, Observation).Variant);
if (aSettings.Palette[index] != builtin[index]) { Overrides[index] = aSettings.Palette[index]; }
```

The real observation, so on a dark desktop `builtin` is the **dark** palette.
Every role differs from the light record, so every role is banked as a
deliberate override pinning its light colour. `rebuild()` then lays those 46
overrides over a correctly resolved dark variant.

The constructor's own comment states the intent — *"A fresh record, whose
palette is that profile, therefore carries no customisation at all"* — and it is
exactly right about what should happen. It is only wrong about which profile a
fresh record's palette is.

## Why every marker stayed green

`variant()` reports `dark`, which is true. `AppliedFromProfile` reports that the
window's colours came from the profile's semantic roles rather than the platform
style, which is also true. Both claims hold while the product is visibly wrong,
because neither asks whether the colours are the *variant's* colours.

`APPEARANCE_OBSERVED` was added while diagnosing this and reports observation,
resolved variant and customised-role count together. The `46` is the assertable
fact: a fresh installation must report `0`.

## Required direct evidence

- On a dark desktop with an empty settings record and mode `system`, the applied
  base and selection are the dark palette's, and the customised-role count is
  `0`.
- The same on a light desktop, resolving light.
- A genuinely customised role still survives a restart and still reports as
  customised, so the fix does not buy correctness by discarding overrides.
- The count is asserted, not merely reported. A fresh installation reporting any
  non-zero customisation is the defect.

## What must not be done instead

Comparing against baseline A in the constructor so the counts cancel. That makes
a fresh record read as uncustomised by accident of matching defaults, and leaves
a genuinely stored light palette on a dark desktop indistinguishable from no
record at all. The two cases are different and the record has to be able to say
which it is.

Treating this as a presentation bug in `rebuild()`. The rebuild is doing exactly
what it was told; it was told 46 lies.
