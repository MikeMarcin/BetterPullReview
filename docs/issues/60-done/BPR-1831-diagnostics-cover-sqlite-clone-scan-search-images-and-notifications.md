---
schema: "implement-rfc.card.v1"
id: "BPR-1831"
title: "Diagnostics cover the SQLite stores, clone scan, source search, images, avatars and notifications"
status: "Done"
rfcs: []
depends_on: ["BPR-1828", "BPR-1829"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1829--BPR-1831--37b8c8c13a6b.json","kind":"defect","found_by":"BPR-1828's coverage inventory: the SQLite stores, the linked clone scan, source search, body images, avatars and notifications left no diagnostic record","related_cards":["BPR-1722","BPR-1828","BPR-1829","BPR-1830"],"shared_category":"storage (storage.database), workspace (workspace.clone_scan), search (search.source), image (image.fetch), notification (notification.delivery)"}
acceptance: [{"criterion":"AC1: the coverage inventory is updated, before and after","outcome":"pass","revision":"2a013674d","evidence":{"kind":"observation","detail":"Inventory below; before is main 1ed5f00c9 (BPR-1829).","path":"docs/issues/60-done/BPR-1831-diagnostics-cover-sqlite-clone-scan-search-images-and-notifications.md"}},{"criterion":"AC2: store open, migration, read and write failures with the schema version; clone scans; searches; image and avatar fetches; notifications record their outcome with typed failures, high-frequency ones aggregated","outcome":"pass","revision":"2a013674d","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_operation_diagnosis|application_host_operation_diagnostics|application_source_search_host|sqlite_storage_diagnostics)$'","detail":"core: storage lines round-trip with store and schema; a trailer on any other family, a missing or unknown store, a reordered or zero-padded schema and a write flag on storage are refused; every new family's line reads back. host: the aggregate records a group's first end at once, holds ends inside the window and then records one line counting them with the longest duration, flushes at destruction; avatar and image fetches record success, too_large and a cancellation and pass bytes and failures through unchanged, 50 avatars collapse to one count=50 line; the notifier records the permission answer, a submission, an unattributable activation, a refusal and an activation, and nothing for a quiet bucket; the clone host records its store read, a scan's start and end (count=2), a throwing scan as scan_failed, not_a_clone, no_matching_remote, a serving link and an unreadable store. search: a search records at once, twenty keystrokes inside the window record nothing until a flush counts them (count=20), unavailable and a throwing execution fail at once. sqlite: a real markdown image store records storage_open and storage_migrate schema=1 under store markdown_images, five reopens are held and then counted with schema=1, a non-database diagnostic-preferences file records a failed open and a not_a_database statement under its store, a bare connection's bad statement records storage_statement sql_error store=unknown, and nothing is recorded once the scope ends."}},{"criterion":"AC3: only enums, counts, durations and schema versions reach the log and the export; private strings never do","outcome":"pass","revision":"2a013674d","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_release_maintenance)$'","detail":"Canaries in folders, remotes with a token, repository names, image addresses, hosts, logins, notification titles and bodies, search queries, a scratch root and image keys never reach a record (host, search and sqlite tests). quill_logging: the real logger writes the five operations under categories storage, workspace, search, image and notification with the exact line; it drops a storage line on another operation, one with a trailing word after its schema, a success carrying a failure and a bare event; the metadata preview classifies operations 15 and 19 without their lines. Control: with storage removed from the logger's admission list quill_logging fails at the trailing-word check. application_release_maintenance test_host_operation_export: each line exports exactly under both policies; one with a path, an unknown store, a query or an unknown failure keeps its record with the detail omitted and no PRIVATE text; a storage record filed under category github is refused."}},{"criterion":"AC4: emitted from the host and application layers and composed; digests resealed","outcome":"pass","revision":"2a013674d","evidence":{"kind":"command","command":"ninja -C build/dev app_better_pull_review; ctest --test-dir build/dev -R '^(application_review_round_trip_composition_contract|architecture_boundaries|core_boundary_contract)$'","detail":"The SQLite adapter (a host capability) records through storage_diagnostics_scope; clone_link_host, source_search_host, review_queue_notifier and diagnosed_image_fetch record in the application layer; no widget or Qt model emits. github_application.cpp opens the scope and the aggregate after the profile log, wraps the image fetch port, hands the log to the clone host, PR search and notifier, and flushes before the shutdown record. Resealed over BPR-1829: full application digest ed9214b8 and production app manifest digest 92e37b21; the mutation-branch pin is unchanged. The real composition round trip passes on the merged tree; architecture_boundaries and core_boundary_contract pass."}},{"criterion":"AC5: gates","outcome":"pass","revision":"2a013674d","evidence":{"kind":"command","command":"ninja -C build/dev coding-format-check; python3 scripts/verify_documentation.py --root .","detail":"coding-format-check passes with clang-format 22.1.8 over 1,881 files; DOCUMENTATION_OK. sqlite_ contract tests (30) pass with the instrumented connection policy. test_network_isolation_manifest fails only on the two rows BPR-1828 recorded (application_agent_chat_custom_agent_contract, gh_cli_git_smart_transport); the new sqlite_storage_diagnostics row is registered unmeasured (macOS)."}}]
---

