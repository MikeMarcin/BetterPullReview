---
schema: "implement-rfc.card.v1"
id: "BPR-1436"
title: "Share one QProcess supervision between the gh and Git runners, and bound gh's background reap"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
---

# Share one QProcess supervision between the gh and Git runners, and bound gh's background reap

The 2026-10-13 audit found that the gh runner
([github_cli_process_runner.cpp](../../../src/adapters/github/src/github_cli_process_runner.cpp))
and the installed-Git runner
([installed_git_process.cpp](../../../src/adapters/git/src/installed_git_process.cpp))
each carried their own serialized process worker, process-group kill,
process-group resolution, bounded wait slice, kill-and-reap and background
reap. The two copies had drifted. Git's background reap stops at a deadline,
abandons the child and leaves its worker quarantined with `reap_failure`.
gh's looped with no deadline, so one child that never exits held its worker
thread for good, and destroying the runner joined that thread and never
returned. The owner approved consolidating them on 2026-10-13.

Adapters may not depend on each other, and the platform target is
header-only without Qt, so the shared code needs a target of its own.

## Acceptance

- AC1: One first-party Qt Core static target,
  `better_pull_review_process_support` (`BetterPullReview::ProcessSupport`),
  holds the serialized worker lane with its quarantine and the process-group
  wait, kill, resolution, kill-and-reap, background reap and abandonment.
  The gh and Git runners both use it and keep no copy of their own. The
  target depends on no first-party layer, and the architecture document and
  root `AGENTS.md` record it as shared adapter support.
- AC2: gh's background reap has a deadline. A child that outlives it is
  abandoned with a typed `reap_failure`, the lane stays quarantined so later
  requests fail with `reap_failure` without launching, and destroying the
  runner returns. A focused test fails on the old unbounded reap and passes
  on the new one.
- AC3: The architecture boundary gate owns the new target. It refuses a
  QProcess or kill surface outside the designated owners, a support target
  that links another layer, and a lane that loses its quarantine, watchdog or
  bounded reap. The platform header stays the only Windows descriptor and
  handle-list builder.
- AC4: Windows-only code paths are unchanged in behaviour and as close as
  possible in text. The Windows follow-ups below need the Windows lane.

## Windows follow-ups (need the Windows lane, not done here)

- gh on Windows gets the standard-handle list but no Job Object, so a gh
  descendant survives cancel and timeout. Git puts its child in a
  kill-on-close job; gh should take the same containment.
- `src/adapters/agent/src/windows_agent_process.cpp` keeps its own native
  Windows supervision. Whether it can share any of this lane needs a build
  and a test on Windows.

## Evidence

Recorded at finish.
