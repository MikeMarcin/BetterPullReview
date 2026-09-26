---
schema: "implement-rfc.card.v1"
id: "BPR-0971"
title: "Audit pathological diagnostic privacy and backpressure"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0971-diagnostics","path":"docs/rfcs/plans/bpr0971-diagnostics/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9d2fe772f506b8aa63356d7cfb992f9516c9dfae","evidence":{"kind":"artifact","detail":"Complete seven-subsystem field/source/bound inventory with classified provider and explicit terminal ownership.","path":"docs/verification/bpr0971-completion.md"}},{"criterion":"AC2","outcome":"pass","revision":"9d2fe772f506b8aa63356d7cfb992f9516c9dfae","evidence":{"kind":"artifact","detail":"Unchanged canaries pass sanitizer/provider/file/export; hostile input and real pathological composition courses pass without diagnostic content retention.","path":"docs/verification/bpr0971-completion.md"}},{"criterion":"AC3","outcome":"pass","revision":"9d2fe772f506b8aa63356d7cfb992f9516c9dfae","evidence":{"kind":"artifact","detail":"Measured exact loss/recovery; full/closed terminal and full/unavailable Quill preserve composed outcomes and epoch durability. Windows/Linux runs owner-deferred; native crash and uninterruptible I/O limits explicit.","path":"docs/verification/bpr0971-completion.md"}}]
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

## Classifier investigation checkpoint — 2026-09-25

Assignment `classifier-20260925` at `daaa2023`; test candidate `161b988b`.
The [classification investigation](../../verification/bpr0971-capability-classifier.md)
extends the canaries to schema-shaped names and uppercase code/type identifiers.
All eight cases survive the sanitizer, provider facade and actual Quill file;
none survives export and the active credential is absent. Exact useful-unknown
regressions remain passing (focused CTest 3/3, 15.61 s).

The adapter has no provenance that distinguishes arbitrary new public error
symbols from content with the same syntax. A shape filter cannot prove AC2.
A closed explanation grammar with request-bound public symbols and explicit
unknown-span omission is feasible, but it changes BPR0602's current exact
unknown-value expectations. That contract interpretation needs resolution
before changing the classifier. The evidence records the precise choice; no
privacy waiver or claim that all engineering is blocked is requested.

Product policy is unchanged and AC2 still fails. The card stays In Progress.
Strict tidy has ten reproduced baseline findings in the capability-test unit/
helpers; provider and logging units pass. No rule suppression was added.
The [new handoff](../../rfcs/plans/bpr0971-diagnostics/CLASSIFIER-HANDOFF-2026-09-25.md)
releases the isolated writer and identifies direct terminal-output bounds and
failure isolation as the next independent engineering slice. Earlier remaining
obligations and owner-deferred Windows/Linux verification are preserved.
## Terminal bounds checkpoint — 2026-09-25

The isolated `r4-terminal-20260925` continuation at baseline `f6de40b6` adds
bounded developer startup exception capture (`e1dbf44b`) and caps each Git
diagnostic replacement append before growth (`2b58ce29`). The
[terminal inventory and evidence](../../verification/bpr0971-terminal-bounds.md)
records exact limits and the real MCP helper's remaining content, line-injection
and blocked-pipe counterexamples. Final focused startup/Git tests pass 3/3;
architecture and the two composition contracts pass 3/3, including an actual
tests-disabled Release build. Documentation and board validation pass. Strict
tidy retains two recorded baseline findings outside the changed lines.
BPR0971 remains In Progress.

App-source pin review for `f6de40b6..e1dbf44b`: the two tests-enabled exception
callers share a private 2 KiB capture helper; ASCII controls become spaces and
truncation is explicit. First-failure ownership, mapped failures, exit codes,
tests-disabled behavior and mutation admission are unchanged. The new private
header joins both inventories, the full TU and manifest are resealed, and the
production mutation-branch digest is unchanged. This is implementer self-review,
not independent acceptance. The [terminal handoff](../../rfcs/plans/bpr0971-diagnostics/HANDOFF-terminal-2026-09-25.md)
retains the final checks, command state and next engineering action.

## MCP terminal continuation — 2026-09-25

Assignment `r4-mcp-stderr-20260925`, baseline `0f0c461e`, product candidate
`c907c173`. The [MCP terminal evidence](../../verification/bpr0971-mcp-terminal.md)
records complete content-free environment counts/presence, a 512-byte serialized
startup record, and one owned POSIX writer canceled and joined after its bounded
ordinary-delivery wait. No inherited descriptor flags change. The Mac full/closed
stderr courses preserve refusal exits and exact JSON-RPC output. The integration
selection passes 5/5; the final audit passes 20 observations. Prior-binary and
environment-removal-bypass negative controls fail as required. Strict tidy,
formatting, documentation and board validation pass.

This addresses the recorded Mac MCP name-content, line-injection and full-pipe
counterexamples. Windows still has synchronous terminal delivery, Linux native
evidence remains deferred, and the classifier/application-terminal/remaining
pathological-source obligations remain open. The card stays In Progress. The
[handoff](../../rfcs/plans/bpr0971-diagnostics/HANDOFF-mcp-2026-09-25.md) records
candidate identities, command ownership and next action; no human acceptance is
claimed or requested by this slice.

## Whole-card completion — 2026-09-25

Assignment `r4-full-card-20260925`, product/test candidate `9d2fe772f506b8aa63356d7cfb992f9516c9dfae`.
The [completed seven-subsystem inventory and evidence](../../verification/bpr0971-completion.md)
resolves the earlier private-content and direct-terminal counterexamples. Unknown
provider codes/symbols are omitted while closed useful explanations and known
public query symbols remain. All eight original canaries pass through sanitizer,
provider, real file and export. Pathological diff/syntax/queue/agent courses and
the full large-review application complete the named inventory.

An explicitly owned terminal queue caps content/storage, drops without blocking
producers, reports loss, and cancels/joins blocked writes. Arbitrary Qt/exception
text and workspace paths are omitted; typed epoch diagnostics replace raw stderr.
The final integration selection passes 10/10, changed-boundary selection 9/9,
startup race regression 20/20, ordinary terminal regression 50/50, and tests-disabled
production builds. Mutation-host authority digest is unchanged. The evidence
records the initially discovered queue-contention bug and its verified repair.

AC1–AC3 pass under the owner's explicit Windows/Linux native-run deferral.
Windows cancellation source is implemented but native compilation/runtime is
unqualified; Linux native evidence is also deferred. No native crash, resident
secret-memory or uninterruptible device/kernel-I/O guarantee is claimed. This is
standalone-card acceptance, not RFC0014 release qualification or completion of
later RFC0027 controls. No owner privacy-policy waiver was needed. Historical
checkpoints above retain their original candidate-specific meaning.
