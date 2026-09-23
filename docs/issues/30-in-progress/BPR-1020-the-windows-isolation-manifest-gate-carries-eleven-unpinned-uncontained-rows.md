---
schema: "implement-rfc.card.v1"
id: "BPR-1020"
title: "The Windows isolation manifest gate carries eleven unpinned uncontained rows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win under BPR-0653","found_at":"53d229d7","run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json","related_cards":["BPR-0812","BPR-0833","BPR-0872","BPR-0873"]}
---

# BPR-1020: The Windows isolation manifest gate carries eleven unpinned uncontained rows

## Defect

On the Windows station, `test_network_isolation_manifest` fails. Eleven rows are
registered `WINDOWS_UNCONTAINED` but are missing from the pinned Windows set in
`scripts/verify_test_network_isolation.py` (`WINDOWS_UNCONTAINED_TESTS`). Each
row produces three findings: it "does not start with
[...better_pull_review_network_sandbox.exe]", it "lacks the
bpr_network_isolated label", and it "uses bpr_windows_uncontained but is not in
the pinned Windows set".

[[BPR-0653]] found them at `53d229d7`. The list at `38cc26d7` is the same, 33
findings in all:

| Row | Registered in | Added by |
|---|---|---|
| `package_runtime_contract` | `tests/CMakeLists.txt` | `65dd7dd4` (RFC-BPR-0014 release package paths) |
| `release_source_assets_contract` | `tests/CMakeLists.txt` | `1c429749` (separate corresponding-source asset) |
| `release_package_audit_contract` | `tests/CMakeLists.txt` | `3da341d1` ([[BPR-0957]]) |
| `release_security_audit_contract` | `tests/CMakeLists.txt` | `9409ff61` (release hardening) |
| `sqlite_release_maintenance_files` | `tests/sqlite_contract/CMakeLists.txt` | `19a3dfd7` (release maintenance host) |
| `modified_endpoint_fixture_contract` | `tests/git_contract/CMakeLists.txt` | `a9492592` |
| `application_logging` | `tests/end_to_end/CMakeLists.txt` | `19a3dfd7` (Windows registration, was UNIX only) |
| `application_selected_file_safety_public_path` | `tests/end_to_end/CMakeLists.txt` | `a9492592` |
| `application_attended_recent_replay` | `tests/end_to_end/CMakeLists.txt` | `e73831f5` ([[BPR-0932]]) |
| `application_release_maintenance_public_path` | `tests/end_to_end/CMakeLists.txt` | `a34fc07f` |
| `application_release_update_public_path` | `tests/end_to_end/CMakeLists.txt` | `5ed24a8c` ([[BPR-0947]]) |

All eleven were added off Windows. The pinned set is checked only on the
`windows-appcontainer` backend, so the gate cannot see them from macOS or
Linux. This is the same drift [[BPR-0812]] cleared.

## Scope

- For each row, either show that it runs contained and register it contained,
  or measure its first contained failure on the Windows station and pin it
  with that cause. Do not pin a row without a measured cause.
- A row that also fails uncontained is a separate defect. It is recorded here
  and carded, not fixed here.

## Acceptance

- AC1 `test_network_isolation_manifest` on the Windows station reports no
  finding.
- AC2 Each of the eleven rows is registered contained where it passes
  contained, or is pinned with its measured first contained failure; no row is
  pinned without a measured cause.
- AC3 `verify_test_network_isolation_tests.py`
  (`test_network_isolation_structure_contract`) still passes.
