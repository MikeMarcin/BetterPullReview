---
schema: "implement-rfc.card.v1"
id: "BPR-0914"
title: "Route pasted PR links through the connected CLI session"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
---

# Connected CLI pull-request links

Owner report: after connecting a GHES CLI account, Home stays empty and a pasted
PR link asks for native credentials. Work directly without sub-agents.

## Bounded milestone

Reuse the accepted exact-host CLI session for pasted PR links, preferring it over
an inactive native profile. Discovery alone must not grant authentication.
Preserve signed-out native setup, anonymous GitHub.com preview, cancellation,
and host isolation. Verify policy and the composed public link path.

## Acceptance

- AC1: An accepted CLI session opens an exact-host pasted PR without native credentials.
- AC2: Inactive profiles and other hosts cannot borrow the accepted session.
- AC3: Focused regression and documentation checks pass.

## Diagnosis and boundary

Composition excludes CLI profiles from setup observations and requires a native
credential authority to open a destination. CLI sessions intentionally have no
native vault authority. Core resolution also chooses the first matching host,
which can be an inactive native profile ahead of an authenticated CLI profile.

Home personal PR discovery is separately unimplemented, tracked by BPR-0598 and
BPR-0599 under RFC-BPR-0026. The composed queue replays opened snapshots; this
repair cannot establish account-wide discovery or close RFC-BPR-0026.

Baseline: `e60ea0e0`. No other source writer or dirty source was present.
