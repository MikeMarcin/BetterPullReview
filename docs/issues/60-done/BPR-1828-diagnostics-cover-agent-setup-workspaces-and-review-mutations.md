---
schema: "implement-rfc.card.v1"
id: "BPR-1828"
title: "Diagnostics cover agent setup, agent workspaces and review mutations"
status: "Done"
rfcs: []
depends_on: ["BPR-1827"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1828--BPR-1828--74fd79528396.json","kind":"defect","found_by":"owner's anonymized bpr-diagnostics-v2 export from 0.4.1 (about 5,600 records) for a failed agent run: only github.*, epoch.* and application.* records, nothing for the agent run, the workspace it visibly set up, the agent setup check, or thread resolution and other review mutations","related_cards":["BPR-1540","BPR-1680","BPR-1722","BPR-1827"],"shared_category":"agent (BPR-1827's agent.run and this card's agent.setup_check and agent.workspace)"}
acceptance: [{"criterion":"AC1: the coverage inventory is recorded, before and after","outcome":"pass","revision":"99901dee2","evidence":{"kind":"observation","detail":"Inventory below, from every diagnostic_log_port::record caller on main 4aba3fb6a and every user-visible host operation that does I/O or can fail.","path":"docs/issues/60-done/BPR-1828-diagnostics-cover-agent-setup-workspaces-and-review-mutations.md"}},{"criterion":"AC2: agent setup checks, workspace preparation and review mutations record started/succeeded/failed/cancelled with typed failures","outcome":"pass","revision":"99901dee2","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_operation_diagnosis|application_operation_diagnostics|application_agent_setup_host)$'","detail":"core_operation_diagnosis: the line round-trips; a failure of another family, a process off a setup check, write flags off a mutation, a trailing word, a leading zero, a 33-bit count, a reordered key and -0 are refused. application_operation_diagnostics: through the wrapper, resolve records started and succeeded, unresolve's refusal failed refused_by_server, a reaction cancelled, a description conflict failed conflict, a comment edit written and read back, a reviewer request started and ended counting reviewers, a search, target read, publication (counting its reconciliation), refused submission (unplaceable_line) and repair their ends; each call reaches the provider once and its answer passes through unchanged; a workspace failure names its reason. application_agent_setup_host test_check_diagnostics: a passed status check records started then succeeded with process success, exit 0, two runs; not signed in records failed not_signed_in, nonzero_exit, exit 1; Stop records cancelled and the late answer nothing."}},{"criterion":"AC3: the logger and the anonymized export admit exactly the line and nothing private","outcome":"pass","revision":"99901dee2","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_release_maintenance|application_agent_launch)$'","detail":"quill_logging: the real logger writes agent.setup_check, agent.workspace and github.review_mutation records with category agent or github and the exact line; it drops a forged line, a line on another family's operation, a success with a failure, a failure without one and a bare event; the metadata preview classifies operations 9 to 11 without their lines. application_release_maintenance test_operation_export: under both policies each line exports exactly under its category; one with a thread, path, prompt, body or unknown failure word keeps the record with its detail omitted and no PRIVATE text; an agent record filed under category github is refused. BPR-1827's agent.run line now exports with category agent and its exact detail; before this change the rich classifier computed category github for it, so its records were refused."}},{"criterion":"AC4: emitted from the application layer and composed","outcome":"pass","revision":"99901dee2","evidence":{"kind":"command","command":"cmake --build build/dev --target src/app/CMakeFiles/better_pull_review_composition.dir/src/github_application.cpp.o; python3 tests/end_to_end/composition_contract_tests.py --cmake cmake","detail":"agent_setup_host, epoch_application_host's workspace forwarders and application::with_review_mutation_diagnostics record; no widget or adapter emits them. github_application.cpp gives agent setup the profile log and wraps the live mutation provider after its unchanged erasure. Resealed: full application digest, production mutation-branch digest (53d6792a1 to 70fe8a7e4) and production app manifest digest."}},{"criterion":"AC5: gates","outcome":"pass","revision":"99901dee2","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 scripts/verify_documentation.py --root .","detail":"coding-format-check passes with clang-format 22.1.8 over 1,871 files; DOCUMENTATION_OK; core_boundary_contract, architecture_boundaries and architecture_boundary_contract pass; composition_contract_tests.py ran 8, OK. Controls: with the logger check disabled quill_logging fails at an operation line that is not exactly a diagnosis never enters the log; with the old category rule restored application_release_maintenance fails at an operation record exports under either policy. test_network_isolation_manifest fails on two rows this card does not touch (application_agent_chat_custom_agent_contract, gh_cli_git_smart_transport)."}}]
---

# BPR-1828: Diagnostics cover agent setup, agent workspaces and review mutations

The owner exported diagnostics from 0.4.1 to debug a failed agent run. The
anonymized `bpr-diagnostics-v2` export held about 5,600 records, all
`github.*`, `epoch.*` and `application.*`. The run visibly went through
"setting up a workspace", but nothing about the run, the workspace, the agent
setup check or the review mutations around it was in the export.

[[BPR-1827]] adds the agent run's own `agent.run` records. This card covers
the rest of the agent path (setup check, workspace) and the review mutations,
and records what remains uncovered.

## Acceptance

- **AC1, inventory:** every user-visible operation that does I/O or can fail
  is listed with whether it emits diagnostics, before and after.
