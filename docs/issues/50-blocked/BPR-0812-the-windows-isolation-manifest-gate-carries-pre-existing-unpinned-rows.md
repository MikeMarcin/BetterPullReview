---
schema: "implement-rfc.card.v1"
id: "BPR-0812"
title: "The Windows isolation manifest gate carries pre-existing unpinned uncontained rows"
status: "Blocked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win under BPR-0806","found_at":"1f29eaa4","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
blocked_by: "bpr-0834-epoch-rows-fail-at-windows-startup"
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

## Windows station, 2026-09-17 (task-20260917-win-hardening)

The base is `de58ebc6`, after [[BPR-0802]]'s macOS half.

**The complete Windows finding list before this work** had 24 findings: eight
rows with three findings each. Each row "does not start with
[...network_sandbox.exe]", "lacks the bpr_network_isolated label", and "uses
bpr_windows_uncontained but is not in the pinned Windows set". The rows:

- `application_agent_chat_anchored_continuation_contract`
- `application_agent_finding_dif_context_contract`
- `application_epoch_ordinary_update_recreated`
- `application_epoch_portable_occurrence_two_catalogues`
- `git_sparse_cache_contract`
- `profile_window_restoration_fixture_contract`
- `sqlite_profile_activation`
- `sqlite_profile_runtime_lease`

The raw `add_test` finding had already gone with [[BPR-0802]].

**Method.** A temporary, reverted hook in `bpr_add_network_isolated_test`
registered the named rows contained. Each row was run inside the AppContainer,
its first failure was read, and the row was then run uncontained as
registered. Logs are under
`build/logs/task-20260917-win-hardening/ctest-trial-contain*.log` and
`ctest-bpr0812b.log`.

| Row | Contained, measured | Resolution |
|---|---|---|
| `sqlite_profile_activation` | Passes (`profile-activation-tests:pass`) | Registered contained |
| `sqlite_profile_runtime_lease` | Exit 3. A reverted terminate hook printed `canonical: Access is denied.` for the test's temporary root. | Pinned `dos-volume-name`; passes uncontained |
| `git_sparse_cache_contract` | Exit 3. The same hook printed `canonical: Access is denied.` for the Git executable. | Pinned `dos-volume-name`; passes uncontained (33.97 s) |
| `profile_window_restoration_fixture_contract` | First failure: `tempfile.TemporaryDirectory`'s descriptor is unusable in a container ([[BPR-0350]]). After moving the script to `contained_temporary_directory`, `git init` fails with `unable to get current working directory: Permission denied` (probed directly). | Script now uses the contained helper; pinned `dos-volume-name`; passes uncontained |
| `application_agent_chat_anchored_continuation_contract` | Uncontained it also failed at startup (registry refusal), because its `QTemporaryDir` data root was never owner-private on Windows. With the root narrowed, contained it stops at `agent-chat-composition-failure:private-test-root`: narrowing inside the container fails. | Fixture root narrowed in a narrowed parent; pinned `dos-volume-name`; passes uncontained (11.11 s) |
| `application_agent_finding_dif_context_contract` | The same, with `agent-finding-composition-failure:private-test-root` | The same; passes uncontained (14.57 s) |
| `application_epoch_ordinary_update_recreated` | **Parked.** It fails **uncontained** with `STARTUP_FAILED` before any course step, identically with the unmodified HEAD script. Narrowing its `--epoch-fixture-root` with `icacls` did not change that (tried, reverted). With no passing uncontained run, the contained cause cannot be measured. | Not pinned |
| `application_epoch_portable_occurrence_two_catalogues` | **Parked**, the same (`composed public read failed (1)`, stdout `STARTUP_FAILED`) | Not pinned |

**Side effect.** Narrowing the chat course's data root also makes the
already-pinned siblings pass uncontained on Windows:
`application_agent_chat_composition_contract`, `_stale_`, `_continued_` and
`_construction_cut_`.

`application_agent_finding_composition_contract` still fails uncontained, with
the panel `Finished: success` and no named failure term. It was already pinned
and is outside this card; it is reported, not fixed.

**Gate after the change.** `test_network_isolation_manifest` reports exactly
the six findings of the two parked epoch rows. With those two rows
temporarily pinned, the gate printed
`TEST_NETWORK_ISOLATION_OK:configured=354:registrations=341:backend=windows-appcontainer`.
That run is recorded to show that nothing else remains; the rows are not
pinned without a measured cause. `test_network_isolation_structure_contract`
passes.

**Still open.** The two epoch rows need their uncontained `STARTUP_FAILED`
diagnosed first. `main.cpp` prints `STARTUP_FAILED` from about ten branches
without a cause, so that is the next step.

**Portable sources touched:**
- `tests/end_to_end/agent_chat_composition_public_path.cpp` and
  `tests/end_to_end/agent_finding_composition_public_path.cpp`: the data root
  is `<QTemporaryDir>/data`, and both are narrowed with
  `make_storage_root_owner_private` (mode 0700 on POSIX).
- `tests/scripts/verify_profile_window_restoration_fixture.py`:
  `contained_temporary_directory`, which is `tempfile` on POSIX.
- `tests/sqlite_contract/CMakeLists.txt`: one `WINDOWS_UNCONTAINED` marker
  removed, a no-op elsewhere.
- `scripts/verify_test_network_isolation.py`: pins only.

## Integrated; blocked on BPR-0834 (dsp-003)

Main merged `windows-lane` at `711d3ddc` as `902cda35`. On macOS at
`8a03a5be`, `test_network_isolation_manifest`, `architecture_boundaries`,
`profile_window_restoration_fixture_contract`, `sqlite_profile_runtime_lease`,
`git_sparse_cache_contract` and the agent chat and finding courses whose
roots were narrowed pass, except the two finding contracts that also fail at
`2ee49a16` before this merge ([[BPR-0835]]) (`build/r2-logs/dsp3-int1-ctest.log`,
`dsp3-agent-ab-*.log`, `dsp3-base-finding-ctest.log`).

Six of the eight rows are resolved. The remaining six findings belong to
`application_epoch_ordinary_update_recreated` and
`application_epoch_portable_occurrence_two_catalogues`, which fail at
startup on Windows even uncontained, so they cannot be measured or pinned
until [[BPR-0834]] fixes that. This card is blocked on BPR-0834 and returns
to the Windows station with it.

## Windows station, 2026-09-17: the last six findings (task-20260917-win-startup-roots)

[[BPR-0834]] fixed both epoch courses. They pass uncontained, their contained
failure was measured as `startup_failure::registry`, and they are pinned
`dos-volume-name`. `test_network_isolation_manifest` on the Windows station now
passes with **zero findings**:
`TEST_NETWORK_ISOLATION_OK:configured=357:registrations=344:backend=windows-appcontainer`.
The card is left in place for the integrator.
