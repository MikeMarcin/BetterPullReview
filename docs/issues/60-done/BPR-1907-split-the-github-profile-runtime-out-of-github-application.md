---
schema: "implement-rfc.card.v1"
id: "BPR-1907"
title: "Split the GitHub profile runtime out of github_application.cpp"
status: "Done"
rfcs: []
depends_on: ["BPR-1906"]
priority: "P2"
metadata: {"baseline":"6b521dc18","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass; subfolders welcome."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"240c2d4d6","evidence":{"kind":"observation","detail":"profile_runtime.hpp declares github_profile_runtime; construction, sign-in, release-access, shutdown and presentation sources define it unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"240c2d4d6","evidence":{"kind":"observation","detail":"cli_accounts, startup_failures, defect_identity and insights_metrics files."}},{"criterion":"AC3","outcome":"pass","revision":"240c2d4d6","evidence":{"kind":"command","detail":"composition_contract_tests.py and the round-trip contract read the composition-root set; application_composition_contract and the round-trip contract pass, including hostile cases."}},{"criterion":"AC4","outcome":"pass","revision":"240c2d4d6","evidence":{"kind":"command","detail":"Full dev build, tests-off composition build, format 22, 73/73 focused rows pass; mutation pin unchanged. Windows/Linux not built."}},{"criterion":"AC5","outcome":"pass","revision":"240c2d4d6","evidence":{"kind":"observation","detail":"Completed card and journal line accompany the source for the local main fast-forward."}}]
---

# Split the GitHub profile runtime out of github_application.cpp

Round 3 of the `github_application.cpp` refactor. `github_profile_runtime`
(one BPR profile's providers, hosts, stores, effect lanes and windows) is a
2,561-line class with a 790-line constructor, plus 300 lines of members
already defined after it. The startup, public `github_application` members,
`implementation` and the fixture accessors all use it, so it needs a
declaration header before `implementation` can be split.

## Acceptance

- AC1: Declare `github_profile_runtime` in a private header and define its
  members, unchanged, in responsibility sources: construction (constructor,
  service start, identity), sign-in and connections, release access,
  shutdown, and presentation/activity/windows.
- AC2: Give the root's remaining shared helpers their own files: CLI account
  use and the CLI read offer, startup failure records, defect identity and
  Insights process metrics.
- AC3: The composition contract's source scans read the composition root's
  whole source set (the root plus `github_application/`), with the same
  required and hostile cases.
- AC4: Preserve behavior: full dev and tests-off builds; architecture,
  composition, round-trip and the composed courses that drive a profile;
  format and documentation gates; reseal the moved code's pins only.
- AC5: Land the change, the completed card and a journal line on local main.

## Result

Source `240c2d4d6` (regenerated over main `85744eb28` after another session rebased round 2 and landed BPR-1912/1913; the generated files are byte-identical to the first cut `e90e45a32`). `github_application.cpp` goes from 38,470 to 35,440 lines.

| File (header / source lines) | Responsibility |
|---|---|
| `profile_runtime.hpp` (686) | `github_profile_runtime`'s declaration: 244 members and its nested types |
| `profile_runtime_construction.cpp` (1,290) | the 790-line constructor, service start and identity initialization |
| `profile_runtime_sign_in.cpp` (371) | connections, native/CLI sign-in, startup selection, device refresh |
| `profile_runtime_release_access.cpp` (122) | release-update access and its credential vault |
| `profile_runtime_shutdown.cpp` (217) | shutdown and cleanup |
| `profile_runtime.cpp` (484) | presentation, activity, discovery and windows |
| `cli_accounts` (58 / 52) | CLI account use and the CLI read offer |
| `startup_failures` (13 / 83) | first startup refusal and failure records |
| `defect_identity` (25 / 33), `insights_metrics` (16 / 21) | defect thread identity; Insights process counters |

Over 500 lines, both within the owner's 2,000-line ceiling for this pass:
`profile_runtime_construction.cpp` is one constructor of 790 lines plus the
two start-up steps it runs, which read best together, and `profile_runtime.hpp`
is the class's declaration.

Generation now parses the tests-off build too, so includes used only by
production branches arrive guarded by `!defined(BPR_BUILD_TESTING)`. A first
cut guarded `github_cli_runtime.hpp` by the `BPR_GITHUB_CLI_PLATFORM` macro it
defines; the tests-off Release self-test inside `application_composition_contract`
failed with `CLI_ENTRY_ABSENT_FAILED`. Includes are now guarded only by
command-line or platform macros, and code testing a header-defined `BPR_` macro
includes its header.

The composition contracts read the composition root's whole source set: the
Python contract's provider-selection, CLI-gate and boundary-answer checks, and
the round-trip contract's async mutation-host inventory and product strings
(split files' include lines excluded). Digests still read the root alone.

Checks on macOS arm64: full dev build; tests-off composition build;
format 22; and 73 focused rows all passing, including
`application_composition_contract` with its Release self-test, the round-trip,
workbench, review-ledger, defect, queue, agent chat/finding contracts,
architecture, live-mutation isolation, `qt_composed_github_scale_100`,
`qt_composed_cli_read_consent`, every profile, startup, CLI, device and
public-preview course. Windows and Linux were not built.

The separate `application_composition_source_contract` row (seven source and
compiler claims) was missing from the first selection and failed on two
consequences of the split: the async mutation-host hostile course writes the
root to a temporary directory, so the split files are now copied beside it;
and `cli_read_offered_here` is now defined out of line, so its gate pattern
accepts a class qualifier with the same required body. That row and the
round-trip contract pass on `240c2d4d6`; the combined build over main's
BPR-1913 change passes the architecture, composition, round-trip,
CLI-consent and review-tour courses.

`application_review_tour_publication_manual` fails identically at round 1
(`2fc1cb2a3`), at main `85744eb28` and here (`read operation=8 count=10
recording-failure=9`, "actual provider snapshot renders draft PR status"): it
predates this refactor, like `application_agent_invocation_composition_contract`.
