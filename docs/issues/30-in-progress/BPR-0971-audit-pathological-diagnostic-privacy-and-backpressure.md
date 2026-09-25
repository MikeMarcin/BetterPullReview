---
schema: "implement-rfc.card.v1"
id: "BPR-0971"
title: "Audit pathological diagnostic privacy and backpressure"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0971-diagnostics","path":"docs/rfcs/plans/bpr0971-diagnostics/milestone-plan.json"}}
---

# Audit pathological diagnostic privacy and backpressure

## Transferred obligation

Original RFC0013 acceptance row 8's full large/error/pathological workload inventory.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

BPR-0602/0606 and current application_logging, quill_logging and quill_logging_backpressure pass their scoped metadata-only and queue-pressure courses. The complete pathological-source/error privacy inventory remains unperformed. Earlier private-target diagnostics retained no private source; they are candidate-specific observations.

## Acceptance

- AC1: Inventory diagnostics across GitHub, Git, diff, syntax, persistence, queue and agent error/large-input paths, classifying every retained field and bound.
- AC2: Use synthetic secret/content canaries and hostile oversized inputs through the composed public paths to verify no credentials, private source, request bodies or uncontrolled provider text reach diagnostic storage or exports.
- AC3: Measure bounded backpressure/drop reporting and shutdown under stress; prove diagnostic failure cannot change authority, durable outcome or terminal delivery. Keep external native crash-report limits explicit.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## Bounded audit checkpoint — 2026-09-25

Assigned standalone audit baseline `97a7564b`; tested candidate `a0bc7259`.
The [field/source inventory and evidence](../../verification/bpr0971-diagnostics.md)
records a concrete AC2 failure: short synthetic private-content phrases survive
capability-probe sanitization and reach the real Quill file. The provider-facade
probe independently retains the same content. Active credential removal and the
metadata-only export pass; neither makes readable logs content-safe.

Nine focused tests passed, with the final changed provider/logging checks rerun
2/2. Added eight-writer/80,000-submission pressure accounting, malformed/oversized
summary omission, real-app full/unavailable logger courses, and successful epoch
commit/drain checks under all five logger statuses. Source and durable authority
are unchanged. Exact commands, measured bounds, failures, temporary negative
control and native limitations are in the evidence record.

AC1 remains partial for the named terminal-output exceptions and missing
pathological courses. AC2 fails. AC3 is verified only for Quill, file refusal,
provider terminal and epoch durability paths; direct stderr/native crash paths
remain open. Windows/Linux runs are deferred by the owner's 2026-09-25 direction.
The card remains In Progress and is not accepted.

Next engineering round must define a content classifier that satisfies RFC0027's
unknown-error usefulness and omission of unclassifiable private spans, make both
`--audit-content-policy` probes pass, and complete the non-Quill terminal courses.
This is remaining engineering, not a request for the owner to waive privacy or
a ready human-review criterion. See the
[handoff](../../rfcs/plans/bpr0971-diagnostics/HANDOFF-2026-09-25.md).
