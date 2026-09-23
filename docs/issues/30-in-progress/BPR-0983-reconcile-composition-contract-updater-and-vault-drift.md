---
schema: "implement-rfc.card.v1"
id: "BPR-0983"
title: "Reconcile the composition contract with the updater's boundary readers and the moved vault selection"
status: "In Progress"
rfcs: ["RFC-BPR-0035"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"RFC-BPR-0015 r3 closeout focused checks, 2026-09-22","found_at":"2218ef1f211e689ed41444a1832326e0c458c421","origin_commits":["5ed24a8c","d1e0f46c","65dd7dd4"],"related_rfcs":["RFC-BPR-0014"]}
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
