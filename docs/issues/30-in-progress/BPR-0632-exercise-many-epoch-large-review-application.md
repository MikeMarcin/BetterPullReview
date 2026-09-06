---
schema: "implement-rfc.card.v1"
id: "BPR-0632"
title: "Exercise many-epoch large-review application replay"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0631"]
platforms: ["mac"]
---

# Exercise many-epoch large-review application replay

The large-review fixture contains 32 distinct Git heads, but the composed course
currently opens only the latest and therefore creates one application epoch.
RFC-BPR-0013 requires many-epoch benchmark and stale-selection evidence.

## Bounded acceptance and ownership

- Drive all 32 stored heads through the ordinary public refresh, provider pager,
  installed Git/cache actor and durable epoch catalog in a disposable local root.
  Extend only the existing test fixture transport with exact bounded head support.
- Exercise public epoch selection/history, current and historical source identity,
  selection supersession and cancellation while jobs are active. Check actual
  source and retained refs/catalog against stored independent head/delta values.
  Render the full application; unit/model counts alone do not establish acceptance.
- Record workload, Mac configuration, per-refresh/selection timing, UI input and
  bounded job/cache counters. Measure before setting justified local thresholds;
  retain native-platform and broader RFC limitations.
- Own existing fixture configuration/transport and narrow fixture evidence under
  src/app/, existing large_review_benchmark.cpp and CMake registration, plus
  focused tests for any contained demonstrated product defect. Split materially
  wider defects before expanding ownership. Keep the ordinary latest-head course.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work is local, network-denied and synthetic. No Actions, workflow dispatch,
push, paid CI, credential access or production app/account operation. Existing
human/live gates and Windows/Linux work remain open.
