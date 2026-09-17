---
schema: "implement-rfc.card.v1"
id: "BPR-0812"
title: "The Windows isolation manifest gate carries pre-existing unpinned uncontained rows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win under BPR-0806","found_at":"1f29eaa4","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0812: The Windows isolation manifest gate carries pre-existing unpinned uncontained rows

## Defect

On the Windows station, `test_network_isolation_manifest` reports the same 25
findings at `1f29eaa4` and at `d587b5f1`, so [[BPR-0806]]'s pins added no new
finding. Among them are 8 uncontained rows that the gate does not pin,
including `sqlite_profile_runtime_lease`, `sqlite_profile_activation`,
`profile_window_restoration_fixture_contract` and `git_sparse_cache_contract`,
and the raw `add_test` ownership finding for `tests/github_contract/CMakeLists.txt`.
On macOS at `c0f2fa31` the gate reports only the `gh_cli_git_smart_transport`
findings, which [[BPR-0802]] owns. The unpinned rows are therefore invisible
from the Mac, where most of that work was done.

## Scope

- Record the complete Windows finding list on this card.
- For each unpinned uncontained row, either show that it runs contained and
  register it contained, or measure its contained failure on the Windows
  station and pin it with that cause. Do not pin a row without a measured cause.
- Leave `gh_cli_git_smart_transport` and the raw `add_test` finding to
  [[BPR-0802]].

## Acceptance

- `test_network_isolation_manifest` on the Windows station reports no finding
  except those owned by [[BPR-0802]].
- `verify_test_network_isolation_tests.py` still passes.
