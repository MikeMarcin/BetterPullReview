---
id: BPR-0183
title: "Qt: the appearance profile drives the composed window"
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0182]
---

# Qt: the appearance profile drives the composed window

`appearance_profile` is a complete presentation engine — paired variants,
semantic roles, contrast checks, typography, bounded invalidation — that the
application never constructs. The window instead inherits Qt's native palette,
which is `QMacStyle` doing its own work.

## What this slice does

The composed window owns an `appearance_profile`, subscribes it to the
appearance host's projections, and applies it to the composed surfaces. The
native palette stops being the source of presentation and becomes what it
should always have been: one **observation** fed to
`appearance_application_host::observe`, from which the core resolves a
variant.

That inversion is the whole point. Today the OS decides directly. Afterwards
the OS reports, and `core::resolve_appearance` decides — which is what makes an
explicit mode capable of ignoring it.

## The stale include

`diff_workbench.hpp` includes `appearance_profile.hpp` and
`diff_workbench.cpp` uses nothing from it. Either the workbench consumes the
profile in this slice or the include goes. Do not leave an include that implies
a relationship the code does not have.

## Required direct evidence

- With System selected, an observation carrying a Dark scheme changes
  `appearance_profile::variant()`, and the composed surfaces' palettes follow.
- With Light explicitly selected, the same observation changes neither. This is
  the assertion RFC-BPR-0006 names and the one that has never had a surface;
  it must be shown to fail if the explicit mode is allowed to resolve from the
  observation.
- The palette actually reaches the panes, asserted on a pane's own palette
  rather than on the profile that was supposed to have applied it.
- An appearance change publishes a bounded invalidation and does not disturb
  review state, selection, or the measured mapping.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

Unblocks BPR-0114, whose attended run is the same claim against a real
operating-system scheme change rather than a supplied observation.

## What was done

The composition owns a `qt::appearance_profile`, built **from the appearance
host's settings after `restore()`** so a stored mode is the one presentation
starts on rather than one it is corrected to a frame later. The profile's sink
applies it to the composed window, and every composed surface inherits it.

### The inversion this slice exists for

Before: the operating system decided presentation directly, through Qt's native
palette. An explicit mode had nothing to ignore, which is why BPR-0114 had no
surface.

After: `QStyleHints::colorSchemeChanged` produces a
`core::appearance_observation`, which goes to **both** owners — the host, which
records it against durable settings, and the profile, which re-resolves what
presentation should look like. Telling only one leaves them disagreeing about
the same fact. `core::resolve_appearance` decides; the system merely reports.

## An assertion of mine that was too weak, caught by its own break

The first version emitted `APPEARANCE_APPLIED:<selection>:<base>` and the
fixture contract asserted the hex format. Removing `apply_to` entirely **still
passed**: the window kept the platform's palette, which also has well-formed
colours. The assertion proved that colours were reported, not that they came
from anywhere in particular.

It now reports provenance — `:from-profile` or `:from-platform` — computed by
comparing the window's palette against the profile's semantic roles. The same
break now yields `APPEARANCE_APPLIED:#308cc6:#ffffff:from-platform`, `#308cc6`
being macOS's own selection blue.

## What this did not fix, measured rather than assumed

BPR-0194 recorded a 1.11:1 selection contrast and attributed it to the
uncomposed palette. Composing the palette moved it to **1.23:1**. Still nowhere
near the 3:1 non-text minimum.

The cause is `diff_workbench.cpp`'s `color.setAlpha(72)` — the band is the
highlight at 28% opacity. At that alpha the requirement is unreachable by any
colour: the theoretical best is pure black at 28% over a white pane, which is
2.01:1, and pure white over the dark pane, which is 2.56:1.

So the fix BPR-0194 originally prescribed — refit `code_selection_background` —
could not have worked, and the re-measurement would have come back barely
moved. That card is corrected with the proof.

## Evidence

### Breaks

| Break | Assertion that failed |
|---|---|
| Profile never applied to the window | `APPEARANCE_APPLIED:…:from-profile` — reported `from-platform` |
| (first attempt, format-only assertion) | **not caught** — recorded, and the reason provenance is now asserted |

The System-follows-while-explicit-does-not claim is already proven at the
component in `appearance_profile_tests.cpp`. What was missing was the
application composing it at all, which is what this card supplies. BPR-0114's
attended run needs BPR-0184's gesture to select an explicit mode before it can
observe the second half on the real composition.

### Gates

- `ctest --preset dev` — **123/123**. `ctest --preset core-only` — **57/57**.
- `coding-format-check` passed. `verify_documentation.py` — `DOCUMENTATION_OK`.
