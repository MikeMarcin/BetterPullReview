---
schema: "implement-rfc.card.v1"
id: "BPR-1771"
title: "Record the permanent Debug timing policy and retained release evidence"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1771--BPR-1771--a20b56b43888.json","found_by":"v0.3.4 release and direct owner instruction, 2026-10-26"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"661d2d0f678e2f18aef1d44c23c7cdee4edd3b3e","evidence":{"kind":"artifact","detail":"Direct owner instruction is recorded without expiry in the decision record, AGENTS.md and the release-test-profiles standing policy. Dedicated Debug timing courses may be omitted and mixed timing assertions need no repeated waiver.","reference":"docs/issues/_checkpoints/RFC-BPR-0038/owner-debug-performance-policy-20261026.json"}},{"criterion":"AC2","outcome":"pass","revision":"661d2d0f678e2f18aef1d44c23c7cdee4edd3b3e","evidence":{"kind":"artifact","detail":"Published v0.3.4 results preserve all original failures, exact source identities, measurements, configured skips and eight named Ubuntu Debug omissions. Release timing, functional/hang/resource/compile/artifact/source/security/trust checks remain separate. No timing repair or historical PASS is claimed.","reference":"docs/verification/release-v034-results.md"}},{"criterion":"AC3","outcome":"pass","revision":"661d2d0f678e2f18aef1d44c23c7cdee4edd3b3e","evidence":{"kind":"command","detail":"Documentation structure and 67 documentation contract tests pass on the final administrative tree; README names exact published installer assets."}}]
---

# BPR-1771: Record the permanent Debug timing policy and retained release evidence

## Acceptance criteria

- AC1: Record the direct owner policy in repository instructions and the release
  guide with no expiry and no repeated approval request for Debug timings.
- AC2: Retain actual failures and omissions with their identities. Preserve
  Release timing, Debug functional checks, bounded hang detection, resource,
  compilation, source/legal, security and native trust boundaries.
- AC3: Pass the documentation gate and its contract tests.

## Owner decision and completed follow-up

During v0.3.4 qualification the owner first waived macOS Debug history timing,
then all Debug timing-budget assertions for that release. They subsequently
made the decision permanent:

> From now and until the end of time I won't care about debug timings, we dont even have to run those in debug.

The [decision record](../_checkpoints/RFC-BPR-0038/owner-debug-performance-policy-20261026.json),
[release guide](../../development/release-test-profiles.md#standing-debug-performance-policy)
and repository `AGENTS.md` now carry that instruction. The owner answered
directly; no further attended session or permission request is required.
Dedicated Debug performance courses may be omitted; exact names and omitted
coverage remain explicit. Mixed-course timing failures remain failed evidence
and require no performance rerun or repeated waiver. Functional failures and
actual hangs remain distinct, and a larger bounded Debug watchdog may complete
a semantic course cut off by a short execution budget.

This completes the policy/documentation follow-up. It requires no Debug timing
repair and establishes no passing historical benchmark or shared failure cause.
RFC-BPR-0038 and its native/security gaps remain open.

## Retained v0.3.4 observations

Original native suites bind
`62bfc09f6210233d31de5edb3379aa3381a7315f`; the
[published results](../../verification/release-v034-results.md) explain the final
four-file licensing/test-only source bridge. Raw reports and SHA-256 bindings
remain in the external release session named there.

| macOS Debug timing predicate | Recorded milliseconds, against 100 ms |
|---|---|
| History worst refresh pulse, both attempts | 119.779542 / 116.872584 |
| Launch fixture initial-load input gap | 144.854750 |
| Responsiveness initial-load input gap, both attempts | 150.550458 / 139.918542 |
| Active-work initial-load input gap, both attempts | 143.147459 / 135.893416 |
| Pathological final load / first unavailable-selection pulse | 136.100500 / 106.202917 |

Launch's parent-observed startup bounds passed. Active-work's 31 delivered/applied
inputs and close/quiescence checks completed. These facts do not supply credit
for any checks left unreached after another assertion. The corresponding
complete Release courses passed.

Windows Debug Insights observed two real turns over 100 ms while its fixture
assumed exactly one; the longest was 261.381 ms around an intentional 250 ms
block. Its raw timing-sensitive failure and later unreached checks remain
recorded. Separate Release Insights and Debug core/host/storage controls passed.
The existing Windows timeouts retain their carried release exception and
[BPR-1765](../10-backlog/BPR-1765-investigate-windows-debug-diff-workbench-200-timeout.md)
and [BPR-1766](../10-backlog/BPR-1766-investigate-windows-debug-pr-find-timeouts.md)
are unchanged. A fresh threaded-reply semantic completion course passed under
its larger bounded watchdog; its original failure remains recorded.

Ubuntu Debug explicitly omitted the eight performance rows listed in the
standing guide and retained the separate history/session race control. Its
selected functional inventory completed 831 PASS plus one configured skip.
Omitted courses receive no Debug pass or functional coverage credit. All three
platforms retain their separate native Release acceptance results.
