---
schema: "implement-rfc.card.v1"
id: "BPR-1548"
title: "Show images inline in the Markdown reader"
status: "Ready"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1547"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"inline-image-presentation"}
---

# BPR-1548: Show images inline in the Markdown reader

The shipped reader shows images inline with loading, reflow, sizing, HiDPI,
animation, links, accessibility and fallback, composed through the real
application with packaged decoders
([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** Qt tests and a composed fixture course show RFC AC01, AC02 and
  AC05 at 1.0x, 1.25x and 2.0x.
- **AC02:** The package ships `qgif` and `qjpeg`; the release package
  verifier agrees.
- **AC03:** Markdown, architecture, threat-model and fixture documentation
  describe the policy; composition contracts are resealed.
