---
schema: "implement-rfc.card.v1"
id: "BPR-1525"
title: "Restore Windows release gate contracts without weaker assertions"
status: "Ready"
rfcs: ["RFC-BPR-0077", "RFC-BPR-0082"]
depends_on: []
priority: "P1"
metadata: {"source":"0.2.1 native full gate at 8f0d25418cd4278065d5a043dabaa728feb202f1","assigned_implementer":"/root/windows_gate_fixes","milestone_plan":{"id":"bpr-1525-windows-release-gates","path":"docs/rfcs/plans/bpr-1525-windows-release-gates/milestone-plan.json"}}
---

# BPR-1525: Restore Windows release gate contracts

The Windows normal production gate reports deterministic source Find/PR Search fixture-byte mismatches, nine intentionally unmeasured containment registrations, a contained Python grandchild launch denied with WinError 5, and matcher-stop latency 16.2308 ms against its unchanged 10 ms limit. Preserve the active original run, producer source and evidence. No signing, authentication or publication changes belong to this slice.

## Acceptance

- AC1: Identify each failure's demonstrated source or environmental cause, retaining the original failures and exact revision.
- AC2: Repair deterministic fixture bytes and cancellation latency without relaxing any assertion, budget or core architecture boundary.
- AC3: Measure the nine contained Windows registrations before recording each actual refusal; the child probe must start its grandchild and prove that grandchild has an AppContainer token and network denial.
- AC4: Pass focused Windows corrected contracts and cancellation checks, relevant portable checks and documentation; report remaining full-release obligations and exact-source invalidation explicitly.

## Ownership

Own the two source-course fixture generators, Windows isolation manifest and counterexample probes, application matcher cancellation wait and its two application construction sites, focused evidence and this card. The application callback repair is authorized by the release coordinator. Do not edit active producers or launch competing Windows/WSL work while their full gate runs.
