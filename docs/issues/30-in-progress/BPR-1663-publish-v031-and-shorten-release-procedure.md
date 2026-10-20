---
schema: "implement-rfc.card.v1"
id: "BPR-1663"
title: "Publish v0.3.1 and shorten the release procedure"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"Owner requested publication of v0.3.1 and a faster procedure; confirmed full macOS, Windows and Ubuntu 26.04 packages","baseline":"5aff89be6","milestone_plan":{"id":"release-v031","path":"docs/rfcs/plans/release-v031/milestone-plan.json"}}
---

# BPR-1663: Publish v0.3.1 and shorten the release procedure

## Acceptance

- AC1: Prepare one clean committed 0.3.1 source with accurate public release notes.
- AC2: Run native normal gates, package and sign the three selected platforms, audit actual runtime/source/SPDX and final native trust, retaining failures and skips.
- AC3: Publish through the existing guarded public publisher only after selected artifacts and required dispositions are ready; verify downloaded bytes, source binding, native trust and release/tag state.
- AC4: Record measured time and safe procedure improvements, reusing unchanged evidence and producer caches without weakening gates.

The owner explicitly extended the existing Ubuntu RSS-growth exception to
v0.3.1 on 2026-10-19, requesting an implementer pursue its repair after publication.
Only the unchanged 216 MiB process RSS predicate is excepted; its result stays failed. RFC-BPR-0038's
remaining native/security/source obligations remain explicit.

## Release integration findings

The Qt6.12 candidate `cecded819` completed its normal macOS gate with 813 passes,
one configured Debug cache-course skip and one failure. The Discuss-to-Review
course exposed a real race: durable read completion replaced an already-open
reader, destroyed its reply menu and disturbed an immediate reply. The bounded
release repair updates read marks within the current card and preserves the
visible post's position when the New divider changes. It keeps the existing
semantic read command and host-owned persistence.

Exact final regressions fail with the four old production presentation files
and pass after byte-checked restoration. Immediate menu navigation, Reply text
and caret, and mixed-divider viewport continuity pass at 100%, 125% and 200%;
the focused composed integration and strict touched-unit clang-tidy 22 pass.
See [release engineering evidence](../../verification/release-v031-engineering.md).
The original Windows gate completed 784 rows: 781 passed, two failed and one
configured Debug cache-byte course was skipped. Its diff-zoom failure matches
the reader/menu race. Key-free controls prove the Home oracle also sampled an
earlier menu command before its tab finished updating. The fixture now waits
for that command's actual displayed/core next-file result before reopening
Compose, retaining the original Home/full-tab assertion and draft round trip.
The exact fixture passes the Windows 4×3×3 course and four macOS public courses,
with strict format/tidy checks. No product navigation change is made.

Final common-source gates, optimized cache courses, artifacts, current
security/source dispositions and publication remain pending. No acceptance
criterion is closed by these bounded repairs alone.
