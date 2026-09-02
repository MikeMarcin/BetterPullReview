---
id: BPR-0530
title: Pin the uncontained registrations four slices added
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# Pin the uncontained registrations four slices added

## Why

`test_network_isolation_manifest` pins every `WINDOWS_UNCONTAINED`
registration by name in `scripts/verify_test_network_isolation.py`, so the
Windows AppContainer exemption is a measured cause per test rather than a
flag anyone can add. The first full `core-only` matrix run after
[[BPR-0528]] — [[BPR-0300]]'s ledger row, 2026-09-02 — failed 1/112 on that
gate: `WINDOWS_UNCONTAINED appears 65 times in the sources, expected between
one and 61, the size of the pinned set`. Red at HEAD (`cf65ff8f`) in both
the `dev` and `core-only` trees, and unrelated to BPR-0300, which adds no
registration of that kind.

Nine registrations had been marked uncontained without being pinned:
[[BPR-0516]]'s `application_device_authorization_browser_handoff` and
`_browser_absent`; [[BPR-0526]]'s `application_device_authorization_panel_course`
and `_panel_absent`; [[BPR-0522]]'s `application_mcp_context_helper`;
[[BPR-0528]]'s `rfc0016_test_inventory`, `rfc0017_test_inventory`, and
`rfc0017_release_absence_negative_control`; and [[BPR-0525]]'s Windows mirror
of `github_device_authorization_refresh_contract`. Every focused selector on
every station stayed green throughout, because no focused selector runs this
gate, and on macOS and Linux only the count bound fires — the per-name label
check exists only where the `bpr_windows_uncontained` label does, on Windows,
whose station ran focused selectors and never the matrix that would have
named them.

## What changed

The nine names are pinned with the cause their siblings record: the
application and inventory tests under `dos-volume-name` — the application
canonicalises its data root and the inventory shells `ctest --show-only`,
both needing the drive-letter translation a lowbox token is refused — and
the refresh-contract mirror under the same cause as the device-authorization
contract beside it. Nothing about any registration changed.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Red at HEAD before the pin, in the dev tree | `ctest --preset dev -R '^test_network_isolation_manifest$' --output-on-failure` at `cf65ff8f` | `***Failed`: `WINDOWS_UNCONTAINED appears 65 times in the sources, expected between one and 61` | pre-existing |
| Every uncontained registration is now pinned, in both directions | a scan of every `bpr_add_network_isolated_test` call carrying `WINDOWS_UNCONTAINED` across `tests/**/CMakeLists.txt` (55 literal names, two `foreach` forms) against `WINDOWS_UNCONTAINED_TESTS` | zero unpinned; 70 pinned; source count 65 within the bound | this commit |
| The gate is green | the same `ctest` selector | Passed | this commit |
| The gate's own contract holds, including that every cause is a measured one | `python3 tests/scripts/verify_test_network_isolation_tests.py` | **37 tests, OK** | this commit |

The per-name check on the Windows station is the only place a wrong cause
string would be observed as a wrong label, and that is [[BPR-0285]]'s
final-head replay to confirm.
