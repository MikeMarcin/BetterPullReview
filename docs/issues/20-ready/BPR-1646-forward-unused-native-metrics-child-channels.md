---
schema: "implement-rfc.card.v1"
id: "BPR-1646"
title: "Forward unused native metrics child channels"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/application/insights_native_metrics_tests.cpp"]}
---

# BPR-1646: Forward unused native metrics child channels

application_insights_native_metrics passes directly but its contained canonical self child cannot create QProcess private standard-stream pipes. Neither parent nor synthetic energy child consumes those streams.

## Acceptance

- AC1: Forward only the unused standard input/output/error channels in the native metrics fixture, keeping containment, canonical self child, isolated scratch,80ms CPU work, exact process handle, reap and lifetime assertions.
- AC2: Rebuild and run the actual contained native metrics row. Preserve the original direct pass and contained CreateNamedPipe access denial; do not register an uncontained exception.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.
