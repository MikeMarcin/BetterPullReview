---
schema: "implement-rfc.card.v1"
id: "BPR-1545"
title: "Core policy for inline Markdown images"
status: "Ready"
rfcs: ["RFC-BPR-0084"]
depends_on: []
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-policy-core"}
---

# BPR-1545: Core policy for inline Markdown images

The core decides which image references of a rendered body are admitted, how
each matches GitHub's served address, whether a fetch and each redirect hop is
admitted and may carry a credential, the displayed size, and each image's
state, rejecting stale completions by typed identity
([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** Core tests, without `QApplication`, cover source admission
  (schemes, `data:`, local, network-path, relative), the per-body count, the
  bounded `<img>`/`<source>` tag recognizer with entity decoding, served-address
  matching (camo canonical source, attachment UUID, exact), the GitHub.com and
  GHES host and credential-origin rules, redirect re-admission with permanent
  credential drop, display sizing (natural, pixel and percentage hints, aspect
  ratio, width clamp), state transitions and stale rejection, each with a
  negative control.
- **AC02:** `core-only` and both architecture-boundary tests pass; no new core
  library.
