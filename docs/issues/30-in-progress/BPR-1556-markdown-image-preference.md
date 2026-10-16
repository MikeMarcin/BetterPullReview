---
schema: "implement-rfc.card.v1"
id: "BPR-1556"
title: "A preference to load images in descriptions and comments, on by default"
status: "In Progress"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"inline-markdown-images-r1","governing_rfc":"RFC-BPR-0084","amendment":"RFC-BPR-0084 owner amendment 1 (2026-10-16)","criteria":["AM1-03"],"origin":"owner accepted Q3 in attended session 8a5e3c244f674294366fd8796da2897e, cursor 17","reservation":"docs/issues/_reservations/BPR-1554--BPR-1556--0108f19c75d6.json"}
---

# BPR-1556: A preference to load images in descriptions and comments, on by default

Under [RFC-BPR-0084 owner amendment 1](../../rfcs/RFC-BPR-0084-inline-markdown-images.md#owner-amendment-1-2026-10-16-answers-to-q1-q3):
Settings > Review behaviour gets "Descriptions and comments: Images — Load / Don't load", Load by default, persisted in the profile and applied live.

## Acceptance

- **AC01 (AM1-03):** Storage tests show the preference persisted with schema version 21, a migrated row loading images, and an invalid value refused.
- **AC02:** Qt and composed tests show the preference applied live in an open window: Don't load turns every image into its placeholder and sends no request; Load shows them again.
- **AC03:** The settings row has an accessible name, a Reset and an information glyph, and follows the other Review behaviour rows.
