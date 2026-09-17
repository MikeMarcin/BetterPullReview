---
schema: "implement-rfc.card.v1"
id: "BPR-0872"
title: "Windows: eleven contained rows fail because they cannot start a child process"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs, the run's first full Windows sweep","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0872: Windows: eleven contained rows fail because they cannot start a child process

## Defect

Eleven rows fail on the Windows station, all sharing one measured cause: a row
running inside the AppContainer cannot start the child process it needs.

- `issue_id_allocation_contract`
- `github_recorded_fixture_contract`
- `large_review_fixture_contract`
- `attended_review_verdict_contract`
- `project_source_dump_contract`
- `dif_assets_contract`
- `rfc0017_public_preview_decision_contract`
- `test_network_isolation_child`
- `rfc0016_relocated_operator_contract`
- `rfc0016_production_identity_security`
- `application_agent_finding_protocol`

Row-by-row first failing lines are in
`docs/verification/dsp4-windows-merged-head-sweep.md`.

This is the run's **first full Windows sweep**, so these are newly *observed*
rather than newly broken. Nothing in this run's changes touches them. Their
age is unknown and worth establishing before anyone treats them as a
regression.

Two related observations from the same sweep suggest the container's process
and filesystem boundary is the common theme rather than eleven separate
defects:

- `git rev-parse` run inside the container exits 128 with "fatal: Unable to
  read current working directory: Permission denied" ([[BPR-0873]]).
- Seeding executables exit 3, this station's known contained abort for
  `std::filesystem::canonical` ([[BPR-0833]], [[BPR-0845]], and
  [[BPR-0876]]).

## Scope

- Establish for each row whether it is meant to run contained at all. A row
  that needs a child process, a git invocation or the real filesystem may
  simply want `WINDOWS_UNCONTAINED`, which is an existing, deliberate
  registration and not a weakening — several rows already carry it for exactly
  this reason.
- Where a row genuinely should stay contained, name what it needs from the
  container and whether that is grantable.
- Say how old these failures are: check the same rows at an earlier commit so
  nobody later reads this card as "the September merges broke Windows".
- Group the answer. Eleven rows with one cause should take one decision, not
  eleven.

## Acceptance

- Every row above either passes on Windows or carries a recorded, reasoned
  registration change, with the reason on this card.
- The card states whether the failures predate this run.
- macOS and Linux are unaffected: all eleven pass there today and must
  continue to.
