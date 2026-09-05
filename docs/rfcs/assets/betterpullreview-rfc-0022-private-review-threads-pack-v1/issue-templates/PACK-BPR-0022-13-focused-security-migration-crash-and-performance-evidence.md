---
provisional_id: PACK-BPR-0022-13
title: Complete focused correctness, security, migration, crash, quota, and performance evidence
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-05, PACK-BPR-0022-06, PACK-BPR-0022-07, PACK-BPR-0022-08, PACK-BPR-0022-09, PACK-BPR-0022-10, PACK-BPR-0022-11, PACK-BPR-0022-12]
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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
