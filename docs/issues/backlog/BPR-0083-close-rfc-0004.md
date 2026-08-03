---
id: BPR-0083
title: Verify and close RFC-BPR-0004
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0054, BPR-0072, BPR-0073, BPR-0074, BPR-0075, BPR-0076, BPR-0077, BPR-0078, BPR-0079, BPR-0080, BPR-0081, BPR-0082]
---

# Verify and close RFC-BPR-0004

Re-read every acceptance criterion and run clean relevant developer,
core-only, sanitizer, tests-disabled Release, target-native available-platform,
architecture/dependency, documentation-impact, original replacement-pack,
tracker, workflow-absence, and diff-hygiene gates. Replay the complete direct
claim matrix after final documentation and simplification.

Rerun both selected public-composition tests on the final post-BPR-0082 source
with `QT_QPA_PLATFORM=offscreen` and `CTEST_PARALLEL_LEVEL=1`; allow only one
application process at a time. The earlier BPR-0079 public raster plus
BPR-0080 component renders cannot substitute for this final C3 replay.

Capture rendered two-direction interaction and machine-readable traces proving
shared authority, left/right/divider input, insertion/deletion pinning,
unequal replacement, reversal, start/end, one scrollbar, link-map attachment,
identity agreement, stale rejection, and visible-set performance through the
real composition or closest deterministic rendered harness.

Capture an owner-activated, target-native System-theme transition on the full
selected composition. Record the native style/plugin, inherited palette and
font before/after the OS appearance change, stable semantic selection, one
post-theme geometry rebuild, and visibly differing full-window pixels. An
offscreen or forced-palette render cannot satisfy this gate. This RFC-4 gate
does not substitute for RFC-BPR-0005's planned integrated presentation-polish
and native visual-acceptance pass.

Mark RFC-BPR-0004 Auto-Verified only when every required live card is done and
RFC-BPR-0003 is genuinely Auto-Verified. Do not begin RFC-BPR-0005 while this
closeout card remains open.
