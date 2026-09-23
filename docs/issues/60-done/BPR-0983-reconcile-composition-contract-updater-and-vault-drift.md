---
schema: "implement-rfc.card.v1"
id: "BPR-0983"
title: "Reconcile the composition contract with the updater's boundary readers and the moved vault selection"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"RFC-BPR-0015 r3 closeout focused checks, 2026-09-22","found_at":"2218ef1f211e689ed41444a1832326e0c458c421","origin_commits":["5ed24a8c","d1e0f46c","65dd7dd4"],"related_rfcs":["RFC-BPR-0014"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b48bda8b","evidence":{"kind":"command","detail":"Routed through the composition's one reader: inside_automated_test_boundary() in github_application.cpp reads the marker only in tests-enabled graphs and answers false otherwise; the profile composition, release_update_desktop.cpp and profile_desktop.cpp ask it. test_the_automated_test_boundary_has_one_reader passes and refuses six hostile sources (second reader in main, updater reading for itself, reader removed, reader only in a comment, reader in every graph, profile ignoring the reader). Binary control: restoring the updater's own read makes the Release scan find the marker.","path":"docs/issues/60-done/BPR-0983-reconcile-composition-contract-updater-and-vault-drift.md#ac1-the-updater-asks-the-compositions-one-reader"}},{"criterion":"AC2","outcome":"pass","revision":"b48bda8b","evidence":{"kind":"command","detail":"test_concrete_github_selection_is_owned_by_composition requires make_platform_vault( in github_application.cpp, all three platform vault factories in src/app/src/platform_credential_vault.cpp, and no other file under src/ outside src/adapters/github naming one; passes. Before: 3 vault subtests failed at df077faa.","path":"docs/issues/60-done/BPR-0983-reconcile-composition-contract-updater-and-vault-drift.md#ac2-the-vault-selection-assertion-follows-the-file-that-owns-it"}},{"criterion":"AC3","outcome":"pass","revision":"b48bda8b","evidence":{"kind":"command","detail":"ctest build/dev -R '^application_composition_contract$' passes on macOS in 202.67 s, including the real BUILD_TESTING=OFF Release configure, build of app_better_pull_review, verify_composed_hosts production profile and binary scan with BPR_AUTOMATED_TEST_BOUNDARY absent. application_review_round_trip_composition_contract passes after the reseal; 16 boundary and neighbour rows pass.","path":"docs/issues/60-done/BPR-0983-reconcile-composition-contract-updater-and-vault-drift.md#evidence"}}]
---

# BPR-0983: Reconcile the composition contract with the updater's boundary readers and the moved vault selection

## Observation

`application_composition_contract` (`tests/end_to_end/composition_contract_tests.py`)
fails at the unchanged `2218ef1f` baseline for three reasons that are not
RFC-BPR-0015's. They were found while reconciling that RFC's own guards on
[[BPR-0982]], which owns the CLI parts of the same test methods.

1. `test_concrete_github_selection_is_owned_by_composition` looks for
   `make_macos_keychain_vault(`, `make_windows_credential_manager_vault(` and
   `make_linux_secret_service_vault(` in `src/app/src/github_application.cpp`.
   65dd7dd4 (2026-09-21, retained-lease maintenance) moved that selection to
   `src/app/src/platform_credential_vault.cpp`, still inside the composition
   root. The assertion reads a stale path; nothing indicates the selection left
   `src/app`.
2. `test_the_automated_test_boundary_has_one_reader` finds two readers of
   `BPR_AUTOMATED_TEST_BOUNDARY` outside `github_application.cpp`:
   `src/app/src/release_update_desktop.cpp` (5ed24a8c, BPR-0947), which
   downgrades the update capability to `untrusted_installation` when the
   variable is set, and `src/app/src/profile_desktop.cpp` (d1e0f46c, BPR-0948),
   which then declines the update-transaction lifetime. Both readers are in
   every graph.
3. As a consequence, `test_tests_off_release_graph_omits_public_path_fixture`
   finds the bytes `BPR_AUTOMATED_TEST_BOUNDARY` in the tests-disabled Release
   binary, which [[BPR-0797]] and [[BPR-0802]] require to carry no such reader.

Each reader makes a shipped build *safer* when the variable is set (the
updater refuses to install), so this is not an observed escalation. It does
give a stray environment variable a behavior in a shipped build, which is the
thing the contract forbids.

## Acceptance

- AC1: Decide where the updater learns it is inside the automated test
  boundary. Either route it through the composition's single reader, or amend
  the one-reader contract with an explicit, reviewed exception that keeps the
  variable out of the tests-disabled graph. Do not simply delete the check.
- AC2: Point the vault-selection assertion at the composition file that now
  owns it, without weakening what "owned by composition" means.
- AC3: `application_composition_contract` passes in `build/dev` on macOS, with
  the tests-disabled Release scan run for real.

## Boundaries

No updater, vault or credential behavior change is authorized by this card
beyond what AC1 decides. RFC-BPR-0015's CLI consent and discovery parts of the
same contract belong to [[BPR-0982]].

## Resolution — 2026-09-23, Mac integrator

### AC1: the updater asks the composition's one reader

Decision: route, not an exception. The marker keeps exactly one reader,
`bpr::app::inside_automated_test_boundary()`, declared in
`src/app/include/bpr/app/github_application.hpp` and defined in
`src/app/src/github_application.cpp`:

- In a tests-enabled graph it returns `!qEnvironmentVariableIsEmpty("BPR_AUTOMATED_TEST_BOUNDARY")`,
  the same rule the composition already used (any non-empty value is inside).
- In a tests-disabled graph it returns `false`, and the name is not compiled.

Its three askers:

- a profile's composition: `InsideAutomatedTestBoundary = inside_automated_test_boundary() || aFixture.has_value();`
  (unchanged answer);
- `release_update_desktop.cpp`, where inside the boundary nothing is installed
  (`untrusted_installation`) and View Release opens no browser;
- `profile_desktop.cpp`, where inside the boundary no update lifetime is taken.

In a tests-enabled graph every answer is the same as before, except that a
variable set to the empty string is now outside for the updater too, as it
already was for the composition. In a tests-disabled graph the variable no
longer stops installs, the View Release browser or the update lifetime,
which is what the one-reader contract asks: a stray environment variable has
no behaviour in a shipped build. No credential, provider, vault, transport or
mutation path changes. `docs/architecture/system-architecture.md` names the
reader and its askers.

### AC2: the vault selection assertion follows the file that owns it

`test_concrete_github_selection_is_owned_by_composition` now requires that
`github_application.cpp` selects the vault through `make_platform_vault(`, that
`src/app/src/platform_credential_vault.cpp` names all three platform vault
factories, and, new, that no file under `src/` outside that one and
`src/adapters/github/` (which defines them) names any of them. "Owned by
composition" is therefore stronger than before: it was a presence check in one
file, and it is now presence in the composition root plus absence everywhere
else.

### Contract changes

`tests/end_to_end/composition_contract_tests.py`:

- `AUTOMATED_TEST_BOUNDARY_READER` is the reader function with its
  tests-enabled read and its `false` otherwise;
  `AUTOMATED_TEST_BOUNDARY_PROFILE_ANSWER` is the profile's use of it; the
  findings require each exactly once and the literal nowhere else in `src/`.
- The updater and the update lifetime must call the reader.
- Hostile controls, each detected: the pre-BPR-0802 second reader in
  `main.cpp`; the pre-BPR-0983 updater reading the marker for itself; the
  reader removed; the reader left only in a comment; the reader compiled into
  every graph; the profile ignoring the reader.

### Composition pins

`application_review_round_trip_composition_contract.cmake`: the full
`github_application.cpp` digest and the production application manifest are
resealed with a review comment. Reviewed range `9903869f..` this change;
`git log 9903869f..HEAD` over the pinned scope listed nothing else. The
mutation-branch pin is unchanged (`a7e2081e…`). Review of the change inside
the pinned scope: the new free function and its declaration, the profile's
reader line (same answer; a blank line after it keeps the neighbouring
assignments byte for byte, which the contract's hostile consent controls
search for), and two comment rewrites; nothing on a credential, provider,
vault, transport, publication or mutation path.

## Evidence

macOS 26 arm64, `build/dev`, at the BPR-0983 content on `df077faa`
(`4a16ebdc`, which arrived before the commit, changes only Linux course
scripts and cards):

- **Before**, the contract's six source methods at `df077faa`: 4 failures,
  exactly this card's findings (the three vault subtests and
  `test_the_automated_test_boundary_has_one_reader`).
