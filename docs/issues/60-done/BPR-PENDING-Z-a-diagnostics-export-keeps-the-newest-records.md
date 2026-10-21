---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-Z"
title: "A diagnostics export keeps the newest records"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report, 2026-10-21: an export taken after hours in the app held nothing from that session; the owner's two exports were read as data and are not committed","related_cards":["BPR-1216","BPR-1218","BPR-1540","BPR-1680"]}
acceptance: [{"criterion":"AC1: the card names why the running session was missing","outcome":"pass","revision":"5e8d4cbe4","evidence":{"kind":"observation","detail":"Both of the owner's exports hold 316 records in 253,046 bytes, which is the 256 KiB output limit less its reserve. Releases up to 0.3.0 read the log files in name order (session-<random identity>-<part>.jsonl), and the exporter kept the first records it read. The running session's file sorted after older sessions' files, so all of its records fell into omitted_records (2,749 in the second export). That count did not separate allow-list omissions from over-limit omissions. The flush before reading works, and the allow-list admits review submission and discovery records.","path":"docs/issues/60-done/BPR-PENDING-Z-a-diagnostics-export-keeps-the-newest-records.md"}},{"criterion":"AC2: an export taken during a live session contains its records up to the moment of export","outcome":"pass","revision":"5e8d4cbe4","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^quill_logging$'","detail":"live_session_export runs an older real Quill session of 700 records, renames its file to the lowest possible name and makes it an hour old. A second session stays open, records 30 records and one review submission left unknown, and is neither closed nor flushed by the test. A real release_maintenance_files preview, with that live logger as its control port, holds all 30 records and the review submission. It holds no session identity, counts the older session's oldest records as left out, and keeps that session's newest records. On the old exporter and reader it fails at 'every live record is in the export'."}},{"criterion":"AC3: over its limits an export keeps the newest records and says what it left out and why","outcome":"pass","revision":"5e8d4cbe4","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(application_release_maintenance|sqlite_release_maintenance_files)$'","detail":"test_export_keeps_newest feeds 1,200 older-session records ahead of three current-session records. The current session is in the export and its review submission keeps its reason line. The kept older records are exactly the newest, records run oldest first and the byte limit holds. The header reads unclassified_records 0, limit_omitted_records N, limit_keeps newest, unread_files 0. The metadata export keeps dropped=3 to dropped=1026 of 1,027. test_export_reads_newest_files retains 17 MiB of logs: it reads the newest file, keeps its newest 1,024 records and counts the older file unread. On the old code these fail at 'the current session's records are in the export', at the metadata check run alone, and at 'logs over the input limit still export'."}},{"criterion":"AC4: review submission records appear in an export, anonymized","outcome":"pass","revision":"5e8d4cbe4","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(quill_logging|application_release_maintenance)$'","detail":"A review_submission.failed record keeps its op, its failure_domain review_submission, its reason and its counts-only line. No session identity reaches the export. These checks follow the current-session checks, which fail first on the old code. No allow-list entry changed."}},{"criterion":"AC5: gates","outcome":"pass","revision":"5e8d4cbe4","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <7 changed translation units>; ctest --preset core-only; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22.1.8 and clang-tidy 22 pass. architecture_boundaries, architecture_boundary_contract, core_release_maintenance, qt_release_maintenance_scale_100/125/200, application_release_maintenance_public_path, application_diagnostic_controls_public_path, diagnostic_controls and quill_logging_content_policy/backpressure pass. core-only passes 310 of 313; the three documentation tests need the .agents link this worktree lacks. Both reseal checks report nothing to reseal (no src/app change). verify_documentation_tests passes 67. With a temporary .agents link, the documentation gate reports only the card's provisional id format and the issue counter: NEXT_ID BPR-1714 is behind the shared high water BPR-1715 with or without this card, which allocation settles."}}]
---

# BPR-PENDING-Z: A diagnostics export keeps the newest records

The owner worked in the app for hours, exported diagnostics, and found nothing
from that session: "My log is having nothing from today's session."

## Acceptance

- **AC1, cause:** the card names why the running session was missing, against
  the shipped code.
- **AC2, live export:** an export taken while a session runs holds that
  session's records up to the moment of export.
- **AC3, limits:** over its limits an export keeps the newest records, and
  says what it left out and why.
- **AC4, review submissions:** review submission records reach an export,
  anonymized. If they were being dropped, find out whether they are logged at
  all.
- **AC5, gates:** each new test fails before the change; focused suites,
  core-only, clang-format 22, clang-tidy on the changed sources and the
  documentation gate pass.

## Cause

Both exports hold 316 records, and the newest is a 40-second launch from the
evening before. The second export's header has `recent_records: 0` and
`omitted_records: 2749`. The 316 is not a record cap: it is the byte limit.
At about 800 bytes a record, 316 records fill the 256 KiB output limit less
its reserve. The file is 253,046 bytes.

Two shipped behaviours combine to lose the running session:

1. **The reader's order is random.** `read_diagnostic_files` in
   `release_maintenance_files.cpp` sorted the log files by path. A session
   file is `session-<random session identity>-<part>.jsonl`, so path order
   says nothing about age.
2. **The exporter kept the oldest it read.** `prepare_diagnostic_export`
   rendered records in input order and, once full, counted every later record
   as omitted. Recent context came after the files, so it was dropped too.

The running session's file sorted after older sessions' files, so none of its
records survived. `omitted_records` mixed records the allow-list refused with
records the limit dropped, so the header could not tell them apart. The
"2,319 dropped by the allow-list" reading of the earlier export was in fact
mostly the limit.

