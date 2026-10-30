---
schema: "implement-rfc.card.v1"
id: "BPR-1830"
title: "Diagnostics cover the MCP helper, tours and Review Cycle rounds"
status: "Done"
rfcs: []
depends_on: ["BPR-1828"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1829--BPR-1831--37b8c8c13a6b.json","kind":"defect","found_by":"BPR-1828's coverage inventory: the MCP context helper, tour import and the remote tour CLI, and Review Cycle export, import and publish left no diagnostic record","related_cards":["BPR-1827","BPR-1828","BPR-1829","BPR-1831"],"shared_files":"diagnostic_log_port.hpp operation names, quill_log.cpp operation-line admission and core operation_diagnosis enums, appended only (BPR-1829 and BPR-1831 extend the same lists)"}
acceptance: [{"criterion":"AC1: the coverage inventory is recorded, before and after","outcome":"pass","revision":"9829edb3f","evidence":{"kind":"observation","detail":"Inventory below, from BPR-1828's table on main 53393c95a (with BPR-1829 and BPR-1831) and every helper, tour and Review Cycle path that does I/O or can fail.","path":"docs/issues/60-done/BPR-1830-diagnostics-cover-the-mcp-helper-tours-and-review-cycle-rounds.md"}},{"criterion":"AC2: MCP helper requests, tour import, tour read and publish (CLI and app) and Review Cycle round read, grant and act record started/succeeded/failed/cancelled with typed failures","outcome":"pass","revision":"9829edb3f","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_operation_diagnosis|application_helper_tour_round_diagnostics|application_operation_diagnostics|application_review_tour_host|sqlite_review_cycle_runtime|cycle_cli_public_path|tour_cli_loopback_provider)$'","detail":"core_operation_diagnosis: the helper, tour and round lines round-trip; a failure of another family, a process on a tour, write flags on a round and a trailing cycle=, repository= or root= word are refused. application_helper_tour_round_diagnostics: the supervisor records not_pinned and already_running refusals, a start before the run, success with process success, exit 0 and two answered lines, helper_failed with nonzero_exit exit 3, a cancellation and timed_out; without a log it behaves the same; the tour host counts two offered tours and records not_offered, target_mismatch, a successful import counting one stop, draft_exists, and a terminal's not_offered; tour CLI errors map by code then exit status; no record holds a path or the canary. sqlite_review_cycle_runtime: through the real store a grant records its start and a typed failure, a granted read and a bundle import their ends, and a revoke refused at shutdown its start and cancellation. cycle_cli_public_path: the real bpr cycle course leaves cycle.round freeze, grant (failed not_authorized and succeeded), read (not_granted, revoked, succeeded), respond (succeeded and not_granted) and revoke records in the profile's log, nothing else, and none of the project path or name, README.md, note text, goal text or response ID. tour_cli_loopback_provider: a never-signed-in bpr tour open leaves one tour.operation failed authentication_unavailable record and no repository or host."}},{"criterion":"AC3: the logger and the anonymized export admit exactly the line and nothing private","outcome":"pass","revision":"9829edb3f","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(quill_logging|application_release_maintenance)$'","detail":"quill_logging: the real logger keeps agent.mcp_helper (category agent), tour.operation (tour) and cycle.round (cycle) records with their exact line, drops a round line on the tour operation and a cancellation naming another failure, and the metadata preview classifies operations 20 to 22 (after BPR-1829's 12 to 14 and BPR-1831's 15 to 19) without their lines. application_release_maintenance test_operation_export: under both policies each new line exports exactly under its category; one with root=, label= or an unknown failure word keeps the record with its detail omitted and no PRIVATE text."}},{"criterion":"AC4: emitted from the host and application layers and composed","outcome":"pass","revision":"9829edb3f","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^application_review_round_trip_composition_contract$'","detail":"mcp_context_helper_supervisor, review_tour_host and review_cycle_runtime record; bpr tour's remote steps record in tour_runtime, the CLI's own host. No widget or adapter emits them. github_application.cpp hands the profile's log to the MCP supervisor, the tour host, the window's Review Cycle desktop and the cycle owner service. Rebased onto 53393c95a (after BPR-1829 and BPR-1831) and resealed on the merged tree: full application digest ed9214b8 to af058919, production app manifest digest to 300cff20; the source inventory adds terminal_diagnostic_log.{cpp,hpp}. application_agent_finding_composition_contract, application_agent_finding_mcp_construction_cut_contract, application_mcp_context_helper, core_boundary_contract and architecture_boundaries pass; composition_contract_tests.py and verify_composed_hosts_tests.py passed on the tree merged with BPR-1829 (not rerun after BPR-1831's merge, which changed no file they read for this card)."}},{"criterion":"AC5: gates","outcome":"pass","revision":"9829edb3f","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 scripts/verify_documentation.py --root .","detail":"coding-format-check passes with clang-format 22.1.8 over the merged tree; DOCUMENTATION_OK from the main checkout's verify_documentation.py with --root at this worktree. Control: with the bpr cycle command's set_diagnostics given nullptr, cycle_cli_public_path fails at the commands wrote the profile's diagnostic log; restored, it passes. Shared lists are appended after BPR-1829's entries; BPR-1830 reuses BPR-1829's corrupt, busy and invalid_bundle and BPR-1831's schema_mismatch failures rather than declaring them twice; application_host_operation_diagnostics (BPR-1831) still passes."}}]
---

