---
schema: "implement-rfc.card.v1"
id: "BPR-1554"
title: "Show SVG images in descriptions and comments through QtSvg"
status: "In Progress"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"inline-markdown-images-r1","governing_rfc":"RFC-BPR-0084","amendment":"RFC-BPR-0084 owner amendment 1 (2026-10-16)","criteria":["AM1-01"],"origin":"owner accepted Q1 in attended session 8a5e3c244f674294366fd8796da2897e, cursor 17","reservation":"docs/issues/_reservations/BPR-1554--BPR-1556--0108f19c75d6.json"}
---

# BPR-1554: Show SVG images in descriptions and comments through QtSvg

Under [RFC-BPR-0084 owner amendment 1](../../rfcs/RFC-BPR-0084-inline-markdown-images.md#owner-amendment-1-2026-10-16-answers-to-q1-q3):
SVG body images, such as README badges, render through QtSvg's renderer after the core admits them as self-contained and bounded, rasterized statically at twice their size.

## Acceptance

- **AC01 (AM1-01):** Core and decoder tests show an admitted SVG rasterized at its size and twice its density, and each refused construct (external `href`, external `url()`, entity, script, `foreignObject`, `@import`, `@font-face`, over-bound size, elements or depth) ending in its state, with a negative control.
- **AC02:** The composed fixture course shows an SVG badge inline; the live course shows GitHub's camo-served SVG badge.
- **AC03:** The pinned Qt provider requires QtSvg; the package, release and capsule policies deploy the QtSvg library and no SVG plugin; notices and SBOM name QtSvg.
