---
schema: "implement-rfc.card.v1"
id: "BPR-0588"
title: "Complete focused correctness, security, migration, crash, quota, and performance evidence"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0580","BPR-0581","BPR-0582","BPR-0583","BPR-0585","BPR-0586","BPR-0587"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"RFC criterion matrix or precise blocker","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0023.m9-failure-performance-evidence.md maps all 18 criteria to direct local evidence. AC-12/13/17 retain the exact BPR-0584 designated-or-signed mutation and independent server-read blocker; they are not marked externally accepted."}},{"criterion":"Focused core, SQLite, protocol, host, Qt and end-to-end suites","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"command","detail":"Dev focused selection passed 9/9 in 169.08 s: core_private_review, sqlite_private_review_storage, application_private_review_host, qt_agent_launch_canary, qt_private_review_panel, github_review_comment_permalink, and native/file/retry public paths. Rebuilt SQLite and Qt metric changes passed focused 2/2; documentation gate and 64 documentation tests passed."}},{"criterion":"Process-kill and remote-success/local-failure recovery","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"observation","detail":"SQLite course kills actual transaction writer before/after private content, draft and publication commits and during v6 migration, then verifies atomic reopen. GitHub publication fixture uses competing BEGIN IMMEDIATE after remote success, failed refresh, reopen and repeated read-only reconciliation with one POST."}},{"criterion":"Credential, environment and private-source canaries","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"observation","detail":"Real-child canary checks full allowlisted environment/argv/stdin with parent credential markers; host checks selected-only private input; SQLite scans owned database/WAL for envelope markers. Supplementary credential-format scan of 25 tracked M9 changed files reported zero hits."}},{"criterion":"Quota, page, oversize and high-water refusal","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"observation","detail":"Store tests cover 64-row indexed keyset pages, 512 KiB body pages, 1 MiB exact assistant, 64 MiB thread budget plus one-byte whole refusal, all count/byte counters and signed allocator exhaustion. Host checks selected input bounds; no silent truncation."}},{"criterion":"Database, memory and changed-set measurements","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"artifact","detail":"M9 report records 4,096 -> 67,473,408 byte database (growth 67,469,312), storage max RSS 29,147,136, Qt 200% max RSS 61,177,856, 64 active rows/one changed row, 64 inserts/10 data changes/zero resets, retained selection, query-plan and page bounds."}},{"criterion":"Safe designated mutation policy","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"observation","detail":"BPR-0584 remains explicitly parked for human-review; no live target, authenticated reviewer, signed recording or independent public body/actor/unchanged-state read was supplied. This is a precise blocker, not a claim that the M9 external exit or RFC AC-12/13/17 passed."}},{"criterion":"Documentation impact","outcome":"pass","revision":"d6a56d68c757349151f0c1dfa9a7e946e8da6e74","evidence":{"kind":"artifact","detail":"RFC verification ledger and M9 evidence report list changed tests, actual measurements, commands, product no-impact and external limits. Documentation gate passed DOCUMENTATION_OK, documentation tests passed 64/64, scoped board validation valid."}}]
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