# BPR-1830: Diagnostics cover the MCP helper, tours and Review Cycle rounds

[[BPR-1828]] added the operation line and left the MCP context helper, tour
import and the remote tour CLI, and Review Cycle export, import and publish
without a record. This card covers them. [[BPR-1829]] (sign-in, vault,
updater) and [[BPR-1831]] (SQLite, clone scan, search, images, avatars,
notifications) extend the same lists in parallel, so every shared edit here is
an append.

## Acceptance

- **AC1, inventory:** each helper, tour and Review Cycle operation that does
  I/O or can fail is listed with whether it emits diagnostics, before and
  after.
- **AC2, events:** MCP helper requests, tour import, tour read and publish
  (CLI and app) and Review Cycle round read, grant and act record their start
  (writes and runs) and their end as `succeeded`, `failed` or `cancelled`,
  with a typed failure.
- **AC3, privacy:** only enums, counts, a duration and an exit code reach a
  record or an export: no tour text, comment body, path, repository or login.
- **AC4, layering:** emitted from the host or application layer, not from
  widgets; composition digests resealed.
- **AC5, gates:** clang-format 22, focused tests, documentation gate.

## Design

- **Three more families on the same line.** `core::operation_diagnosis`
  gains the families `mcp_helper` (`agent.mcp_helper`, category `agent`),
  `tour` (`tour.operation`, category `tour`) and `cycle_round` (`cycle.round`,
  category `cycle`), their actions and their own failures, all appended after
  [[BPR-1829]]'s and [[BPR-1831]]'s; `corrupt`, `busy`, `invalid_bundle` and `schema_mismatch` are shared with them. A
  helper run, like an agent setup check, names its process outcome and exit
  code; tours and rounds name neither, and no new family sets the write
  flags. The logger's admission check and the export need no new rule: they
  already parse the line and match its family to its operation.
- **The MCP helper records from its supervisor, not from itself.** The helper
  executable has no diagnostics sink: it links only the core and its JSON
  decoder, takes no argument, keeps a two-name environment and has no
  filesystem root beyond its frozen snapshot. A log sink would need the
  profile's directory, a writer and the logger, and would break the one-line
  link claim its CMake states. So the application's
  `mcp_context_helper_supervisor`, which owns the helper's whole lifetime,
  records each request: a refusal (`no_helper`, `not_pinned`,
  `network_location`, `already_running`), or a start and an end with the
  child's process outcome, exit code, duration and answered line count. A
  helper an external agent starts on its own is outside the application and
  is not recorded.
