---
schema: "implement-rfc.card.v1"
id: "BPR-0834"
title: "Two epoch courses fail at startup on Windows before any step"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0834: Two epoch courses fail at startup on Windows before any step

## Defect

On Windows, uncontained,

- `application_epoch_ordinary_update_recreated` and
- `application_epoch_portable_occurrence_two_catalogues`

both print `STARTUP_FAILED` before any course step, identically with the
unmodified course script at HEAD. Narrowing their fixture root to the
owner-private shape did not help (that change was reverted). Because neither
passes uncontained, [[BPR-0812]] could not measure their contained cause or
pin them; the manifest still reports their six findings.

Reported by `bpr-win` in `task-20260917-win-hardening` step 2, not carded
there.

## Scope

- Find which startup refusal each prints (the `startup_failure` value, as
  the station did for [[BPR-0832]]) and fix it.
- Then measure the contained run and pin or contain the rows, which closes
  BPR-0812's remaining findings.

## Acceptance

- Both courses pass uncontained on the Windows station, and each is either
  contained and passing or pinned with a measured reason.
- They still pass on macOS.