- **AC2, events:** agent setup checks, agent workspace preparation and removal,
  and review mutations record their start (writes) and their end as
  `succeeded`, `failed` or `cancelled`, with a typed failure.
- **AC3, privacy:** the logger and the anonymized export admit exactly the
  line (enums, counts, a duration, an exit code) and nothing private.
- **AC4, layering:** emitted from the application layer, composed in
  `src/app`; composition digests resealed.
- **AC5, gates:** clang-format 22, focused tests, documentation gate.

## Design

- **One line for three families.** `core::operation_diagnosis`
  (`bpr/core/operation_diagnosis.hpp`) is
  `action=ACTION failure=FAILURE process=PROCESS exit=none|N duration_ms=N
  count=N written=0|1 read_back=0|1`, following the discovery line's pattern:
  the logger re-parses the event's sanitized detail and drops anything else.
  The action decides the family and so the operation: `agent.setup_check`,
  `agent.workspace` or `github.review_mutation`. The failure must be one of
  the family's own: `core::agent_setup_failure`, `agent_workspace_failure`
  (for `unavailable`, its `agent_workspace_unavailable_reason`), or
  `review_mutation_failure`, plus `conflict` for an edit GitHub's text
  overtook. Only a setup check names a process outcome and exit code. Only a
  mutation says whether it wrote and whether it learned the outcome by
  reading back.
- **One vocabulary.** `application::diagnostic_operation_names` is the only
  list of logged operation names; the logger and the export classifier both
  read it, where each had its own copy. A record's category is its
  operation's first word (`application`, `epoch`, `github`, `agent`).
  [[BPR-1827]] had added `agent.run` to the logger's and the metadata
  classifier's copies and given it category `agent` in the logger, but the
  rich classifier still computed `github` for it, so an anonymized or
  identifying export refused every `agent.run` record. The shared rule and
  an admitted `protocol=` line fix that.
- **Where it is emitted.** `agent_setup_host` records each check
  (`begin_check`, `settle_check`; a check reset while running is a
  cancellation). `epoch_application_host`'s public
  `materialize_agent_workspace` and `cleanup_agent_workspace` time and record
  each call. `application::with_review_mutation_diagnostics` wraps the live
  provider and forwards every call unchanged. The composition keeps the
  provider's erasure verbatim and wraps afterwards; the wrapper's name keeps
  the composition's single `review_mutation_port` reference, the contract's
  guard against direct bypasses, intact.
- **Volume.** All new records are standard detail. Mutations, checks and
  workspaces are a few per review, far inside [[BPR-1722]]'s 16,384-record
  budget.

## Coverage inventory

Before is main at 4aba3fb6a (with [[BPR-1827]]); after is this card.

| Operation | I/O | Before | After |
|---|---|---|---|
| Application start, shutdown | process | `application.*` | unchanged |
| Sign-in viewer read, capability probe | GitHub | `github.viewer`, `github.capability` | unchanged |
| Pull request read and pages | GitHub | `github.pull_request` | unchanged |
| Comparison, Git cache, smart transport | Git, SQLite | `epoch` stages | unchanged |
| Home discovery refresh and reads | GitHub | `github.discovery` | unchanged |
| Review submission (Submit, Check status) | GitHub, SQLite | `github.review_submission` ending or unknown reason | unchanged |
| Inline comment publication | GitHub | `github.review_comment` failed or response only, detail omitted on export | also `github.review_mutation` `publish_comment` start and end |
| Agent run | process | `agent.run` ([[BPR-1827]]), refused by the rich export | `agent.run` exported under `agent` |
| Agent setup, Check it works | process | none | `agent.setup_check` start and end |
| Agent workspace, setting up and removing | Git, files | none | `agent.workspace` end of materialize and cleanup |
| Thread resolve and unresolve | GitHub | none | `github.review_mutation` start and end |
| Reaction add and remove | GitHub | none | `github.review_mutation` start and end |
| Description and comment edit | GitHub | none | `github.review_mutation` start and end |
| Reviewer search and request | GitHub | none | search end; request start and end |
| Target read before a write, publication reconcile and repair | GitHub | none | `github.review_mutation` end |
| Conversation replies, tour publication (through review submission) | GitHub | the submission session's records | unchanged |
| Device sign-in, credential vault, account switch | GitHub, keychain | none | none |
| Updater check, download, staging | network, files | none | none |
| MCP context helper | process | none | none |
| Tour import and remote tour CLI | GitHub, files | none | none |
| SQLite stores (private review, drafts, ledger, recovery, settings) | SQLite | none, except the submission reason kept in recovery | none |
| Linked clone scan and workspace source reads | files | none | none |
| Source search, blob snippets, markdown images, avatars | GitHub, network | none | none |
| Review Cycle export, import, publish | files, GitHub | none | none |
| Notifications | platform | none | none |
| Diagnostics export and profile purge | files | none (the export reports its own counts) | none |

## Left uncovered

The last rows above. The most useful next ones are the device sign-in and
credential vault (a failed sign-in leaves no trace), the updater, and the MCP
helper's supervision. Each needs its own closed failure vocabulary and its own
privacy review (hosts, account names and file paths are near every one of
them), so they are left for their own cards rather than squeezed into this
line. The fixture composition (`BPR_BUILD_TESTING`) borrows the fixture's
mutation provider by reference and is not wrapped, so fixture courses show no
`github.review_mutation` records; the wrapper's own tests and the live
composition cover them.
