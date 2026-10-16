---
schema: "implement-rfc.card.v1"
id: "BPR-1549"
title: "Keep inline images in a bounded on-disk cache in the profile"
status: "Backlog"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1548"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-disk-cache"}
---

# BPR-1549: Keep inline images in a bounded on-disk cache in the profile

A bounded owner-private on-disk image cache in the profile serves images
across restarts and is removed by profile purge
([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md) AC06).

## Acceptance

- **AC01:** Tests prove the 128 MiB bound, host and account isolation,
  seven-day expiry, a restart showing a cached image without a request, and
  purge removal.
