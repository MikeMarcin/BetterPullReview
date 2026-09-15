---
schema: "implement-rfc.card.v1"
id: "BPR-0770"
title: "Import the authored plan and reconcile the live source"
status: "Ready"
rfcs: ["RFC-BPR-0032", "RFC-BPR-0033", "RFC-BPR-0034"]
depends_on: []
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"intake-baseline","acceptance":["A1","B1","C1"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0770: Import the authored plan and reconcile the live source

## Outcome

Verify the pack; record the live HEAD, the RFC and issue allocations and the exact composition seams the design names (the Accounts page adoption in `workbench_window.cpp`, the last-runtime branch and failure presentation in `profile_desktop.cpp`, `main.cpp`'s quit policy). Import the authored five milestones rather than repartitioning the work.

## Acceptance

- Live HEAD, RFC ID mapping, allocation receipts and any drift from the pack's source audit are recorded.
- Every card belongs to the remapped authored plan; nothing settled in RFC-BPR-0018, 0024 or 0028 is reopened.
- Technical readiness stays separate from formal closeout; no unrelated predecessor work is added.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
