---
schema: "implement-rfc.card.v1"
id: "BPR-0588"
title: "Complete focused correctness, security, migration, crash, quota, and performance evidence"
status: "Ready"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0580","BPR-0581","BPR-0582","BPR-0583","BPR-0585","BPR-0586","BPR-0587"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Complete focused correctness, security, migration, crash, quota, and performance evidence

## Objective

Exercise every authority boundary and failure mode with direct, secret-free evidence.

## Required final state

- Every RFC acceptance criterion has passing direct evidence or a precise live blocker naming missing target/person/service.
- Core, SQLite, protocol, host, Qt, and end-to-end suites cover the matrices in the RFC and traceability file.
- Process-kill and remote-success/local-failure tests prove transactional recovery without duplicate publication.
- Credential/environment/private-source canaries prove no unauthorized data crosses process/storage/evidence boundaries.
- Quota/page/oversize/high-water tests prove whole-operation refusal and no silent truncation.
- Benchmarks report database growth, query page bounds, peak memory, active-set/changed-set work, and Qt reset counts.
- Designated mutation evidence or an explicitly parked live-service card satisfies repository safety policy.

## Non-goals

- Using real private PR content in evidence
- Treating fixture metadata as proof of provider-wide behavior
- Full unrelated test matrix unless impact requires it

## Completion evidence

- Claim matrix with commands/artifacts
- Secret scan
- Benchmark report
- Crash logs
- Designated mutation receipt and independent read or blocker card

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Resolve and test the live RFC's 1 MiB body versus 512 KiB page contract with
explicit immutable body continuations, progress, and no silent truncation.
Recheck total invocation-input bounds for selected compositions. Ledger schema
6 and recovery schema 5 are now separate live authorities; recheck each frontier
before allocation. Preserve exact v5 migration cases plus v6 note revision/CAS
and high-water behavior.

## M9 local-evidence prerequisite clarification (mandate r2)

BPR-0584's verified local publication implementation is integrated on main;
its designated or signed recording and independent public body/actor read remain
human-pending. This card may collect and complete local correctness, failure,
security and performance evidence against that integrated implementation. The
required final state permits a precise blocker for unavailable external proof;
it does not count that blocker as RFC AC-12/13/17 or BPR-0584 acceptance.
M9 reports the external exit as pending until a qualifying target and readback
exist, and RFC closeout remains blocked. This changes only the board's terminal
status prerequisite, preserving every evidence and safety requirement.
