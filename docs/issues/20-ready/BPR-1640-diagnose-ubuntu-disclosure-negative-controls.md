---
schema: "implement-rfc.card.v1"
id: "BPR-1640"
title: "Diagnose Ubuntu disclosure negative controls"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1640: Diagnose Ubuntu disclosure negative controls

The actual Ubuntu normal suite at source8eef fails the two disclosure scale
rows100 and200. Current disclosure readability assertions must remain intact;
the failure is in the old-bar negative controls, with platform-style rendering
still requiring a source-backed bounded repair and native verification.

The coordinator retains the native PC through its complete789-row baseline.
No native reproduction or source repair is claimed by this diagnosis card.

## Acceptance

- AC1: Bind the actual rendered old-bar fill/text and selected style at both failing scales; preserve the original raw output and establish why the supposed native-bezel control does not represent its stated input.
- AC2: Correct only the demonstrated fixture or product cause with a meaningful failing old-bar control, preserving every current disclosure fill/readability assertion and scale. Product visual changes require the already loaded frontend-design skill and agreed bounded scope.
- AC3: Verify native focused scale evidence and appropriate format/tidy/documentation checks, retain negative controls, commit explicit paths and reconcile command ownership.

## Source diagnosis checkpoint

The retained output reports style `windows`. Its supposed native-bezel control
renders theme fills #F6F7F9, #1C2026, #FFFFFF and #000000, rather than the
#E3E3E3 button-family palette input. At100%, Dark's title contrast is12.91:1;
at200%, it is13.84:1, so the expected less-than4.5:1 old-owner-report assertion
also fails. Both high-contrast variants pass the current reading, causing the
two further negative-control assertions to fail at each scale.

The helper changes Button/Light/Midlight/Mid/Dark roles before constructing a
checked plain QToolButton. The style's actual checked-button painting and any
host stylesheet interaction must be established before choosing a repair; no
blanket style switch, assertion removal or readability threshold change is made.
Evidence: ignored ubuntu-gate-r5.partial4 raw/normalized logs, rows564–565.