# BPR-1831: Diagnostics cover the SQLite stores, clone scan, source search, images, avatars and notifications

[[BPR-1828]]'s coverage inventory listed these as the last host operations
with no diagnostic record. A broken store, a clone that is never found, a
search that silently fails, images that never load and notifications that
never appear all left an export with nothing to say.

## Acceptance

- **AC1, inventory:** the coverage inventory is updated, before and after.
- **AC2, events:** store opens (with the schema version), migrations and
  failed reads and writes; clone scans; searches; image and avatar fetches;
  notifications record `succeeded`, `failed` or `cancelled` (and a scan its
  start) with a typed failure. High-frequency operations do not flood the log.
- **AC3, privacy:** only enums, counts, durations and schema versions reach
  the log and the export; queries, paths, URLs, logins and content never do.
- **AC4, layering:** emitted from the host or application layer, composed in
  `src/app`; composition digests resealed.
- **AC5, gates:** clang-format 22, focused tests, documentation gate.

## Design

- **The same line, five more families.** `core::operation_diagnosis` gains
  `storage`, `clone_scan`, `source_search`, `image_fetch` and `notification`
  families, logged as `storage.database`, `workspace.clone_scan`,
  `search.source`, `image.fetch` and `notification.delivery`. A storage line
  alone appends `store=STORE schema=N`; the store is a closed enum told from
  the database file's fixed name (`review-ledger.sqlite3` is
  `review_ledger`, the clone ledger's `ledger.db` is `clone_ledger`, anything
  else `other`), so no path is ever formed into a line. Every other family's
  line is byte-for-byte what [[BPR-1828]] wrote, and the parser still refuses
  a trailer on them. The failure sets reuse [[BPR-1829]]'s `busy`, `locked`
  and `identity_exhausted`.
- **SQLite.** Thirty-odd stores open connections their own way, many per
  call. Three choke points in the adapter see all of them:
  `configure_connection` (every connection, after it opens: `storage_open`
  with the schema version read directly), `open_private_store_database` (a
  file refused before a connection: a failed `storage_open`), and
  `detail::execute` (a `PRAGMA user_version = N` that ran: `storage_migrate`).
  Failed reads and writes come from SQLite's own error log
  (`SQLITE_CONFIG_LOG`, installed once before `sqlite3_initialize`), whose
  result code is classified (`busy`, `locked`, `io_error`, `read_only`,
  `constraint`, `integrity_failure`, `not_a_database`, `cannot_open`,
  `sql_error`, `quota_exceeded`, `cancelled` for an interrupt; notices,
  warnings and retried schema changes are not failures). The callback may
  not call SQLite, so `detail::prepare` notes the store and whether the
  statement writes (`sqlite3_stmt_readonly`, with transaction control as a
  write) on the thread, and finalizing or failing to prepare clears it. A
  failure on a thread with no prepared statement is `storage_statement` of
  store `unknown`. The message, which carries SQL and paths, is never read.
  The adapter records only while `adapters::sqlite::storage_diagnostics_scope`
  lives; the profile opens it on its log. Without a scope a statement costs a
  relaxed atomic load and a thread-local write. A thread-local guard stops a
  log that touched SQLite from re-entering.
