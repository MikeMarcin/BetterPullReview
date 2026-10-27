---
schema: "implement-rfc.card.v1"
id: "BPR-0878"
title: "Windows: eight unrelated sweep failures, each needing its own triage"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005","triage_20261026":{"date":"2026-10-26","disposition":"done","reason":"All eight rows pass on Windows at the v0.3.4 native suite source."}}
acceptance: [{"criterion":"BPR-0878:every-row-passing-or-carried","outcome":"pass","revision":"62bfc09f6","evidence":{"kind":"command","path":"docs/verification/release-v034-results.md","detail":"v0.3.4 Windows 11 x64 native suites at 62bfc09f6 (Release 832/832 PASS, Debug windows-dev junit): rfc0017_release_absence_negative_control, sqlite_epoch_catalog_behavior_contract, github_cli_process_runner, live_review_submission_restart_recovery, epoch_kill_recovery, epoch_transaction_kill_recovery, rfc0016_production_course_security and application_review_submission_composition_public_path all pass in both junit reports."}}]
---

# BPR-0878: Windows: eight unrelated sweep failures, each needing its own triage

## Why this is one card

The run's first full Windows sweep found 53 failures. Most fall into groups
with a single measured cause and have their own cards: [[BPR-0872]],
[[BPR-0873]], [[BPR-0874]], [[BPR-0875]], [[BPR-0876]], [[BPR-0877]], plus
[[BPR-0869]] and [[BPR-0870]] from the earlier Windows task.

These eight do not share a cause with each other. This card exists so they are
not lost between the grouped ones; it is **intake, not a unit of work**. Each
line below should become its own card, or be fixed and recorded, as it is
picked up. Do not close this card by fixing some of them.

Row-by-row first failing lines are in
`docs/verification/dsp4-windows-merged-head-sweep.md`.

## The eight

1. `rfc0017_release_absence_negative_control` —
   `rfc0017-test-inventory:fail:string present in release binary: --network-denied`.
   A negative control finding a test-only string in a release binary is the
   kind of thing worth looking at promptly rather than by P3.
2. `sqlite_epoch_catalog_behavior_contract` — "FAIL: a copy of the previous
   file migrates to v6". A copied store file again, the [[BPR-0853]] family.
3. `github_cli_process_runner` — "FAILED: opt-in preserves only exact bounded
   stdout and completed exit code".
4. `live_review_submission_restart_recovery` —
   `RuntimeError: --kill-prepared exited before checkpoint (1)`, then "FAILED:
   submission store opens".
5. `epoch_kill_recovery` — `RuntimeError: fetch: recovery refs do not contain
   one exact base/head pair`.
6. `epoch_transaction_kill_recovery` — `RuntimeError: receiving: recovery refs
   do not contain one exact base/head pair`. Almost certainly the same cause
   as 5; take them together.
7. `rfc0016_production_course_security` — `AssertionError: Lists differ`, a
   backslash-versus-forward-slash expectation on `outcomes/*.json` paths. A
   Windows path-separator assumption, and probably the cheapest of the eight.
8. `application_review_submission_composition_public_path` — fails in 0.06 s
   with no output before its marker. No output at all is its own signal:
   it is failing before it can say anything.

## Scope

- Triage each into its own card with a measured cause, or fix and record it.
- Items 5 and 6 look like one cause; items 1 and 8 look like the two most
  worth reaching first, one for what it implies about release binaries and one
  because it dies before producing evidence.
- Establish for each whether it predates this run. None of this run's changes
  is implicated in any of them, but nobody has checked.

## Acceptance

- Every row above is either passing on Windows or carried by its own card with
  a measured cause.
- This card is closed only when none of the eight is left unaccounted for.

## Item 3 fixed and recorded under BPR-1114 (bpr-main-win, 2026-09-24)

`github_cli_process_runner` still failed on Windows at `877e3d60`, at "opt-in
preserves only exact bounded stdout and completed exit code".

**Cause, shown by the control below.** The test executable is its own
helper child, and its `http-error` mode writes
`HTTP/2.0 401 Unauthorized\r\n\r\n{}` through `std::cout`. The helper's
standard output was in the C runtime's text mode, which writes each `\n` as
`\r\n`. So the exact bytes the row expects could not arrive. The runner itself
was not at fault. The bytes that did arrive were not captured.

**Fix.** The helper sets its standard output and error to binary mode on
Windows, as the Git process contract's fake Git already does ([[BPR-0437]]).
The change is in `tests/github_contract/github_cli_process_runner_tests.cpp`.
It is part of [[BPR-1114]], whose new Windows rows run after this one and
could not run while it failed.

**Checked.** With the fix, the row passes on Windows. The control is a
station patch that put the helper back in text mode, rebuilt and ran, then
was restored. Under that patch the row fails at the same line. So item 3 is
accounted for. The other seven are unchanged.

## Closed in triage, 2026-10-26

All eight rows now pass on Windows. Each one passes in both the Release
and the Debug junit of the v0.3.4 Windows 11 native suites, which ran at
`62bfc09f6`: `rfc0017_release_absence_negative_control`,
`sqlite_epoch_catalog_behavior_contract`, `github_cli_process_runner`
(item 3, fixed under [[BPR-1114]]), `live_review_submission_restart_recovery`,
`epoch_kill_recovery`, `epoch_transaction_kill_recovery`,
`rfc0016_production_course_security` and
`application_review_submission_composition_public_path`. The Release suite
passed its complete inventory, 832 of 832
([v0.3.4 results](../../verification/release-v034-results.md)). Nothing
remains unaccounted for, so the card's acceptance holds. No cause is
recorded for items 1, 2 and 4 to 8: they stopped failing somewhere between
the 2026-09-17 sweep and that run.
