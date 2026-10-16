---
schema: "implement-rfc.card.v1"
id: "BPR-1555"
title: "Show WebP images in descriptions and comments through Qt's WebP plugin"
status: "In Progress"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"inline-markdown-images-r1","governing_rfc":"RFC-BPR-0084","amendment":"RFC-BPR-0084 owner amendment 1 (2026-10-16)","criteria":["AM1-02"],"origin":"owner accepted Q2 in attended session 8a5e3c244f674294366fd8796da2897e, cursor 17","reservation":"docs/issues/_reservations/BPR-1554--BPR-1556--0108f19c75d6.json"}
---

# BPR-1555: Show WebP images in descriptions and comments through Qt's WebP plugin

Under [RFC-BPR-0084 owner amendment 1](../../rfcs/RFC-BPR-0084-inline-markdown-images.md#owner-amendment-1-2026-10-16-answers-to-q1-q3):
WebP body images decode through Qt's `qwebp` plugin from `qtimageformats` (libwebp, BSD-3-Clause; the owner's library approval) within the same bounds as GIF.

## Acceptance

- **AC01 (AM1-02):** Decoder tests show still and animated WebP within the bounds and an over-bound one refused, with a negative control.
- **AC02:** The composed fixture course shows a WebP image inline.
- **AC03:** The pinned Qt provider acquires `qtimageformats` (amending an older cache in place); the package, release and capsule policies ship only `qwebp` from it; dependencies, notices and SBOM name QtImageFormats and libwebp.
