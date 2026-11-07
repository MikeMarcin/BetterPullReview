---
schema: "implement-rfc.card.v1"
id: "BPR-1906"
title: "Split github_application.cpp's composition adapters into responsibility files"
status: "Done"
rfcs: []
depends_on: ["BPR-1905"]
priority: "P2"
metadata: {"baseline":"2fc1cb2a395abcdcdf9a679833e6652d76d6d3dd","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing. Amendment the same day: aim for at most about 2,000 physical lines per file for this initial refactoring; subfolders are welcome for organization."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"667134d4b","evidence":{"kind":"observation","detail":"Ten responsibility header/source pairs under src/app/src/github_application/, headers declaring and sources defining; largest 412 lines."}},{"criterion":"AC2","outcome":"pass","revision":"667134d4b","evidence":{"kind":"observation","detail":"AST-generated moves keep #if conditions, comments and platform branches; test-only includes guarded; contract-read items stay in the root."}},{"criterion":"AC3","outcome":"pass","revision":"667134d4b","evidence":{"kind":"command","detail":"Full dev build and tests-off composition build; format 22, DOCUMENTATION_OK; 45/46 focused rows pass; application_agent_invocation_composition_contract fails identically at baseline 2fc1cb2a3 (pre-existing). Mutation pin unchanged. Windows/Linux not built."}},{"criterion":"AC4","outcome":"pass","revision":"667134d4b","evidence":{"kind":"observation","detail":"Completed card and journal line accompany the source commit for the local main fast-forward."}}]
---

# Split github_application.cpp's composition adapters into responsibility files

Round 2 of the `github_application.cpp` refactor ([[BPR-1905]] was round 1).
Lines 348–3738 hold the small adapters, policies and constants the profile
runtime and `github_application::implementation` compose. Both classes keep
these as members or construct them, so the later splits of those classes need
them declared in headers rather than in the root's anonymous namespace.

## Acceptance

- AC1: Move the composition adapters into responsibility-named header/source
  pairs under `src/app/src/github_application/`: review-mutation selection,
  queue notifications, host profiles, device authorization, GitHub
  presentation projections, session settings, blob sources, workbench
  composition, window review lanes and agent composition. Headers declare;
  member and function bodies move to the sources, unchanged.
- AC2: Each moved definition keeps its preprocessor conditions, comments,
  initialization and platform branches. Items the composition contracts read
  in the root (`inside_automated_test_boundary`, `cli_read_offered`, the
  provider using-declarations) stay there.
- AC3: Preserve behavior: dev (tests on) and tests-off composition builds;
  architecture boundaries; the round-trip, composition, public-preview,
  device-authorization, appearance and workbench composition courses; format
  and documentation gates. Reseal the moved code's pins only.
- AC4: Land the change, the completed card and a refactoring journal line on
  local main.

## Result

Source `667134d4b` moves the adapters with unchanged bodies; `github_application.cpp`
goes from 41,674 to 38,467 lines. Files (header / source lines), all under 500:

| Responsibility file | Lines |
|---|---|
| `review_mutation_selection` | 115 / 234 |
| `queue_notifications` | 125 / 155 |
| `profile_hosts` | 335 / 268 |
| `device_authorization_composition` | 339 / 262 |
| `github_presentation_projections` | 107 / 83 |
| `session_settings` | 116 / 106 |
| `blob_sources` | 171 / 215 |
| `workbench_composition` | 203 / 354 |
| `window_review_lanes` | 116 / 71 |
| `agent_composition` | 412 / 389 |

The moves were generated from Clang's AST rather than by hand: in-class member
definitions become declarations plus out-of-line definitions, each definition
keeps its `#if` conditions and comments, and includes are each file's own
declaring headers (test-only ones guarded by `BPR_BUILD_TESTING`). Five
constexpr helpers stay whole in their headers because constexpr definitions
must be visible. `windows_system_root`'s Windows branch, inactive on macOS,
got its guarded `<Windows.h>` by hand; the root's own `<Windows.h>` block,
whose last user moved, is gone. The provider using-declarations,
`cli_read_offered` and `inside_automated_test_boundary` stay in the root,
where the composition contracts read them.

Checks on macOS arm64: full dev build (all targets) and the tests-off
`app-release` composition library build; clang-format 22 check;
`DOCUMENTATION_OK`; the round-trip contract's inventory/digest mode; and 46
focused ctest rows, 45 passing — the architecture rows, `live_mutation_isolation`,
`qt_composed_github_scale_100`, `application_composition_contract`, the
round-trip, workbench, review-ledger, defect, queue, agent chat/finding
composition contracts, every device-authorization, device-binding,
public-preview, profile-window and appearance course, and notification
delivery.

`application_agent_invocation_composition_contract` fails identically at the
round-1 baseline `2fc1cb2a3` (built separately): the seed's canary completes
with full context evidence, the panel reports `Finished: failed_to_start`,
and the seed exits 1. It is pre-existing and unrelated to this move. Windows
and Linux were not built.