- **After**, `ctest -R '^application_composition_contract$'` passes in
  202.67 s, including `test_tests_off_release_graph_omits_public_path_fixture`,
  which configures and builds `app_better_pull_review` for real in a temporary
  `BUILD_TESTING=OFF` Release tree, runs `verify_composed_hosts.py --profile
  production` on it and scans the binary: `BPR_AUTOMATED_TEST_BOUNDARY` is
  absent.
- **Binary control**: with only the updater's pre-BPR-0983 read restored
  (`qEnvironmentVariableIsSet("BPR_AUTOMATED_TEST_BOUNDARY")` in
  `release_update_desktop.cpp`), the Release scan fails on
  `forbidden='BPR_AUTOMATED_TEST_BOUNDARY'` and the one-reader test fails
  (184.18 s); source restored.
- Six source-level hostile controls for the one reader are each detected
  (listed above).
- `application_review_round_trip_composition_contract` passes after the
  reseal (11.29 s); before the reseal it failed on the full-file digest.
- Boundary askers and neighbours, all pass: `core_release_update`,
  `application_release_update`, `release_update_transport_contract`,
  `release_update_native_https`, `release_update_transaction`,
  `release_update_native_staging`, `release_update_recovery`,
  `application_release_update_public_path`,
  `application_device_authorization_browser_handoff`,
  `application_device_authorization_browser_absent`,
  `application_composition_capabilities_inside_boundary`,
  `application_composition_capabilities_product_run`,
  `application_poller_liveness`, `composed_application_hosts`,
  `application_public_path`, `application_setup_destination_public_path`.
- `application_profile_empty_desktop_native_menu_bar` fails at "closing the
  last window takes no menu off the bar" with and without this change (the
  product source restored to `df077faa` and rebuilt), so it is not this
  card's. The course needs an active, unlocked console
  ([[BPR-0782]]); this run was unattended.
- clang-format 23.1.1 passes the four touched C++ files; clang-tidy on the
  three touched translation units reports only findings on untouched lines
  (`github_application.cpp` 8155, 15655, 15657; `profile_desktop.cpp` 1251;
  `home_view_state.hpp`; `dif_runtime.hpp`).
