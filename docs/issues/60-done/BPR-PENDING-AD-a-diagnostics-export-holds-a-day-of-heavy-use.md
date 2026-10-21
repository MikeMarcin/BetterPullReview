---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AD"
title: "A diagnostics export holds a day of heavy use"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1717"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner, 2026-10-21, through the coordinator: \"You can raise the diagnostic report limit to whatever we need to to make it actually useful to diagnose things\"; and: \"It would also be nice if the diagnostic log included like a date or timestamp and file name so I didn't have to keep renaming them.\"","related_cards":["BPR-1540","BPR-1680","BPR-1715","BPR-1717"],"compression":"not added: the pinned zlib compiles only inflate and checksum sources and no other compressor is approved"}
acceptance: [{"criterion":"AC1: the limits are chosen from measured record sizes and rates","outcome":"pass","revision":"685846069","evidence":{"kind":"observation","detail":"The Home discovery and logging fixture courses' own diagnostics files (263 and 47 records) give 706 to 867 bytes a record, 795 and 783 on average; the owner's two exports averaged 801. A session start is about 15 records, a pull request opened about 30 to 45 (13 page responses and 15 epoch records for the fixture's pull request), a Home refresh about 5 to 10 (seven refreshes wrote 37 discovery records). Heavy use comes to about 1,000 records an hour. Output: 16,384 records and 16 MiB (about 16 hours heavy). Input: 32 MiB and 131,072 lines, so at least 27 MiB (about 35,000 records) is read whenever logs hold that much, more than the output keeps; retention's 50 MiB is about 66,000 records.","path":"docs/issues/60-done/BPR-PENDING-AD-a-diagnostics-export-holds-a-day-of-heavy-use.md"}},{"criterion":"AC2: an export of an eight-hour heavy session keeps all of it","outcome":"pass","revision":"685846069","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(application_release_maintenance|quill_logging)$'","detail":"test_export_keeps_long_session feeds 8,000 records 3.6 s apart (7.4 MB), with a Home refresh every 25 records and a refused review every 500. All 8,000 are kept, limit_omitted_records is 0, the time range is the session's, the refresh and ending lines survive and no session identity does. quill_logging's live export now keeps both real sessions whole. With the old limits restored both fail: at 'every record of an eight-hour session is kept' and 'both sessions fit the export whole'."}},{"criterion":"AC3: the bounds and BPR-1717's guarantees hold","outcome":"pass","revision":"685846069","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(application_release_maintenance|sqlite_release_maintenance_files|core_release_maintenance|qt_release_maintenance_scale_(100|125|200)|application_release_maintenance_public_path)$'","detail":"test_export_bounds: 16,394 records keep the newest 16,384 and count 10; about 3,700 records with 4 KB classified details (17 MiB) stop at the 16 MiB byte limit keeping the newest; input one byte or one line over its limit is refused. test_export_keeps_newest, the metadata export and test_export_reads_newest_files run at the new limits (the files sized from the input limit). Current-session scope, unread_files, the allow-list and anonymisation tests are unchanged and pass. The suite runs in 3.6 s at -O0; an offscreen preview of a 16,384-record (13 MiB) document took 31 ms."}},{"criterion":"AC4: review submission outcomes are logged, anonymized","outcome":"pass","revision":"685846069","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_review_submission|live_review_submission_provider|quill_logging|application_release_maintenance)$'","detail":"A run whose outcome is known logs one review_submission record with the POST's HTTP status and review_submission_ending=ENDING run=RUN post=ANSWER post_class=N refusal=REFUSAL stop=STOP comments=N duplicate=0|1. The provider rows: every confirmed review logs published with its run and three comments; the GitHub CLI's 422 logs refused, unplaceable_line, http_status 422, comments 2; Check status settling a migrated attempt logs settled with no_answer; one settled after the delay logs settled, one still unknown logs its reason. The logger drops a forged line, a record with both an ending and a reason, another operation or an invalid ending. The export keeps the exact line under both policies and omits one with a host or repository in it. Controls: without the session's call the provider row fails at 'one ending is logged'; without the logger check quill_logging fails at 'an ending whose detail is anything but its own line never enters the log'; without the allow-list entry application_release_maintenance fails at 'keeps exactly its counts-only line'."}},{"criterion":"AC5: a saved export is named for when it was taken and its header says so","outcome":"pass","revision":"685846069","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_release_maintenance|qt_release_maintenance_scale_100|application_release_maintenance|application_release_maintenance_public_path)$'","detail":"core::diagnostic_export_file_name gives BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt: zero padded, sortable, no colon or other refused character, the version only when it is dotted numbers, an impossible time left out. The save dialog suggests it for the local minute and QCoreApplication's version. The header begins exported_at 2026-10-21T14:32:05-07:00, app_version 0.3.1, app_build; unknown without a stamp or a well-formed build. Controls: the old fixed name fails the panel at 'the suggestion holds the local date, time and version'; a header without the time fails at 'the header names the local export time'."}},{"criterion":"AC6: gates","outcome":"pass","revision":"685846069","evidence":{"kind":"command","command":"cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <20 changed translation units>; ctest --test-dir build/dev -R <focused>; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22.1.8 and clang-tidy 22 pass. Passing: core_release_maintenance, core_review_submission, application_release_maintenance, sqlite_release_maintenance_files, qt_release_maintenance_scale_100/125/200, live_review_submission_provider, live_review_submission_restart_recovery, application_review_submission_composition_public_path, application_release_maintenance_public_path, application_diagnostic_controls_public_path, application_insights_host, application_insights_public_path, application_logging, application_personal_discovery_public_path, diagnostic_controls, quill_logging, quill_logging_content_policy, quill_logging_backpressure, architecture_boundaries, architecture_boundary_contract, core_boundary_contract. Both reseal checks report nothing to reseal (no src/app change). verify_documentation_tests passes 67. With a temporary .agents link, removed after the run, verify_documentation reports only the card's provisional ID format and the issue counter (NEXT_ID BPR-1721 behind the shared high water BPR-1722, set by other cards), which allocation settles."}}]
---

