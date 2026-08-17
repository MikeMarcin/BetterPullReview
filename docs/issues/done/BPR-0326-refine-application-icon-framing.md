---
id: BPR-0326
title: Refine the application icon framing
status: Done
rfc: RFC-BPR-0014
depends_on: [BPR-0324]
---

# Refine the application icon framing

The first Dock rendering of the replacement icon made the circular wave motif
read larger than adjacent application marks. The square background also kept
opaque corners instead of presenting as a rounded application tile.

## Required final state

- Reduce the circular motif while preserving its geometry, palette, and
  center alignment.
- Clip every layer to a rounded outer mask with transparent corners.
- Regenerate the complete PNG, Windows ICO, and macOS ICNS family from the
  revised canonical vector.
- Prove the selected optical scale and corner radius cannot regress silently.

## Completion record

- The complete disk stack is centered and scaled to `0.84`, reducing its
  effective diameter from 88.3% to 74.2% of the source canvas before glow.
- A 190-unit rounded mask clips the background, disk haze, and border, leaving
  real alpha at all four corners in the rendered asset family.
- The direct asset contract pins the revised SVG digest, 1024-square view box,
  rounded mask, optical scale, derivative sizes, platform containers, and
  runtime bindings.
- A second generator pass reproduced every derivative digest. The focused
  asset, application-icon, and tray tests, production macOS bundle comparison,
  production self-test, and documentation gate pass.

## Non-goals

- Do not redraw or recolor the supplied wave paths.
- Do not change the application name, bundle identifier, executable name, or
  platform bindings.
- Do not claim signed, notarized, installable, or release-ready packages.
