---
schema: "implement-rfc.card.v1"
id: "BPR-1635"
title: "Select native nanosecond metadata timestamp"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"4b73e70461c316483ea92d9e4710f1149db13a96","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3ca343cbeb252b476b620a340cd821802ab08539","evidence":{"kind":"observation","detail":"Apple st_mtimespec and Linux st_mtim retain exact seconds*1e9+nanoseconds and snapshot fields; no warnings disabled;format22/diff pass."}},{"criterion":"AC2","outcome":"pass","revision":"3ca343cbeb252b476b620a340cd821802ab08539","evidence":{"kind":"observation","detail":"Coordinator full Ubuntu r5 normal build8eef compiles actual Linux branch under GCC15.2 Werror; runtime snapshot courses report45 unchangedcheckouts,writes0 and canarypass. Original unrelated C5 typed-question failure retained under1637. Actual Mac fullmetadata passes17.44s before and15.16s after separatefixturecorrection;scopedtidy22 Werrorpasses;explicitcommit/selfreview/docs complete;fullgatesparent-owned."}}]
---

# BPR-1635: Select native nanosecond metadata timestamp

Ubuntu normal gate-r3 fails compiling the Git metadata harness: its POSIX branch
uses macOS-only `stat::st_mtimespec`. Linux exposes `st_mtim`. The actual native
failure is retained, and the coordinator delegates this exact platform selection.

## Acceptance

- AC1: Select the platform's native seconds/nanoseconds pair, preserving exact nanosecond snapshot comparison, all file fields and warning flags; format/self-review the bounded change.
- AC2: Verify focused native compilation and affected metadata behavior, retain failed baseline and exact candidates, pass documentation/diff checks, commit explicit paths and hand full gates to the coordinator.

## Verification

Apple st_mtimespec and Linux st_mtim retain exact seconds*1e9+nanoseconds and snapshot fields; no warnings disabled;format22/diff pass.

Coordinator full Ubuntu r5 normal build8eef compiles actual Linux branch under GCC15.2 Werror; runtime snapshot courses report45 unchangedcheckouts,writes0 and canarypass. Original unrelated C5 typed-question failure retained under1637. Actual Mac fullmetadata passes17.44s before and15.16s after separatefixturecorrection;scopedtidy22 Werrorpasses;explicitcommit/selfreview/docs complete;fullgatesparent-owned.
