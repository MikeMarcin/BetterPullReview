---
schema: "implement-rfc.card.v1"
id: "BPR-0830"
title: "The ledger round trip still reads \"present on entry\" from a counter the window's re-read can satisfy"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux under BPR-0824 (task-20260917-lnx-verify new defect 4)","found_at":"3b968fcc0027c774187c11489f4020c2463bcd2f","run":"run-20260916-stations","dispatch":"dsp-002"}
acceptance: [{"criterion":"AC1 the round-trip course and application_review_ledger_composition_contract pass on macOS and Linux, repeated","outcome":"pass","revision":"4b677d8be911f68ddbd8f387e955e76c24befd9a (Linux); build/r2-dev from 2842f484 plus BPR-0820, compiled sources equal to 866c5b90 (macOS)","evidence":{"kind":"command","detail":"Linux: composition contract 10/10, diff workbench public path 10/10, seed-then-reuse 10/10 (docs/verification/dsp2-linux-confirmation.md, BPR-0830). macOS: composition contract 5/5, diff workbench public path 5/5 with PRESENT_ON_ENTRY:0, seed-then-reuse 5/5 with reuse PRESENT_ON_ENTRY:1 (build/r2-logs/dsp3-0830-s1-ctest.log, dsp3-0830-s2-roundtrip.log)."}},{"criterion":"AC2 the negative control fails as expected","outcome":"pass","revision":"same as AC1","evidence":{"kind":"command","detail":"file_receipt rows deleted before reuse: reuse reports PRESENT_ON_ENTRY:0 on Linux 3/3 and macOS 3/3 (dsp3-0830-s3-negative-control.log); Linux lldb-forced re-sample showed the old counter printing :1 and the fix :0."}}]
---

# BPR-0830: The ledger round trip still reads "present on entry" from a counter the window's re-read can satisfy

## Defect

[[BPR-0824]] found that the review ledger composition probe raced. Its
window's queued `refresh_shared_ledger` re-reads the receipt the probe has
just committed, so `ReceiptLoadHits` can count that re-read. `5875ce4f`
moved the composition probe to `MaterialObservations == 1 &&
MaterialReceiptHits == 1`. The ledger round-trip course
(`review_ledger_round_trip`) still sets "present on entry" from
`ReceiptLoadHits > 0U`, which the same re-read can satisfy, so a receipt
that was not present on entry could be reported as present.

## Scope

- Move the round trip's "present on entry" onto the material observation's
  own receipt hit, as BPR-0824 did for the composition probe.
- Keep a negative control: with the receipt deleted before entry, the course
  must still report absent.

## Acceptance

- The round-trip course and `application_review_ledger_composition_contract`
  pass on macOS and Linux, repeated.
- The negative control fails as expected.

## Linux half (bpr-linux, task-20260917-lnx-confirm)

**Fix.** `review_ledger_round_trip` in `src/app/src/github_application.cpp`
now sets "present on entry" from `MaterialReceiptHits > 0U`, the counter
`5875ce4f` added, instead of `ReceiptLoadHits > 0U`.
`docs/verification/fixture-catalog.md` names the counter. It stays `> 0U`
rather than the probe's `== 1U`: the round trip does not own how many
publications precede its sample, so an exact count could report a present
receipt as absent. No platform code is involved.

**The defect, reproduced on Linux.** `lldb-22` forced a re-sample after the
Seen commit, on a seeded root whose receipt row had been deleted before
reopen. The base code printed `REVIEW_LEDGER_PRESENT_ON_ENTRY:1`
(`ReceiptLoadHits=1`, `MaterialReceiptHits=0`). The fix printed `:0` twice. With
the receipt kept, the fix printed `:1`.

**Linux runs on the candidate.**

- `application_review_ledger_composition_contract` passed 10 of 10.
- `application_diff_workbench_public_path` passed 10 of 10, each with
  `REVIEW_LEDGER_PRESENT_ON_ENTRY:0`.
- The cross-process round trip (seed, then reuse) gave reuse
  `PRESENT_ON_ENTRY:1` 10 of 10.
- **Negative control.** With the receipt deleted before entry, reuse reported
  `PRESENT_ON_ENTRY:0` 3 of 3.
- The host test, both epoch persistence rows, epoch maintenance and two
  appearance contracts pass.

The evidence is in "BPR-0830" of
[the dsp-002 Linux confirmation](../../verification/dsp2-linux-confirmation.md).

**Review.** An independent `rfc-reviewer` found no blocking issue.

**Pins, for main's reseal.** The edit moves
`application_review_round_trip_composition_contract`'s pins, which this card
does not own and did not change:

- whole file: `ba30b1bad5c7eadf64305d0638894696d218d7ab52276c855c7e1aa4afcac834`;
- manifest: `590b846f6059b28bbdf94f0feadf9444d0c98c45de000972c30e391facef24a4`;
- mutation branch: unchanged.

That contract is red on `linux-lane` until the reseal.

**Status.** The Linux criteria are met. The card stays In Progress until
macOS repeats the round trip and the composition contract, including the
negative control.

## macOS half; finished (dsp-003)

Run on `build/r2-dev` built from `2842f484` plus BPR-0820's change, which
contains `4b677d8b` (merged `48b6422b`). The compiled sources match main
`866c5b90`. The runs used the ctest registration's macOS network-deny wrapper
and environment. Logs are in `build/r2-logs/`.

- `application_review_ledger_host` and
  `application_review_ledger_composition_contract` pass 5 of 5 (the contract
  35.5 to 36.3 s; `dsp3-0830-s1-ctest.log`).
- `application_diff_workbench_public_path` passes 5 of 5, each run printing
  `REVIEW_LEDGER_PRESENT_ON_ENTRY:0` and `REVIEW_LEDGER_OK`.
- **Cross-process round trip.** Each run used a fresh canonical private
  root, with `--github-fixture github-com --epoch-fixture-root R`, seed then
  reuse. Seed printed `PRESENT_ON_ENTRY:0` and reuse printed
  `PRESENT_ON_ENTRY:1`, 5 of 5, each with `REVIEW_LEDGER_OK`
  (`dsp3-0830-s2-roundtrip.log`).
- **Negative control.** Between seed and reuse, the `file_receipt` rows in
  `<root>/state/v1/review-ledger.sqlite3` were deleted (1 to 0, with
  `quick_check` ok). Reuse printed `PRESENT_ON_ENTRY:0` and
  `REVIEW_LEDGER_OK`, 3 of 3 (`dsp3-0830-s3-negative-control.log`).

**Limit.** An unforced control cannot tell the fix from the old code:
Linux's base code also printed `:0` in its two unforced control runs. Only
Linux's `lldb`-forced re-sample showed the old counter reporting a deleted
receipt as present. That forced reproduction was not repeated on macOS. What
macOS shows is that no run reports a missing receipt as present, and that a
kept receipt is still found.

**Seen along the way, not changed.** A `--epoch-fixture-root` spelled through
a symbolic link (`/var/folders/…` instead of `/private/var/folders/…`) prints
only `STARTUP_FAILED`, with no cause
(`dsp3-0830-s2attempt1-noncanonical-*.log`). The storage root requires a
canonical path (`profile_storage_root.cpp` ~53).
