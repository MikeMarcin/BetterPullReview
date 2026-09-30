---
schema: "implement-rfc.card.v1"
id: "BPR-1228"
title: "Restore the application composition contracts at main"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"reported red at baseline by the BPR-1211 implementer; diagnosed at main 22d184166 by the Review Cycles intake session on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1228--BPR-1228--e74f102785c8.json"}
acceptance: [{"criterion":"The two named ctest entries pass on macOS","outcome":"pass","revision":"46e0d07b2","evidence":{"kind":"command","detail":"macOS arm64 dev preset: application_review_round_trip_composition_contract pass (13.1 s) at 46e0d07b2; application_composition_contract pass (363 s) in the full dev run with the same test bytes; its tests-off release-graph course also passed alone (432 s, real Release configure and build)."}},{"criterion":"The reseal comment names each pinned-scope commit and what it changes","outcome":"pass","revision":"46e0d07b2","evidence":{"kind":"observation","detail":"Reviewed aa8826e5..e9e65eab: 27dbeef51, 637cd41df, eca094604, bf7a96367, 9ad03a828, 52fa4a06c, acebc2089, each named with its change in the cmake reseal comment and in the card's review table; mutation-branch pin unchanged; new composition sources join the tree inventory only."}},{"criterion":"Documentation gate","outcome":"pass","revision":"46e0d07b2","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1228: Restore the application composition contracts at main

## Observation

At `main` 22d184166 on macOS arm64,
`ctest --test-dir build/dev -R "application_review_round_trip_composition_contract|^application_composition_contract$" --output-on-failure`
fails both tests. There are three separate causes, and only the first is a
reseal.

1. **Pinned composition digest.** `tests/end_to_end/application_review_round_trip_composition_contract.cmake:481`
   reports the full LF-normalized `src/app/src/github_application.cpp` digest
   drifted: expected `fa135765…265e6f`, found `bb53bee5…e78e17`. The pin was
   last resealed at aa8826e5a. Five later commits change the file without a
   reseal: 27dbeef51 and 637cd41df (tour owner channel and live-draft routing,
   RFC-BPR-0045), eca094604 (agent tour-writing tickets), bf7a96367 (BPR-1214)
   and 9ad03a828 (BPR-1211). The pin's convention is a reviewed reseal: a
   comment block records what each pinned-scope commit changes and that the
   production mutation branch and grants are unchanged. That review has not
   been done, so this is not a mechanical hash update.
2. **Ambiguous testing grant.** `composition_contract_tests.py`
   `test_whole_review_authority_is_distinct_and_test_factory_is_private`
   compiles `make_review_submission_testing_grant({}, {}, {})` in the testing
   graph. a6513264c added a second overload taking `const staged_review_plan&`,
   so the braced call is ambiguous and the testing declaration no longer
   compiles. The probe needs a typed third argument (or the overloads a
   distinct name); decide which keeps the test's intent that the factory exists
   only in testing graphs.
3. **Release graph names the public-path fixture.**
   `test_tests_off_release_graph_omits_public_path_fixture` asserts
   `github_public_path_fixture.cpp` is absent from the tests-off `build.ninja`.
   It is not compiled, but `src/adapters/logging/CMakeLists.txt` globs every
   `src/*.cpp`/`*.hpp` into `CMAKE_CONFIGURE_DEPENDS` for the diagnostic build
   identity (ac64e5ad0/3381a02c5), so the file appears in the "re-run CMake"
   rule. Either the test should inspect compile edges only (it already checks
   `compile_commands.json`) or the identity glob should exclude testing-only
   sources; that is a design choice about what the diagnostic build identity
   binds.

## Outcome

Both contract tests pass at the resulting `main` head, with each cause fixed at
its owner: a reviewed reseal comment for the five commits, a probe or overload
change that keeps the testing-only factory contract, and a decision on the
release-graph assertion versus the diagnostic identity glob.

## Direct acceptance

- The two named ctest entries pass on macOS at the candidate revision.
- The reseal comment names each pinned-scope commit and what it changes.
- `python3 scripts/verify_documentation.py --root .` passes.

## Resolution (main stabilization milestone, macOS arm64, 2026-09-29)

Candidate `46e0d07b2`. Two further causes surfaced once the first was
resealed: seven pinned-scope commits rather than five (52fa4a06c and
acebc2089 landed after the observation), and new composition sources the
source-tree inventory did not list.

### Review of the pinned scope, aa8826e5..e9e65eab

`git log aa8826e5a..HEAD` over `github_application.cpp` and every file in
`bpr_production_app_sources` names seven commits. They touch only
`github_application.cpp`, `github_application.hpp` and `profile_desktop.cpp`.

| Commit | What it changes in the pinned scope | Review |
|---|---|---|
| 27dbeef51 | Makes the tour owner endpoint beside activation, bound to the same runtime lease; `profile_desktop` polls it one bounded step at a time on the desktop effect lane, and a closing profile waits for the poll | No provider, credential or GitHub path. Exceptions on the poll are swallowed and the reply closes its socket, so a caller sees no answer rather than a wrong one |
| 637cd41df | Routes a forwarded command on the decision lane to the window's live draft when one holds it, otherwise to the profile store on `WorkbenchEffects`; a mutation is answered once durable (5 s bound, then `save_pending`), preedit text answers `editor_busy`; the window lists its draft in the local tour catalog | Store access stays off the decision lane. A request still waiting when the runtime is destroyed is dropped and its socket closed; the CLI reports no answer. Acceptable, noted |
| eca094604 | Assist issue/complete/cancel and outline use through the same route; the owner supplies its own clock and hook revision | The agent supplies text only; no model or provider call in the application. `assist_refusal_code` is an identity wrapper (cosmetic) |
| bf7a96367 | `BPR_BUILD_TESTING` threaded-reply probe phases for BPR-1214 | Test-only; asserts zero submissions and replies |
| 9ad03a828 | BPR-1211: every ledger open, read and write on `ReviewLedgerEffects`; answers delivered through `ledger_answer`, guarded by window lifetime, `Closing` and `ReviewLedgerGeneration`; private review opens after the ledger answers | Reviewed on BPR-1211; rechecked here. Correlated-defect persistence ignores the result as before. Store failure paths stay typed |
| 52fa4a06c | Threaded-reply probe waits for a busy refresh action | Test-only |
| acebc2089 | Composes `review_cycle_desktop` and joins its shutdown and quiescence | Local cycle store, workspace and Qt only |

No grant, target, provider, transport, vault or publication path changes; the
production mutation-branch digest (`e067befe…`) is unchanged. The new
composition sources `tour_owner_service`, `tour_assist`, `tour_cli_types` and
`review_cycle_desktop` join the source-tree inventory only, like
`tour_publication_plan` and `system_browser_opener` before them: they include
the local tour store, workspace and Qt headers, nothing from the GitHub
adapter or the vault. The reseal comment in
`application_review_round_trip_composition_contract.cmake` records this.

9ad03a828 also realigned the `worker_executor ReviewMutationEffects;` member,
which the async-host inventory matched with its exact run of spaces. The check
now matches `worker_executor[ \t]+ReviewMutationEffects;`; a probe run shows it
still refuses `ReviewLedgerEffects` and a non-member spelling.

### The testing grant probe

The probe now calls each overload with a typed third argument
(`review_submission_request{}` and `staged_review_plan{}`). With
`BPR_APPLICATION_TESTING` both compile; without it the call is still refused as
undeclared, so the factory stays testing-only.

### The release graph

Decision: the diagnostic build identity binds the whole `src/` tree, as it
already does for sources other platforms compile, and the test checks build
edges. `ninja_build_edges` drops only the `RERUN_CMAKE` statement and the
input-less `phony` that declares configure dependencies; any compile, link or
custom-command edge that names `github_public_path_fixture.cpp` still fails.
A self-test proves both halves on a synthetic graph.

### Checks

| Check | Result |
|---|---|
| `ctest --test-dir build/dev -R "^application_review_round_trip_composition_contract$"` at `46e0d07b2` | Pass, 13.1 s |
| `application_composition_contract` in the full dev run | Pass, 363 s |
| `test_tests_off_release_graph_omits_public_path_fixture` alone (real tests-off Release configure and build) | Pass, 432 s |
| `test_whole_review_authority_is_distinct_and_test_factory_is_private`, `test_build_edges_keep_compiles_and_drop_only_regeneration` | Pass |
