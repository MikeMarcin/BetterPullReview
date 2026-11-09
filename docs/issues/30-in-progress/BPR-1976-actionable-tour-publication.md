---
schema: "implement-rfc.card.v1"
id: "BPR-1976"
title: "Make author tour publication failures actionable"
status: "In Progress"
rfcs: ["RFC-BPR-0041"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"mandate_revision":"tour-publication-diagnostics-r2","reservation":"docs/issues/_reservations/BPR-1976--BPR-1976--4a9591483a5f.json","required_review":"independent-critical-for-material-publication-safety","milestone_plan":{"id":"bpr-1976-tour-publication","path":"docs/rfcs/plans/bpr-1976-tour-publication/milestone-plan.json"}}
---

# Make author tour publication failures actionable

The owner cannot publish a prepared tour as the PR author on a corporate Enterprise machine. After explicitly choosing compatibility comments, Publish ends with “Publication is unavailable. The private draft is retained.” The machine is unavailable to this session. Trace the actual implementation and repair confirmed local defects; do not invent an account, server, transport or qualification cause from that sentence.

## Acceptance

- AC1. Reproduce the generic refusal through the real production adapter/session and the composed public UI, distinguish confirmed repository defects from unknown corporate observations, and retain safe typed reasons for publication preflight failures.
- AC2. Valid author compatibility publication succeeds where the existing supported transport allows it, including exact historical sources and separately named optional description edits. Qualification, tests-on safety, account/head identity, one-use grants, durable witness and unknown-outcome no-blind-retry boundaries remain enforced.
- AC3. The user sees a truthful specific failure and next step without losing the private draft or asserting that an unknown write failed. Focused changed-surface tests, offscreen UI evidence, documentation and commands are recorded against the exact candidate.
- AC4. Supply independent critical review for material publishing safety changes and reconcile every actionable finding before final acceptance. Keep unavailable corporate/native evidence explicit.

## Owner priority amendment, 2026-11-09

Mandate `tour-publication-diagnostics-r2` preserves r1 and prioritizes: “first and foremost, it seems that the tour published path writes nothing to diagnostics logs at all”.

- AC5. The app records a complete sanitized publication lifecycle, including preview, admission, start, per-operation stage, terminal/refusal, provider-factory early failure, status and recovery. Exact attempt correlation, stable stage/failure names and bounded HTTP status survive the product log and diagnostic export; no credentials, prose, source or response bodies enter this trace. Public composition verifies early refusal, failed compatibility, success and recovery.