# BPR-PENDING-AD: A diagnostics export holds a day of heavy use

The owner, 2026-10-21, through the coordinator: "You can raise the diagnostic
report limit to whatever we need to to make it actually useful to diagnose
things." Later the same day: "It would also be nice if the diagnostic log
included like a date or timestamp and file name so I didn't have to keep
renaming them."

[[BPR-1717]] made an export keep its newest records, but kept the limits of
1,024 records and 256 KiB, which hold about 300 records. Both of the owner's
exports from an hours-long session held 316.

## Acceptance

- **AC1, limits from evidence:** the record and byte limits come from
  measured record sizes and rates, and the card says how.
- **AC2, a day fits:** an export of an eight-hour heavy session keeps all
  of it.
- **AC3, bounds:** the limits hold, and newest-first selection, the header
  counts, anonymisation, the allow-list and "Current session only" work as
  before.
- **AC4, what is logged:** the records that would have shown the owner's two
  problems of the day are logged and exported, anonymized. A review GitHub
  refused ([[BPR-1715]]) and a slow Home refresh ([[BPR-1680]]).
- **AC5, name and header:** the save dialog suggests a name with the local
  date and time, and the header carries the export time and app version.
- **AC6, gates:** each new test fails before the change; focused suites,
  clang-format 22, clang-tidy and the documentation gate pass.

## Measurements

Two fixture courses were run with their profile's diagnostics file copied
out by a temporary, uncommitted hook in the test support's log reader:
`application_personal_discovery_public_path` (263 records, three sessions)
and `application_logging` (47 records, one session).

| | Discovery course | Logging course |
|---|---|---|
| Bytes a record (min, mean, max) | 706, 795, 867 | 707, 783, 837 |
| Pull request loads | 4 | 1 |
| `github.pull_request` records | 57 | 15 |
| `epoch` records | 118 | 15 |
| Session start (application, viewer, capability) | about 15 a session | 15 |
| Home refreshes ended, discovery records | 7, 37 | none |

An exported record is about the size of the record it comes from; the
owner's two exports averaged 801 bytes.

From those:

- **A session start** writes about 15 records: application, two viewer
  records and eleven capability records (nine responses).
- **A pull request opened** writes about 30 to 45: its started and finished
  records, one per page response (13 for the fixture's pull request; more on
  a larger one) and 15 to 30 epoch records.
