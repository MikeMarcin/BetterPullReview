---
id: BPR-0441
title: The real epoch composition does not reach ready on Windows
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# The real epoch composition does not reach ready on Windows

`epoch_maintenance_integration` composes a real installed-Git cache and a
real SQLite catalog, accepts a handoff, drains the effect and decision
lanes to quiescence, and then requires the host to hold a catalog, an
accepted comparison, and `epoch_status::ready`
(`tests/git_contract/epoch_maintenance_integration_tests.cpp:660-669`). On
the Windows host, at `368ab58`, uncontained and as registered:

```
FAIL: real composition retains and resolves its initial epoch
```

Everything before that line now works here: [[BPR-0436]] narrowed the
catalog root the test hands over, so "construct real SQLite catalog" — the
line this test stopped on in the first Qt-graph run — is behind it. The
host drains to quiescence without tripping the 100-turn bound and then is
not ready, which means some effect in the pipeline between the handoff and
the first resolved epoch returned a failure the test does not print. The
candidates are the ones this host has already exposed elsewhere: the
installed-Git adapter under `Q_OS_WIN` ([[BPR-0437]]'s family) and the
fixture packs it is asked to read.

The shipped public path shows the same shape once its roots are narrowed
([[BPR-0436]]): `application_github_public_path_github_com` composes both
stores, renders, and then waits out its whole budget in stage 2 with
`PUBLIC_PATH_EPOCH_CORE_STATUS:8` (`epoch_status::failed`),
`PUBLIC_PATH_EPOCH_STATUS:Review history could not be updated.`, and the
last recorded interaction never consumed (`GITHUB_FIXTURE_EVIDENCE:11111110`).
The epoch pipeline fails before the presentation completes, so that test is
this card's second witness, not a separate defect.
`application_workbench_composition_contract` is the third: once its script
narrows the `state` tree it pre-creates, the application starts and its
first phase ends on the same `PUBLIC_PATH_EPOCH_CORE_STATUS:8` with the
budget expired.

## Required final state

- The failing effect is named from its own diagnostic, not inferred: the
  test or a probe prints the host's last failure before asserting ready.
- The cause is fixed where it lies — adapter, fixture, or test — and the
  composition reaches `ready` on Windows with the same packs it reads on
  macOS and Linux.
- `epoch_maintenance_integration` passes on Windows as registered and is
  unchanged elsewhere.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^epoch_maintenance_integration$'
```

## Named from its own diagnostic, then repaired — 2026-08-23

Two developer-build diagnostics now exist in the application library,
under `BPR_APPLICATION_TESTING`: `dispatch_core` prints
`EPOCH_EFFECT_FAILED:<effect>:<epoch_failure>` for every failed event it
dispatches, and the cache-failure mapping prints
`EPOCH_CACHE_FAILURE:<git_cache_failure>:<redacted detail>` before the
adapter's code is folded into the core's. The first run with them in place
said, for the integration test and for every application test that ended
on `PUBLIC_PATH_EPOCH_CORE_STATUS:8`:

```
EPOCH_CACHE_FAILURE:2:
EPOCH_EFFECT_FAILED:revision-prepare:0
```

`git_cache_failure::invalid_path` from the cache's `prepare_revision`, with
no detail, became `epoch_failure::unavailable` and `epoch_status::failed`.
A probe on the cache's directory helper then measured the path: `locks`,
absent, reported "present" by `missing()` because MSVC's
`symlink_status` sets a `system_category()` not-found code that is never
equal to the `generic_category()` code the adapter compared it with. The
installed-Git adapter's `prepare_isolation` fails on `home` the same way,
and the application's `windows_system_root` hands the adapter
`C:\WINDOWS` where only `C:\Windows` is accepted. All three are
[[BPR-0437]]'s chain and are fixed there and in `github_application.cpp`;
nothing in the fixture packs was wrong.

### Evidence

| claim | command | result |
|---|---|---|
| the composition reaches `ready` on Windows | `ctest --preset windows-dev -R '^epoch_maintenance_integration$'` | Passed, 7.7 s (`cards-run-29.log`, 2026-08-23) |
| the shipped public path completes with the same packs | `ctest --preset windows-dev -R '^application_github_public_path_github_com$'` | Passed, 5.1 s: `PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, `RENDER_OK` (`cards-run-29.log`) |
| the other public-path witnesses | `ctest --preset windows-dev -R '^application_(workbench_composition\|review_ledger_composition\|appearance_store_fallback)_contract$'` | Passed: workbench 9.4 s, review ledger 14.9 s, appearance fallback 12.9 s; `application_epoch_persistence_github-com` and the fixture-verdict contract with them (`cards-run-29.log`) |
| the diagnostics are developer-only | `BPR_APPLICATION_TESTING` is defined only under `BUILD_TESTING`; the tests-off Release graph the composition contract builds carries neither string | by construction |

Implemented by the integrator.
