---
schema: "implement-rfc.card.v1"
id: "BPR-1644"
title: "Diagnose Ubuntu Insights budget description"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1644: Diagnose Ubuntu Insights budget description

The last actual Ubuntu normal row application_insights_public_path fails the
two-client budget assertion after the persisted ownRequests property reaches412.
The same run observes one real250ms main-thread stall; its later failure is the
accessible description's expected1,240 other-client and3,348 remaining strings.

## Acceptance

- AC1: Retain the original row and observe actual accessible text, locale and persisted own count; establish whether budget semantics or presentation expectation causes the failure without assuming a locale issue.
- AC2: Make only a source-backed bounded repair preserving exact persisted count, lowest remaining response, budget split and accessibility/localization behavior, plus all stall/privacy/storage/render assertions.
- AC3: Verify the complete native public course and meaningful controls, focused format/tidy/documentation, intentional commits and host quiescence; no blind assertion weakening or threshold change.

## Source diagnosis checkpoint

The production budget_bar formats all four numeric fields through QLocale.
The assertion spells comma-grouped literals, and does not currently report the
actual description or locale. A platform-locale mismatch is a hypothesis until
native failure context is observed. Failure-only diagnostics can expose the
synthetic description, ownRequests property and locale with the original
predicate unchanged.

Evidence: ignored ubuntu-gate-r5 raw/normalized logs, lines5404–5411.
The coordinator explicitly released the shared PC for focused repairs; the
Mac807 integration remains parent-owned and protected.
