# BPR-1973 Debug functional watchdog checkpoint

Mandate revision: `coordinator-approved-1973-2026-11-09`. Implementer
`/root/release_fixes_main`, own branch `codex/bpr1973-debug-watchdogs`, based
on exact reviewed BPR-1971 candidate
`970812bfe2a1107625c788a2e87d2a220e4ba93d` including accepted BPR-1972.
Plan/claim commits are `fadb04987` and `0b6840b1b`.

## Source and rationale

Only `tests/end_to_end/CMakeLists.txt` and the run-budget documentation change.
The helper accepts optional positive integer `DEBUG_TIMEOUT`, refuses invalid
or shortening values, and retains the stricter original paired-budget check.
Exactly seventeen existing short functional rows opt into 120-second Debug
outer watchdogs. Optimized outer limits remain 15 seconds, inner budgets
8000 ms, and domain predicates, sandbox, commands and environment stay intact.

The original build43 Windows Debug failures are retained externally: all
seventeen rows were killed by the 15-second CTest watchdog, at 15.07–15.32
seconds. These are lower bounds, not completed timings or semantic passes.
The standing owner policy permits this bounded Debug functional watchdog;
actual hangs and functional failures still require investigation.

## Focused proof

External evidence root:
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/pre-freeze-bpr1973/`.

- `external-preparation.json`: isolated actual CMake single/multi probes select
  Release15/Debug120 and retain argv8000; malformed `abc` and shorter `10`
  refuse configuration. `exact-helper-binding.json` proves the source helper
  exactly matches that probed helper and the reviewed proposal.
- `before-normal-graphs-receipt.json` and `after-normal-graphs-receipt.json`:
  normal source configuration, standard Mac features including CLI, owned
  APFS-cloned dependency cache, three disposable build directories. Required
  KSyntax dependency bootstrap occurs during configure; no application target
  or test is built/run. Neither protected producer nor canonical cache is
  written.
- `normal-graph-comparison.json`: 965 nodes in each single Release/Debug and
  multi Debug/Release/RelWithDebInfo graph. Only the exact seventeen Debug
  `TIMEOUT` values differ (15 to 120). Physical and logical commands, inner
  budgets and every other node/property match. Only CTest backtrace metadata
  is canonicalized. Generated raw CTest files independently match after
  backtrace and approved timeout normalization, covering the one JSON node
  without a resolved command too.
- The initial external raw-file comparator missed multi-config indentation.
  Its actual assertion failure/diff is retained; all five JSON comparisons
  already passed. Correcting that operator regex and reusing the exact saved
  graphs proves raw equality without another configure or test attempt.
- Documentation gate, scoped shared board validation and `git diff --check`
  pass. No C++ changes or broad native matrix is required for this scope.

## Integration and remaining obligation

BPR-1971's final main handback is
`63711bb35cdc99eacea89bddbd9c645120e74d3d`. It is merged normally into the
private BPR-1973 branch, preserving all seven tested readiness-source hashes,
its Done/proof metadata and NEXT1974. Only the five authorized BPR-1973 paths
differ against that authoritative main. Composed documentation, scoped board
validation and `git diff --check` pass. Canonical main fast-forwards and
normally pushes source revision
`7312e89352027f86237beee572ca14833dae1869`; `ls-remote` verifies that exact
remote main. Private implementation commit is
`a8804fa5d392cc4555eeb1db31ab074ba2111490`. The subsequent shared Done/docs
closeout changes no proven source; its exact final main is in the external
final handoff receipt. No source from the other implementer is published early.

The exact actual helper also rejects an omitted `DEBUG_TIMEOUT` value under
the consumer's CMake4.4.2 policy. Original source hashes and all generated
cache/command metadata are sealed; the four task-owned disposable graph/cache
directories have been removed. Full JSON/raw graph/log evidence remains.
The preserved BPR-1971 Mac proof branch is unchanged. All owned commands are
terminal; no review server or native job remains.

Windows semantic execution with the supported 120-second bound remains the
final common-source preflight obligation. Mac configured graphs and isolated
helper probes supply no Windows execution or full release qualification
credit. Root owns protected R, frozen previews and all release operations;
the Windows implementer owns the shared PC. No producer source or PC command
is touched by this slice.

## Exact Debug-only opt-ins

- `application_device_authorization_panel_course`
- `application_device_authorization_browser_handoff`
- `application_device_authorization_registered_course`
- `application_device_authorization_panel_absent`
- `application_device_authorization_browser_absent`
- `application_device_authorization_release_contract`
- `application_device_authorization_unavailable`
- `application_device_authorization_public_path`
- `application_render`
- `application_anonymous_public_preview_enabled_probe`
- `application_anonymous_public_preview_live_candidate_wiring`
- `application_anonymous_public_preview_unavailable`
- `application_anonymous_public_preview_release_contract`
- `application_public_path`
- `application_device_authorization_ghes_refusal`
- `application_composition_capabilities_product_run`
- `application_composition_capabilities_inside_boundary`
