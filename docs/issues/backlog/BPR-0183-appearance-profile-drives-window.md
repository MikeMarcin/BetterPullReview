---
id: BPR-0183
title: "Qt: the appearance profile drives the composed window"
status: Backlog
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
