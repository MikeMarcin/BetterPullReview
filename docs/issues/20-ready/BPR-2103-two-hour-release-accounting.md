---
schema: "implement-rfc.card.v1"
id: "BPR-2103"
title: "Account for the two-hour clean release target"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057", "BPR-2058", "BPR-2100"]
priority: "P1"
---

# Two-hour clean release accounting

## Scope

Owner, 2026-11-11: aim for two hours on future clean releases. Preserve the
instruction to leave the active frozen release alone, improve main best effort,
and avoid a lengthy measurement campaign. Two hours is a target, not a measured
result or permission to narrow required qualification.

## Acceptance

- AC1: Extend the existing phase ledger with overlap-aware machine execution,
  explicitly recorded human/service/capacity waits and unattributed elapsed time.
  Open/failed attempts remain visible; accounting neither infers wait causes nor
  grants acceptance. Report the two-hour budget without cancelling work.
- AC2: The runbook distinguishes bounded eligible qualification from broad work,
  useful work during waits, verified independent native lanes and evidence needed
  before further concurrency. Retain required coverage, all failure receipts,
  physical-host exclusion and source/artifact/trust obligations.
- AC3: Focused accounting, CLI and documentation checks pass. No product build,
  benchmark campaign, active-release write, credentials move or unsupported
  performance claim is made.
