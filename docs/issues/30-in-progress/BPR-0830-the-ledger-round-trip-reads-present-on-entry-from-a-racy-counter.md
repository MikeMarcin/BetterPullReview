---
schema: "implement-rfc.card.v1"
id: "BPR-0830"
title: "The ledger round trip still reads \"present on entry\" from a counter the window's re-read can satisfy"
status: "In Progress"
rfcs: ["RFC-BPR-0007"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux under BPR-0824 (task-20260917-lnx-verify new defect 4)","found_at":"3b968fcc0027c774187c11489f4020c2463bcd2f","run":"run-20260916-stations","dispatch":"dsp-002"}
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