- **Flood control.** `application::aggregated_operation_log` groups lines by
  action, failure and store. The first line of a group records at once, so the
  first failure of any kind is always immediate. After that the group records
  at most one line per window; its `count` is how many ends it stands for and
  its duration the longest. The rest is flushed at shutdown, before the
  shutdown record. Storage uses ten minutes, since some stores open on every
  call and a broken store fails on every call (at most one line per store and
  outcome per ten minutes, well inside [[BPR-1722]]'s 16,384-record budget).
  Searches (one per keystroke) and image and avatar fetches (dozens per
  page) use one minute. A search records no start: a start per keystroke is
  the flood this avoids. Migrations, clone scans and notifications are a few
  per session and record directly. Records go through the logger's normal
  `record`, so its nonblocking admission and backpressure drop as before.
- **Where it is emitted.** The SQLite adapter (a host capability port);
  `clone_link_host` (scan start and end counting the clones found, a scan
  that threw as `scan_failed`, each remembered link's check as
  `not_a_clone`/`no_matching_remote`, the store read as `store_unreadable`);
  `source_search_host::observe_diagnostics` (each execution's terminal:
  complete, partial and result-limited succeed, cancelled is a cancellation,
  the rest fail as `unavailable`, `resource_limit`, `identity_exhausted` or
  `search_failed`); `application::diagnosed_image_fetch`, which wraps the
  profile's image fetch port and names a fetch `avatar_fetch` when its policy
  keeps it to avatar origins; `review_queue_notifier` (permission answers,
  submissions, `permission_refused`, `unattributable_activation`,
  activations; a quiet bucket records nothing). No widget emits.
- **Insights.** These failures are diagnostics only; the Insights problem
  counter skips the five operations so its numbers mean what they meant.

## Coverage inventory

Before is main at 1ed5f00c9 ([[BPR-1829]]); after is this card. Rows earlier
cards cover are unchanged and omitted.

| Operation | I/O | Before | After |
|---|---|---|---|
| SQLite store open, policy and schema version (every store) | SQLite | none | `storage.database` `storage_open`, aggregated |
| SQLite schema migration and store creation | SQLite | none | `storage_migrate` with the new version |
| SQLite failed reads, writes and statements | SQLite | none, except the submission reason kept in recovery | `storage_read`, `storage_write`, `storage_statement`, aggregated |
| Remembered sign-in metadata (device bindings, remembered connections) | SQLite | none ([[BPR-1829]] left it here) | as every store, `device_bindings`, `remembered_connections` |
| Clone folders and links read | SQLite | none | `workspace.clone_scan` `clone_load` |
| Linked clone scan | files | none | `clone_scan` start and end, counting clones |
| Remembered clone checked again | files | none | `clone_verify` |
| PR source search | files, Git cache | none | `search.source`, aggregated |
| Body image fetch | network | none | `image.fetch` `image_fetch`, aggregated |
| Avatar fetch | network | none | `image.fetch` `avatar_fetch`, aggregated |
| Queue notification, permission, activation | platform | none | `notification.delivery` |
| MCP helper, tours, Review Cycle | process, files, GitHub | none | none ([[BPR-1830]]) |

## Left uncovered

- In-pane find (`pane_find_controller`) uses the same search host but is not
  given the aggregate; only PR-wide source search records. Blob snippets and
  image decoding after a fetch (and the disk-cache probe) are not recorded.
- The storage scope is process-wide and the newest live scope receives; a
  process with several profile runtimes records every store to the newest
  profile's log. Stores opened before the profile's log exists (profile
  registry, diagnostic preferences at startup) record nothing.
- A statement's store and direction are the last statement the thread
  prepared. A store that keeps a statement prepared while preparing another
  can misattribute a failure to the newer one; a statement prepared outside
  `detail::prepare` (the epoch catalog and device bindings each have one) is
  attributed to whatever the thread prepared last, or to `unknown`.
- A schema version written inside a transaction that later rolls back still
  records its migration; the rollback's failure records separately.
- No fixture course was inspected for these records, and none asserts
  them; the unit, sqlite and logger tests and the passing
  real-composition round trip are the evidence.