- **A Home refresh** writes about 5 to 10 at standard detail: its start, one
  record per read's end and its end (a read's start is verbose). Home
  refreshes on its own at most every five minutes
  (`personal_discovery_freshness_seconds`), so about 12 an hour, plus
  explicit refreshes. A real server's refresh has more reads than the
  fixture's.
- **A review submission** writes one record per Submit or Check status, and
  each comment a few.

Heavy use is taken as 15 pull requests opened and 20 Home refreshes an hour,
with comments and a few submissions: 15 x 45 + 20 x 10 + about 100, roughly
1,000 records an hour, or 8,000 for an eight-hour day.

## Chosen limits

| | Before | Now | Why |
|---|---|---|---|
| Output records | 1,024 | 16,384 | About 16 hours of heavy use, two ordinary days |
| Output bytes | 256 KiB | 16 MiB | 16,384 records at 1 KiB each, wider than any measured |
| Input bytes | 16 MiB | 32 MiB | At least 27 MiB read with 5 MiB files: about 35,000 records, more than the output keeps |
| Input lines | 65,536 | 131,072 | The same one line per 256 input bytes |

- **Records over bytes.** At measured sizes the record limit binds first, at
  about 13 MiB. Records with long classified details (GraphQL error lists up
  to 4 KB) stop at the byte limit first, and the newest are still kept.
- **How far back.** Files are read whole, newest first, while they fit, and
  a session file is at most 5 MiB, so a read covers at least 27 MiB whenever
  the logs hold that much. That is more than 16,384 records, so the output
  limit decides how far back an export reaches. Retention keeps up to
  50 MiB, about 66,000 records or nearly three heavy days; reading all of it
  would only feed records the output drops, at more memory.
- **Memory and time.** The reader now releases each file once it is joined,
  so it holds about one copy of its input: at most 32 MiB of input, 16 MiB of
  output and an index of the admitted records. The
  `application_release_maintenance` suite, which prepares an 8,000-record, a
  16,394-record and a 17 MiB export, runs in 3.6 s in an unoptimised build.
  A 16,384-record (13 MiB) document took 31 ms to show in the preview's
  `QPlainTextEdit` offscreen.
- **The file.** At most 16 MiB of text, inside GitHub's 25 MB attachment
  limit. Exports compress well: the owner's 256 KiB exports zip to about
  8 KB.

## Compression

Not added. The core implementation inventory approves zlib, but
`cmake/BprZlibProvider.cmake` compiles only its inflate and checksum sources
("no gz* file I/O, no deflate"), for the review bundle reader. LibArchive is
pinned only for macOS update staging, reading ZIP. No other compressor is
approved, and the inventory says approval of one use does not admit other
APIs of a library. So the export stays a `.txt` file and no dependency
changed. Writing a standard `.zip` would need zlib's deflate sources
(`deflate.c`, `trees.c`, from the same pinned 1.3.2 archive) admitted as a
new use. That is the owner's decision, and the core's stored-only
`write_cycle_zip` could then write it.

## Name and header

- **Name.** The save dialog suggests
  `BetterPullReview-diagnostics-2026-10-21-1432-0.3.1.txt`, from the local
  date and time to the minute and the application version
  (`core::diagnostic_export_file_name`). The date comes first, so saved
  exports sort by time. The name has no colon or other character a file
  system refuses. The version is added only when it is dotted numbers, so no
  host, account or repository can reach the name; an impossible time is left
  out. It follows the package names' `BetterPullReview-VERSION` convention,
  with the date ahead of the version for sorting.
- **Header.** The rich export's header now begins
  `{"format":"bpr-diagnostics-v2","exported_at":"2026-10-21T14:32:05-07:00","app_version":"0.3.1","app_build":"0.3.1-<digest>-Release",...`.
  `exported_at` is the host's local time with its offset in ISO 8601, from the
  SQLite maintenance adapter's clock; `app_build` is the running build as its
  records name it, from the logger's export capture. Each is `unknown` when
  not known. The header already carried the records' first and last times.
  The metadata (v1) export is unchanged.

## What is logged

The owner's two problems, checked against what an export now holds:

