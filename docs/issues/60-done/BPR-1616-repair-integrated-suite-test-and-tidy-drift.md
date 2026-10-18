---
schema: "implement-rfc.card.v1"
id: "BPR-1616"
title: "Repair integrated suite test and tidy drift"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1594", "BPR-1603"]
metadata: {"criteria": ["AC1", "AC2"], "reservation": "docs/issues/_reservations/BPR-1616--BPR-1616--890f7349c660.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7c88aed2f74b0d94f16d267255af4380d967fea4","evidence":{"kind":"observation","detail":"All eight previously failing rows and the actual profile-recovery course pass serially (9/9). Accepted Agent tab controls and exact finding promotion are asserted; three wording negative controls pass. See BPR-1616 verification."}},{"criterion":"AC2","outcome":"pass","revision":"7c88aed2f74b0d94f16d267255af4380d967fea4","evidence":{"kind":"observation","detail":"All five changed C++ translation units pass clang-tidy 22 with warnings as errors and format 22. Documentation gate and all 67 documentation tests pass. All three reported findings removed; no src/app changes."}}]
---
# BPR-1616: Repair integrated suite test and tidy drift

Owner requested the eight deterministic failures and three tidy findings from
item 9 be fixed. Preserve the accepted Agent design; update stale tests.

## Acceptance

- AC1: All eight previously failing rows pass, with Agent controls and promotion
  checked through the accepted tab, review-thread wording still guarded, and
  profile recovery using the contained temporary helper.
- AC2: The three reported tidy findings are fixed; focused tidy, format 22 and
  the documentation gate pass. No full suite is required for this repair.

## Evidence

Baseline and causes: `docs/issues/_checkpoints/codex-handoff-2026-10-17.progress.md`.
Execution: direct, preserved from that handoff. Origin main merged before edits.

Verified product `7c88aed2f74b0d94f16d267255af4380d967fea4`: all eight original failures and the real profile-recovery
course pass (9/9), all five touched C++ units pass tidy and format 22, and the docs
gate plus 67 regression tests pass. Exact commands, causes and scope are recorded
in `docs/issues/_checkpoints/BPR-1616/verification.md`.