- **Tours.** `review_tour_host`, which the desktop and `bpr tour open` both
  import through, records the offered tours' count and each import's end
  (`not_offered`, `draft_exists`, `target_mismatch`, `refused`, `busy`, or
  success counting the offered stops). `tour_runtime` records `bpr tour`
  `open`, `source`, `counts` and `preview` ends and `publish`'s start and end.
  A failure is named from the command's stable error code where that names a
  class (authentication, transport, incomplete remote, storage) and otherwise
  from its exit status; the message is never read. The app's own tour
  publication goes through review submission and keeps its
  `github.review_submission` records.
- **Rounds.** `review_cycle_runtime` is the one runtime the desktop window,
  `bpr cycle` and the owner's forwarded commands all use. Its freeze, grant,
  revoke, granted read, granted response, response import, export, bundle
  import and Send to Agent record go through `submit_observed`: a write's
  start, then its end with the store's `cycle_store_failure`, a grant's
  refused decision (`not_granted`, `revoked`, `wrong_target`,
  `not_authorized`) or a response's refusal (`read_failed`,
  `malformed_response`, `refused`). A write the closing runtime refuses records
  its start and a cancellation.
- **Where the CLI writes.** A `bpr` command that holds the profile's runtime
  lease opens the profile's own log on its first record, as one more
  cooperating session (`terminal_diagnostic_log`), honouring the profile's
  diagnostic settings; the desktop's export reads every session file, so it
  carries these. A command that forwards to a running desktop opens none; the
  desktop records what it runs (`cycle_owner_service::set_diagnostics`). The
  logging adapter is linked into the tour runtime only where the graph builds
  it; the standalone `tour-cli` preset builds none and records nothing.
- **Volume.** A few records per round or tour command, far inside
  [[BPR-1722]]'s 16,384-record budget. Each lease-holding command that
  records adds one session file, which retention (64 files, 50 MB, 30 days)
  ages out with the rest.

## Coverage inventory

Before is main at 53393c95a ([[BPR-1828]], [[BPR-1829]], [[BPR-1831]]); after is this card.

| Operation | I/O | Before | After |
|---|---|---|---|
| MCP helper request (application-started) | process | none | `agent.mcp_helper` refusal, or start and end with process, exit code and answered lines |
| MCP helper started by an external agent | process | none | none (outside the application; see above) |
| Tours offered for import | GitHub read | `github.pull_request` | also `tour.operation` `tour_observe_imports` count |
| Tour import (desktop and `bpr tour open`) | files | none | `tour.operation` `tour_import` end |
| `bpr tour open`, `source`, `counts`, `preview` | GitHub, files | none | `tour.operation` end |
| `bpr tour publish` | GitHub write | none | `tour.operation` start and end |
| Tour publication in the app | GitHub write | `github.review_submission` | unchanged |
| `bpr tour connect` (sign-in) | GitHub | none | none ([[BPR-1829]]'s sign-in) |
| Review Cycle freeze | SQLite | none | `cycle.round` start and end |
| Round grant and revoke | SQLite | none | `cycle.round` start and end |
| Granted round read (`round read`, `round page`, `round source`) | SQLite | none | `cycle.round` `cycle_round_read` end |
| Granted response (`response append`) and response import | files, SQLite | none | `cycle.round` start and end |
| Round export (rendered or to a file) | files, SQLite | none | `cycle.round` end, or start and end to a file |
| Round bundle import | files, SQLite | none | `cycle.round` start and end |
| Send to Agent's recorded delivery and response | SQLite | none | `cycle.round` start and end |
| Review Cycle Publish as Pull Request | Git, GitHub | none | none |
| Other cycle commands (open, capture, note, review draft, compare, policy, rebind, list, show) | files, SQLite | none | none |

## Left uncovered

Review Cycle's Publish as Pull Request (a Git and GitHub sequence of its own
whose steps carry branch and remote names), the remaining cycle commands
above, a `bpr cycle` refusal decided before the runtime is asked (a round not
yet frozen, an invalid option), and an MCP helper an external agent starts
outside the application. `bpr tour connect` is sign-in, which [[BPR-1829]]
covers. A standalone `tour-cli` build records nothing because it builds no
logging adapter.