- **The slow Home refresh ([[BPR-1680]]).** Every refresh's start and end
  and every read's end are logged as `github.discovery` lines with the
  reason, repositories, requests, lane wait and run time, and the export
  keeps them. Pull request page responses carry their page operation and
  time. Nothing else was missing; with the old limit these records were what
  the limit dropped.
- **The review GitHub refused ([[BPR-1715]]).** Only a submission left
  unknown was logged ([[BPR-1540]]). After [[BPR-1715]] the CLI's 422 settles
  as refused, which logged nothing, and neither did a published review, one
  settled by the ten-minute rule or one stopped before its POST. That gap
  would have hidden the refusal. Each such run now logs one
  `github.review_submission` record:
  - `http_status` is the POST's status when this run sent it, else 0;
  - its detail is exactly
    `review_submission_ending=ENDING run=RUN post=ANSWER post_class=N
    refusal=REFUSAL stop=STOP comments=N duplicate=0|1`
    (`core::review_submission_ending_diagnosis`);
  - ENDING is `published`, `refused` (a 4xx answer), `settled` ([[BPR-1715]]'s
    rule) or `not_sent`; RUN is `submit` or `check`; ANSWER is this run's
    POST answer, or the one the attempt kept when Check status settled it;
    REFUSAL is `none`, `review` or `unplaceable_line`; STOP is why a run that
    sent nothing stopped (`stale_head`, `target_moved`, `cancelled`,
    `undesignated_target`, `unavailable`, `other`);
  - its outcome is `succeeded` when published, `cancelled` when stopped by
    cancellation and `failed` otherwise; its failure domain is
    `review_submission`.

  The logger drops a record whose detail is not exactly that line, that
  carries both an ending and an unknown reason, or that rides another
  operation. The export admits the line only when it parses back. Nothing in
  it names a host, repository, account, path or comment. A refused or unsent
  review now counts as an Insights publishing problem, as an unknown one
  already did.

## Change

- `core`: the output limits; `diagnostic_export_file_name`; the review
  submission ending's enums, `valid`, `describe` and
  `parse_review_submission_ending`.
- `application`: the input limits; `diagnostic_export_stamp`,
  `format_diagnostic_export_time` and the header's new fields; the export
  allow-list admits an ending line; `diagnostic_event::ReviewEnding`;
  `diagnostic_export_capture::Build`.
- Adapters: the SQLite maintenance reader frees each file once joined and
  stamps the export; the Quill logger validates endings and supplies its build
  to the capture; the review submission session logs each known ending; the
  Qt panel suggests the dated name.
- [`release-maintenance.md`](../../architecture/release-maintenance.md)
  records the limits, the measurements, the compression decision, the name,
  the header and the endings.

## Evidence

Implementation `685846069`, on local main `7dc497087`; macOS (Apple M4 Pro),
2026-10-21, `dev` built with `-O0 -g0` and the main checkout's CPM cache.

- **Before.** Each control put one part back as it was, rebuilt and ran the
  tests, then restored it:
  - old limits: `application_release_maintenance` fails at "every record of an
    eight-hour session is kept"; `quill_logging` at "both sessions fit the
    export whole";
  - no ending logged by the session: `live_review_submission_provider` fails
    at "without its cause the review is confirmed and nothing is named, and
    one ending is logged";
  - no ending check in the logger: `quill_logging` fails at "an ending whose
    detail is anything but its own line never enters the log";
  - no ending in the export allow-list: `application_release_maintenance`
    fails at "and keeps exactly its counts-only line and the POST's status";
  - the old fixed file name: `qt_release_maintenance_scale_100` fails at "the
    suggestion holds the local date, time and version";
  - a header without the time: `application_release_maintenance` fails at "the
    header names the local export time with its offset and the build".
- **After.** The suites in AC6 pass.
- **Gates.** `coding-format-check` (clang-format 22.1.8) and clang-tidy 22
  with warnings as errors on the 20 changed translation units pass. Both
  reseal checks report nothing to reseal; no `src/app` file changed.
- **Documentation.** `verify_documentation_tests.py` passes 67 tests. With a
  temporary `.agents` link, removed again after the run,
  `verify_documentation.py` reports two things, both for allocation to
  settle: the card's provisional ID format, and the issue counter, `NEXT_ID`
  BPR-1721 behind the shared high water BPR-1722 set by other cards.
