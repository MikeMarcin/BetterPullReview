---
schema: "implement-rfc.card.v1"
id: "BPR-1547"
title: "GitHub image resolution and fetch adapters and the image decoder"
status: "Backlog"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1546"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"github-image-adapters"}
---

# BPR-1547: GitHub image resolution and fetch adapters and the image decoder

GitHub `bodyHTML` resolution reads, native HTTPS image fetches under the
credential and redirect policy, and the restricted Qt Gui decoder implement
the host ports ([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** Loopback contract tests: resolution uses only the profile's API
  authority; GitHub.com fetches carry no credential; a GHES credential reaches
  only its credential origins; a foreign redirect drops it for good; a
  disallowed hop, a fourth hop, a non-200 status, a non-image type, an
  over-size body and the deadline fail closed.
- **AC02:** Decoder tests: only PNG, JPEG and GIF by signature; SVG, WebP and
  mislabeled payloads are unsupported; pixel, side, frame and decoded-byte
  bounds hold.
- **AC03:** The configured-CLI resolution shape and fixture replay are
  covered; nothing secret is logged.
