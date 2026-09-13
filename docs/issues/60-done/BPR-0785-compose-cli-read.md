---
schema: "implement-rfc.card.v1"
id: "BPR-0785"
title: "Compose bounded configured-CLI reads through the public workbench"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
acceptance: [{"criterion":"Opted-in configured gh on exact host uses bounded Git bridge without BPR token/helper/global-auth mutation","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Source bridge cdf6175c and qualified r2 amendment 211e82f8; macOS sandbox write denial and owned clone temp; official gh internal helper is disclosed. Offline containment and native status-only /user probes retained in build/rfc0015-r2-d3-* evidence."}},{"criterion":"Process and response adapter preserve bounded HTTP provenance, correlation, pagination, cancellation and redaction","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Final five-case focused adapter/provider/Qt CTest pass; 3 fixture harness tests pass. Git pack runs through application v2 parser and strict indexing; typed GraphQL/rate metadata covered."}},{"criterion":"Config, executable and account changes cause typed refusal with disclosed races","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Focused provider/bridge tests cover mismatch, revoke and config drift; public missing-process and malformed-response controls show visible refusal. Native fallback not instrumented; remaining races and real HTTPS helper exchange are disclosed on BPR-0788."}},{"criterion":"Tests-disabled public fixture renders full PR; removing process adapter blocks same public path","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"build/app-release/rfc0015-evidence/public-path.json records six changed review.cpp lines and Diff ready. build/app-release/rfc0015-evidence/compiled-knockout.json records isolated one-line composition removal, same healthy fixture, typed Connect refusal, disabled Open/Refresh and no full-diff success."}},{"criterion":"Focused adapter/provider/composition/docs checks pass; no live mutation","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Final CTest 5/5 including Qt and Git bridge; fixture tests 3/3; DOCUMENTATION_OK; clang-format and diff checks. No live PR write, push or publication."}}]
---

# Compose bounded configured-CLI reads through the public workbench

- [ ] The selected official `gh` and exact hostname execute only for an opted-in CLI profile, without token extraction, a BetterPullReview-launched shell, global auth mutation or native-credential fallback. On macOS every `gh` launch has OS-enforced file-write denial with only a bounded owned clone-store exception. A bounded `gh` Git operation may invoke Git and `gh`'s own credential-helper shell; its child-process and account races are disclosed.
- [ ] The CLI process adapter returns actual bounded HTTP status/headers/body and maps failures into normalized provider results while preserving correlation, pagination, cancellation and redaction.
- [ ] Config, executable and observed account changes cause typed refusal. Tests state which races remain uncontained.
- [ ] A tests-disabled Mac composition renders a representative fixture PR from the real CLI profile; removing the CLI process or response adapter makes the public test fail.
- [ ] Focused adapter/provider/composition checks and documentation gate pass. No live PR mutation occurs.

The `EpicGames/UnrealEngine#1773` 250-commit response limit belongs to
RFC-BPR-0013 and is not a CLI read acceptance target.

Initial macOS Git-object qualification is limited to `gh` 2.100.0 and Apple
Git 2.50.1 (Apple Git-155), with a 32 MiB per-file ceiling and a supervised
96 MiB/128-entry temporary-store abort threshold, not a hard aggregate quota.
Refs synthesized for local protocol v2 represent the accepted API snapshot,
not a fresh remote advertisement. The tests-disabled full-diff render and
isolated process-adapter-removal control are recorded on BPR-0788; neither
claims a native HTTPS credential-helper or Keychain-backend qualification.
