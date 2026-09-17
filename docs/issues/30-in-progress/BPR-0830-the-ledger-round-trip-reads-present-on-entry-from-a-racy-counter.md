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
