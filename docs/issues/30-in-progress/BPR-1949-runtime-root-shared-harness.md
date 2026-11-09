---
schema: "implement-rfc.card.v1"
id: "BPR-1949"
title: "Runtime-root tests adopt shared containment and explicit skip reporting"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"test-correctness","found_by":"frozen43 full qualification","related_cards":["BPR-1942"],"milestone_plan":{"id":"bpr-1949-1950-release-test-contracts","path":"docs/rfcs/plans/bpr-1949-1950-release-test-contracts/milestone-plan.json"}}
---

# BPR-1949: Runtime-root tests adopt shared containment and explicit skip reporting

## Acceptance

- AC1. Use existing ContainedTemporaryDirectory and SkipReportingTestRunner; preserve all seven runtime-root cases, original refusals/permissions checks and POSIX platform skips.
- AC2. The runtime-root course and focused contained-temporary/skip-report contracts pass in the isolated checkout. Retain original Mac/Windows source-gate failures and name skips accurately; no new native coverage is inferred.
- AC3. Documentation/board and intentional latest-main integration pass. Frozen43producers and live native suites remain untouched; exact evidence and pending full qualification are handed back.

## Diagnosis and mandate

The new BPR-1942 script used tempfile.TemporaryDirectory and default unittest.main.
Frozen43 full Mac and Windows Release suites correctly reject both integrations.
The validator's seven assertions are not failing; the harness must reuse existing
contained temporary support and skip reporting. No scan exclusion or platform skip
removal is permitted. This repairs the implementer's own prior test integration.

Canonical main also contains accepted RFC-BPR-0095/BPR-1947 and a1948reservation.
The coordinator authorizes preserving/publishing those accepted tracked commits
with these fixes after proof. Other writer's untracked native-agent-sessions
assets remain untouched. No production Qt/CLI behavior change is in this slice.
