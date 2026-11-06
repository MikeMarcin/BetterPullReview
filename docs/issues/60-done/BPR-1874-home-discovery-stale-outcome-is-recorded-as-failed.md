---
schema: "implement-rfc.card.v1"
id: "BPR-1874"
title: "Home's discovery refresh that ends stale is recorded as failed and names the rejected row"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05, split from BPR-1873","related_cards":["BPR-1680","BPR-1744","BPR-1873","BPR-1875"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "7c3b6cce9", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_home_discovery|application_review_queue_enrollment|github_paged_read|quill_logging)$'", "detail": "test_discovery_batches_retained_reads: a batched retained read that leaves #7 out (Unread: permission, HTTP 404) writes one failed read line 'check=missing_row row=2' (read 1, retained, pull_requests=3) carrying github_failure=permission and HTTP 404 and no repository text; the refresh ends state=stale failed=0 with outcome failed. A row the read did not ask for writes check=unasked_row row=0. The next clean refresh writes no rejection and ends current with outcome succeeded. test_discovery_records_each_refresh_and_read: a refresh stale from a failed read now ends failed. Core: every rejection round-trips; refresh lines, rows without a rejection, rows past the read's pull requests, check=none and a missing row are refused. Adapter: test_retained_home_batch_read returns the pull request REST could not read in Unread with its failure. Logging and export accept the new line."}}, {"criterion": "AC2", "outcome": "pass", "revision": "7c3b6cce9", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_personal_discovery_public_path$'", "detail": "Composed fixture course, provider to host to diagnostics file: every refresh that reads normally ends state=current (first session refreshes 1-6, restarted session's last); stale and stopped only where the course makes reads fail on purpose (offline, refused repositories). No line carries check=, so no host check refuses a row on fixtures. The owner's live cause is not identified from code alone; the new line names it on the next occurrence (see Result)."}}, {"criterion": "AC3", "outcome": "pass", "revision": "7c3b6cce9", "evidence": {"kind": "command", "command": "python3 scripts/verify_documentation.py --root .; cmake --build --preset dev --target coding-format-check", "detail": "DOCUMENTATION_OK; clang-format 22 (22.1.8) check passed over 1944 files."}}]
---

# BPR-1874: Home's discovery refresh that ends stale is recorded as failed and names the rejected row

## Observed

Home's discovery refresh ends `stale` on most cycles with no failed read.
`personal_discovery_host` calls `fail(queue_discovery_problem::failed)` in
`apply_metadata` / `apply_metadata_row` when it rejects a retained row, yet
`finish_cycle` records the refresh as `succeeded`, because only `CycleStopped`
records `failed`.

## Change

- Find why retained rows are rejected on most cycles; fix the rejection if it
  is wrong.
- Record a stale outcome as failed in the diagnostic, naming which check
  rejected which retained row (no private content in the log).

## Acceptance

- AC1. A cycle that rejects a retained row records `failed` with the check and
  row identity; a clean cycle records `succeeded`.
- AC2. If the rejection was spurious, a normal cycle on fixtures ends fresh.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.

## Result

- A refresh (and a focused one-row read) that ends `stale` or `stopped` now
  records `github.discovery.failed`; only one that read everything succeeds.
- Each host check that refuses part of a read's answer writes its own failed
  read line, `... completeness=X check=REJECTION row=N`
  (`core::discovery_rejection`): `incomplete_answer`, `unasked_row`,
  `missing_row`, `duplicate_row`, `undetailed_row`, `missing_head`,
  `review_limit`, `malformed_head`, `unattributed_row`, `mismatched_answer`,
  `unexpected_cancel`. `row` is the place, from 1, among the pull requests the
  read asked for (0 for the whole answer), correlated with the read by
  `refresh=` and `read=`. The line names no number, repository or title, as
  the BPR-1680 contract requires. Older lines still parse.
- Why rows are refused with no failed read: a batched retained read
  (`read_retained_batch`) that falls back to REST and cannot read one pull
  request alone (permission or 404, malformed, response limit) left it out of
  an otherwise successful answer, so the read ended `succeeded`, the host
  failed the missing row, and the failure kind was lost. That is the only
  per-row path that repeats every cycle from the code; for one persistently
  unreadable retained pull request it is a real failure, not a spurious check.
  The adapter now returns those pull requests in
  `personal_discovery_success::Unread` with their provider error, and the
  `missing_row` line carries that `github_failure` and HTTP status, so the
  owner's next log says which check, which row and why.
- Fixture courses show no rejection on a normal cycle (AC2). The live cause on
  the owner's profile is unconfirmed until a log from this build.
