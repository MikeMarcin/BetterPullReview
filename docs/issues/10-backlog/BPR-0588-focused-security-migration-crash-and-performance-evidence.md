---
schema: "implement-rfc.card.v1"
id: "BPR-0588"
title: "Complete focused correctness, security, migration, crash, quota, and performance evidence"
status: "Backlog"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0580","BPR-0581","BPR-0582","BPR-0583","BPR-0584","BPR-0585","BPR-0586","BPR-0587"]
milestone: "Post-Pack v2"
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

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Resolve and test the live RFC's 1 MiB body versus 512 KiB page contract with
explicit immutable body continuations, progress, and no silent truncation.
Recheck total invocation-input bounds for selected compositions. Ledger schema
5 and recovery schema 2 are separate authorities; reserve no version at import.
