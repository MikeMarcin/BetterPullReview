---
schema: "implement-rfc.card.v1"
id: "BPR-0327"
title: "Match the application icon Dock footprint"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0326"]
---

# Match the application icon Dock footprint

The rounded icon still occupied roughly 79–80 pixels beside neighboring Dock
tiles that occupied approximately 71–72 pixels. Reducing only the circular
motif did not correct that outer optical mismatch.

## Required final state

- Scale the complete rounded tile, including its background, border, disk, and
  glow, to match the neighboring Dock footprint.
- Retain the disk-to-tile proportions and rounded-corner treatment established
  by BPR-0326.
- Regenerate the complete PNG, Windows ICO, and macOS ICNS family.
- Pin the outer optical scale in direct asset evidence.

## Completion record

- The complete icon is centered and scaled to `0.875`, creating a 64-unit
  transparent margin on every side of the 1024-unit canvas.
- The disk retains its internal `0.84` scale; combined with the outer optical
  fit, its effective pre-glow diameter is 64.9% of the complete canvas.
- The direct asset contract pins both scale factors, the rounded mask, source
  digest, transparent first pixel at every PNG size, platform containers, and
  runtime bindings.
- A second generator pass reproduced every derivative digest. The focused
  asset, application-icon, and tray tests, production macOS bundle comparison,
  production self-test, and documentation gate pass.

## Non-goals

- Do not redraw, recolor, or change the internal wave composition.
- Do not change the application name, bundle metadata, or platform bindings.
- Do not claim signed, notarized, installable, or release-ready packages.
