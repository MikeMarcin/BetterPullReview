---
schema: "implement-rfc.card.v1"
id: "BPR-0641"
title: "Enable normal inline publication for the Mac private alpha"
status: "In Progress"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0615"]
priority: "P1"
---

# Enable normal inline publication for the Mac private alpha

Owner-approved scope: [private-alpha route](../../development/private-alpha.md).
RFC_BASE_SHA: `ae4480bc`. Keep full RFC closeout on BPR-0242; live account and
mixed-client acceptance remain on BPR-0143/BPR-0144. This implementation slice
requires local evidence and a current Mac production build, not new remote posts.

## Owned paths

Application mutation authority/host; app composition; their focused host,
composition and GHES contract checks; affected architecture/isolation inventories
and product/operator documentation. Reuse the existing editor/provider/recovery.

## Acceptance

- Ordinary production inline Publish selects the accepted authenticated PR on
  GitHub.com or explicitly configured GHES, without a designated-target launcher.
  Each gesture supplies a short-lived one-use grant bound to session, generation,
  target, actor, proposal/body, selection and exact retained diff publication.
- Existing stale-head, endpoint identity, wrong-target, cancellation and durable
  duplicate-prevention paths remain intact. Automated test builds cannot issue
  normal production authority; the attended designated lane stays separately scoped.
- Focused host tests prove altered request/actor/target and replay refusal plus
  sequential distinct user publications. Real composition exercises the normal
  editor and unchanged source-line/publication flow. Local GHES contracts pass;
  live Enterprise verification is explicitly skipped by owner instruction.
- The tests-disabled Mac production app builds and launches. Record direct
  observations and limitations; private-PR smoke waits for an owner-named target.

## Evidence

Pending implementation and focused verification. Do not infer live publication
acceptance or RFC closure from this card.
