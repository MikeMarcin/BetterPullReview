---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AU"
title: "Merge the Home refresh diagnostics branch onto main"
status: "Done"
rfcs: []
depends_on: ["BPR-1680", "BPR-1717", "BPR-1722", "BPR-1725"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"integration","origin":"Owner decision, 2026-10-22, through the coordinator: \"No merge the whole worktree fixes.\"","source_branch":"codex/home-refresh-diagnostics (Codex session of 2026-10-19, worktree /Users/jaeger/Documents/Codex/2026-10-19/task-3/bpr-home-diagnostics, read only)","source_commits":["534fd62698d77033c7710ff8d7c5944f77caf4a3","f330342be27f37458ad510321bb652601375f3c1","64e73aefd6f922169f5163675705592f66e1fc1d"],"related_cards":["BPR-1540","BPR-1679","BPR-1680","BPR-1717","BPR-1722","BPR-1725"]}
acceptance: [{"criterion":"AC1: each of the three branch commits is brought onto main with cherry-pick -x and its own provenance line, resolved in favour of main's design, and the card says what was taken, merged or superseded","outcome":"pass","revision":"1b1745238b6fb9ae01e63cf35854147899dbda20","evidence":{"kind":"observation","detail":"f591e4901 (cherry picked from 534fd6269), f70b46de2 (from f330342be) and 1b1745238 (from 64e73aefd), rebased on local main c7d82b4ea; the table below lists each part of each commit."}},{"criterion":"AC2: the export dialog's Save is the button box's accept button and sits beside Cancel at the right in every platform's button order; the test fails before the change","outcome":"pass","revision":"1b1745238b6fb9ae01e63cf35854147899dbda20","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev -R '^qt_release_maintenance_scale_(100|125|200)$'","detail":"save_sits_beside_cancel checks Save's AcceptRole and Cancel's RejectRole, then forces MacLayout, WinLayout, KdeLayout and GnomeLayout on the box through a proxy style and checks that Save and Cancel are adjacent after the stretch, Save rightmost on macOS. With ActionRole put back it fails at 'Save accepts and Cancel rejects'; with that check also disabled it fails at 'Save and Cancel sit together at the right, after the stretch, in every platform's order'."}},{"criterion":"AC3: the branch's Home refresh timing value that main lacked is in main's github.discovery line, counts and enums only, and still exports anonymized; an earlier build's shorter line still reads","outcome":"pass","revision":"f591e4901026101a0f362f0ca00c394ba2a12094","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_home_discovery|application_review_queue_enrollment|github_provider|github_provider_content_policy|quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_release_maintenance|application_personal_discovery_public_path|application_personal_discovery_cli_public_path|application_home_waiting_public_path)$'","detail":"core_home_discovery round-trips the new fields, reads the BPR-1680 option 1 line, refuses a read line with a backoff, a part of the extension, a reordered field or a trailing word. github_provider counts 403, 429, 502 and timeout answers and times the transport and the background operation's start (fails without the counting: 'the read's time in the transport is part of its run'). application_review_queue_enrollment checks the sums on a refresh's end, the decision-lane wait (fails at 'an answer left waiting for the decision lane says so, not as provider time' without it) and the backoff on a rate-limited refresh (fails at 'a refresh a rate limit stopped ends with the backoff it leaves' without it). application_release_maintenance keeps the long lines under both policies and omits one with a token in it. Both discovery public paths read the composed profile's own diagnostics file and require every line in full."}},{"criterion":"AC4: the branch's current-session priority is in main's exporter without changing its newest-first selection, limits, zip, anonymisation or allow-list; the test fails before the change","outcome":"pass","revision":"f70b46de2f8b91dc5ab98bc74e29ebb29be8cd07","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(application_release_maintenance|core_release_maintenance|sqlite_release_maintenance_files|application_release_maintenance_public_path|application_diagnostic_controls_public_path|diagnostic_controls|application_logging|quill_logging)$'","detail":"test_export_keeps_running_session_through_clock_rollback: an older session of 16,584 records with a later clock and a running session of three records whose clock is behind and set back once. Every running-session record is kept, last, in sequence order, and the limit drops the older session's oldest. With the priority removed it fails at 'every record of the running session is kept although its clock is behind'. BPR-1717's test_export_keeps_newest and the rest of the suite pass unchanged."}},{"criterion":"AC5: gates","outcome":"pass","revision":"1b1745238b6fb9ae01e63cf35854147899dbda20","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <12 changed translation units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"clang-format 22.1.8 and clang-tidy 22 with warnings as errors pass. The command-correctness and round-trip composition reseal checks report nothing to reseal and no pinned-scope commit; no src/app file changed. architecture_boundaries and command_correctness_registry_contract pass."}}]
---
# BPR-PENDING-AU: Merge the Home refresh diagnostics branch onto main

## Owner decision

A Codex session on 2026-10-19 left three commits on
`codex/home-refresh-diagnostics` that never reached main. Asked whether only
its Save button change should come over, the owner answered on 2026-10-22,
through the coordinator: "No merge the whole worktree fixes." This card brings
all three across. Main had meanwhile done overlapping work in [[BPR-1680]]
(option 1, the `github.discovery` refresh and read lines), [[BPR-1717]]
(newest records kept, files read by modification time), [[BPR-1722]] (16,384
records and 16 MiB, review submission endings, a dated file name) and
[[BPR-1725]] (the zip). Where both did the same thing, main's version is kept.
The branch and its worktree were read, not changed.

## Per commit

| Branch commit | On main | Taken | Merged into main's design | Superseded by main |
|---|---|---|---|---|
| `534fd6269` Add privacy-safe Home refresh timing diagnostics | `f591e4901` | The measurements main lacked | As fields of main's `github.discovery` line | The parallel `github.home_refresh` operation and its parser |
| `f330342be` Prioritize current diagnostic evidence under export caps | `f70b46de2` | The running session's priority | Into main's `select()` ordering | Its rewritten selection, output order, reserve and test target |
| `64e73aefd` Group diagnostic Save beside Cancel | `1b1745238` | All of it, unchanged | None | None |

Each main commit carries `(cherry picked from commit …)` for its source.

### 534fd6269: Home refresh timings

The branch added a second Home diagnostic: a `github.home_refresh` operation
with a 46-field `home_refresh=1` line written by the scheduler and by a
provider-side observer, with its own logger check, export allow-list entry and
Insights exclusion. Main's [[BPR-1680]] option 1 already logs every refresh
and read as one `core::discovery_diagnosis` line. Taking both would log every
read twice, so the operation is not taken.

**Superseded.** The `github.home_refresh` operation, `home_refresh_diagnostic`,
its grammar and test, the provider's observer and the per-request
`request_finished` hook. The logger check and allow-list entry are main's
(`parse_discovery_diagnosis`). Main counts a failed discovery read as an inbox
problem. The branch excluded its records from Insights, which does not apply
here. Main's numbered refreshes and reads also stand in for the branch's
cycle and job ordinals. Both keep host, repository, account and pull request
out of the line. The branch's `DiagnosticGeneration` on the request is not
needed: main's lines are standard records written straight to the log, not
recent context.

**Taken, as fields of main's line** (`credential_ms wire_ms decision_ms
http_403 http_429 http_5xx timeouts backoff_s`, after `run_ms`):

- the credential time (`credential_us`): the account operation's start, timed
  in the provider around `begin_operation` or `begin_background_operation`;
- the wire time (`wire_us`): summed in main's `counting_wire_transport`, so
  the rest of the run is local work;
- the completion's wait for the decision lane (`decision_queue_us`), timed on
  the steady clock from the provider's callback to the lane's turn;
- the HTTP 403, 429 and 5xx and timeout counters, counted from each wire
  answer, optional reads' included;
- the refresh's `backoff_s`.

A refresh's end sums the read fields. `personal_discovery_cost` carries the
provider's measurements; the host adds the lane wait. The parser still reads
the line [[BPR-1680]] wrote, which ends at `run_ms`, so earlier retained logs
keep exporting.

**Not taken.** `host_queue_us` and `effect_queue_us`: main's `wait_ms`
already measures the wait for the provider lane, and a job queued in the host
is not yet a read. `decode_us`: the rest of `run_ms` after `wire_ms`. Also
`max_wire_us`, `continuations`, `blocked`, `failures`, the rate-limit resource
split and `retry_after_s`. A failed read's failure, rate limits included, is
already on its line's failure field, and `backoff_s` carries the wait. The
eight per-stage totals and call counts: main's options 3 and 4 replaced the
stages they name with shared searches and GraphQL batches. `apply_us`: the
read's end is logged before it is applied, so the refresh's end still follows
its last read. `job_attempts` and `job_retries` are always 1 and 0 under the
current policy. The transport kind is a property of the account, not of a
read.

### f330342be: export selection

The branch rewrote `prepare_diagnostic_export` to choose, under the old
1,024-record and 256 KiB caps, a priority order: the current session by
descending sequence, then other sessions by descending UTC, then legacy
lines. Its output was in that same order. [[BPR-1717]] and [[BPR-1722]] since
decided the selection and the limits on main.

**Superseded.** Newest-first output, the 2 KiB reserve and its header check,
the per-record bound of three new labels, the lexical winner among
conflicting duplicates, and the `application_diagnostic_export_selection`
target. Main writes the newest run oldest first under a 4 KiB reserve. It
numbers labels in output order after measuring each record at its widest.
File copies win because files are read before recent context, and the
reader's order is deterministic. The selected-records-only header accounting
and the complete-prefix cut were already main's.

**Taken.** The current-session priority. Main ordered every session by its
wall clock. A running session whose clock is behind an older session's, or
was set back, could have its latest records left out. Now the running session
(`aCapture.Session`) sorts after every other session and orders by sequence.
The kept run, the limits, the header, the zip, anonymisation and the
allow-list are unchanged, and in the ordinary case, where the running
session's clock is the latest, the output is byte for byte what it was.

### 64e73aefd: Save beside Cancel

Taken as is. Save joins the export dialog's button box with `AcceptRole`
instead of `ActionRole`. macOS's button order puts action buttons at the
left, away from Cancel, and the accept button at the right beside it. The
accept signal is connected to nothing, so the click still only triggers
Save's action. The commit gained the test described in AC2.

## Evidence

macOS (Apple M4 Pro), 2026-10-22, `dev` configured with `-g0` and the main
checkout's CPM cache, focused targets only, Qt offscreen. Rebased on local
main `c7d82b4ea`: the 23 suites in AC2 to AC4 pass, with
`architecture_boundaries` and `command_correctness_registry_contract`. Each
control in AC2 to AC4 changed one part back, rebuilt, ran and was restored.

The discovery public path's composed profile now prints, for example, its
explicit refresh as `requests=6 wait_ms=402 run_ms=832 credential_ms=0
wire_ms=812 decision_ms=13`, and the CLI variant's two concurrent reads as
`run_ms=434 wire_ms=819`. That is why the line's spans overlap and are not
added together.

`application_review_round_trip_composition_contract` could not run: its
round-trip binary was not built in this lean tree. Its three pins were checked
with the reseal script's print mode and are unchanged. No file it covers
changed.