The other candidates were checked against the code and ruled out:

- **Flush.** `flush_for_export` takes the logger mutex and blocks on
  `flush_log()` for both Quill loggers. The sink writes with native calls and
  keeps no userspace buffer. Records accepted before the export are on disk
  when it reads.
- **Active file skipped.** It is not: preparation reads the active file with
  the rest. A file that does not end in a newline loses only its unfinished
  last line.
- **Another directory.** The logger and the maintenance files share the
  profile's `ApplicationDataRoot`, and the control port forwards through the
  Insights wrapper.
- **Allow-list.** It admits `github.review_submission` and `github.discovery`
  with their closed reason lines (BPR-1540, BPR-1680).
- **Clock.** Timestamps are only reported, never used for selection, so no
  clock or ordering bug was involved.

A second loss sat on the same path. Retention keeps up to 50 MiB, but the
export reads at most 16 MiB. A file that overran the remaining input refused
the whole export (`limit_exceeded`).

## Change

The change stays in the application exporter, the SQLite maintenance reader,
the core snapshot and one line of the Qt panel. Logging, the allow-list and
anonymization are unchanged.

- **Reader.** Files are taken newest first by modification time while they
  fit the 16 MiB input. Once one does not fit, it and every older file are
  left unread and counted rather than refusing the export. The newest file is
  read first, so a running session's file is never refused for having grown
  since the scan. The files are joined oldest first.
- **Rich export.** Admitted records are ordered by time, then session and
  sequence; the duplicate check makes that order total. Version-one lines
  carry no time and count as the oldest. The newest run that fits 1,024
  records and 256 KiB less a 4 KiB reserve is kept and written oldest first.
  A record's size is measured with labels at their widest, so the final
  render cannot overrun the limit. Labels are then numbered in output order.
- **Metadata export.** It keeps its newest 1,024 records the same way.
- **Header and snapshot.** The header adds `unclassified_records`,
  `limit_omitted_records`, `limit_keeps: "newest"` and `unread_files`, and
  `omitted_records` stays their sum. The notice says the oldest are left out.
  `release_diagnostic_snapshot` carries `LimitOmittedRecords` (validated not to
  exceed `OmittedRecords`) and `UnreadFiles`.
- **Panel.** When something was left out, the preview summary adds one line:
  "Kept the newest records. N older records were left out to fit the export
  size limit." It also names the older log files when any were unread. The
  `frontend-design` skill was loaded for this copy.

[`release-maintenance.md`](../../architecture/release-maintenance.md)
describes the ordering and the new counts.

## Review submissions

The allow-list was not dropping review submissions, so nothing was added to
it. The investigation found none in the export because of the cap above.
Today the app logs a submission only when its outcome is unknown (BPR-1540).
That is `github.review_submission.failed`, with the reason and counts. A
published or definitely unpublished review writes nothing, and the BPR-1540
test pins that ("nothing is named or logged"). Recovery writes nothing to the
log either. Records of either outcome would already export: the operation is
admitted with any outcome, and a record with no detail needs no further
vocabulary. Logging them belongs to the publish-fix work in
`review_submission_session.cpp`, which this card leaves untouched.

## Limits that remain

- A retained-sessions export holds about the newest 300 records, because
  256 KiB fills before 1,024 records. An hours-long session that writes more
  than that is cut to its latest part. Raising
  `release_diagnostic_byte_limit` is a separate decision.
- There is no way to pick one past session. "Current session only" selects
  the running one, and retained sessions give the newest records across all
  of them.

## Evidence

Implementation `5e8d4cbe4`, on main `ad6908848`; macOS (Apple M4 Pro),
2026-10-21, `dev` and `core-only` built with `-O0 -g0` to save disk.

- **Before.** The new tests were built against the old exporter and reader,
  with the new header and snapshot fields so they compile:
  - `quill_logging` fails at "every live record is in the export".
  - `application_release_maintenance` fails at "the current session's records
    are in the export". The metadata check, run first on its own, fails too.
  - `sqlite_release_maintenance_files` fails at "logs over the input limit
    still export".
- **After.** These pass: `quill_logging`, `quill_logging_content_policy`,
  `quill_logging_backpressure`, `application_release_maintenance`,
  `sqlite_release_maintenance_files`, `core_release_maintenance`,
  `qt_release_maintenance_scale_100/125/200`,
  `application_release_maintenance_public_path`,
  `application_diagnostic_controls_public_path`, `diagnostic_controls`,
  `architecture_boundaries` and `architecture_boundary_contract`.
- **Gates.** `coding-format-check` (clang-format 22.1.8) and clang-tidy 22
  `--warnings-as-errors` on the seven changed translation units pass. Both
  reseal checks report nothing to reseal, since no `src/app` file changed.
- **core-only.** 310 of 313 pass. `documentation_structure`,
  `documentation_issue_graph_contract` and `issue_id_allocation_contract`
  cannot import `board_model`, because the worktree has no `.agents` link.
- **Documentation.** `verify_documentation_tests.py` passes 67 tests. With a
  temporary `.agents` link, removed again after the run,
  `verify_documentation.py` reports two things. One is the card's provisional
  ID format. The other is the issue counter: `NEXT_ID` BPR-1714 is behind the
  shared high water BPR-1715, with or without this card. Under a numeric
  stand-in ID, tried and reverted, it reports only the counter.
