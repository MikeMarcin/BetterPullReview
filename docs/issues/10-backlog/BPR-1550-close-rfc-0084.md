---
schema: "implement-rfc.card.v1"
id: "BPR-1550"
title: "Verify and close RFC-BPR-0084"
status: "Backlog"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac"]
closeout_for: ["RFC-BPR-0084"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-acceptance"}
---

# BPR-1550: Verify and close RFC-BPR-0084

Live course on `MikeMarcin/BPR-Test`, frozen attended build, the owner's
verdict and closeout ([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** The live course renders a PR whose description holds a
  `user-attachments` image and an `<img width>` tag, within the observed
  rate-limit margin.
- **AC02:** The owner answers, in a fresh short attended session with a frozen
  build and GitHub comparison renders: "Images in descriptions and comments
  show the way GitHub shows them." (Accept/Reject).
- **AC03:** Every RFC criterion has recorded evidence; documentation gate
  passes.

RFC base: `94699fd51c2644c3ebb38743f623927d179859a1`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Through the shipped composition, the PR description and conversation posts of a fixture show Markdown, HTML, linked, list, table, quote, `width`/`height`/percentage and `<picture>` images inline as defined in "What renders"; comparison renders against GitHub's page for the same body show matching placement and size. | Pending |
| **AC02:** A loading image shows its reserved box, then the body reflows once keeping scroll position and selection; an animated GIF animates; images stay within the reader's width and crisp at 1.0x, 1.25x and 2.0x; resizing, re-rendering and an unchanged refresh make no request once cached; a late result for another body, revision or window is never drawn. | Pending |
| **AC03:** Blocked schemes, `data:`, local and relative-escaping references, the per-body count, byte, pixel, frame, deadline and redirect bounds, a mislabeled or non-PNG/JPEG/GIF payload, SVG and WebP each end in the defined state with the placeholder, and no request is made for a blocked reference; core and host tests show each with a negative control. | Pending |
| **AC04:** Loopback contract tests show: resolution uses only the profile's API authority; GitHub.com image fetches carry no credential; a GHES credential reaches only its credential origins; a redirect to another host drops it for good; a disallowed hop fails closed; and no URL, signed address or credential reaches Qt or the diagnostic log. | Pending |
| **AC05:** Alt text is the accessible text, tooltip and fallback; Tab reaches images that open something; Enter and click open them through link admission; every failure keeps the placeholder with its reason; copy and source view preserve the original body. | Pending |
| **AC06:** The memory and disk caches respect their bounds and isolation; a restarted application shows a cached image without a request; profile purge removes the disk cache. | Pending |
| **AC07:** A live course on `MikeMarcin/BPR-Test` renders a PR whose description holds a `user-attachments` image and an `<img width>` tag, within the observed rate-limit margin; the owner accepts in an attended session that "images in descriptions and comments show the way GitHub shows them". | Pending |
| **AC08:** Architecture, Markdown, threat-model and fixture documentation describe this policy; format, tidy, architecture-boundary, documentation and resealed composition-contract gates pass. | Pending |
