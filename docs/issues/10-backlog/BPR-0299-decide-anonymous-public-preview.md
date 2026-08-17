---
id: BPR-0299
title: Decide whether to build anonymous public preview
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0298, BPR-0313]
---

# Decide whether to build anonymous public preview

## Required final state

- A digest-bound product decision accepts or rejects the measured request cost,
  shared-address quota behavior, refresh behavior, triage/viewer/action loss,
  private-repository ambiguity, and GitHub.com-only scope.
- Approval defines exact public-preview naming, limit presentation, sign-in
  handoff, and supported-platform policy and grants no mutation or private read.
- A `do not build` result leaves RFC-BPR-0017 Deferred and BPR-0312 unresolved,
  so every implementation card remains dependency-blocked. It is recorded as
  evidence, not converted into Auto-Verified or a completed positive gate.
- Any available-target implementation exception is independent, cannot waive
  RFC-BPR-0002 final closeout, and updates RFC/card dependencies atomically.
