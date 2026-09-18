---
schema: "implement-rfc.card.v1"
id: "BPR-0902"
title: "Windows: the device refresh course rotates once instead of twice, and the flow requires re-enrolment instead of scheduling"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win, exposed once BPR-0877's lock skip removed the sentinel that was masking it","found_at":"d6dfca2b","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0902: Windows: the device refresh course rotates once instead of twice, and the flow requires re-enrolment instead of scheduling

## Defect

`application_device_authorization_refresh_course` was one of [[BPR-0877]]'s four
rows. Three of the four now pass. This one got further and stopped on a
**second** fault that the first was hiding.

BPR-0877's cause was that the durable-copy scan answered
`size_t::max()` — its "unavailable" sentinel — because it could not read the
lease lock it holds open itself. With that fixed the row reaches
`DEVICE_FLOW_DURABLE_COPIES:0`, which is correct, and then fails on this:

| Marker | Expected | Actual |
|---|---|---|
| `ROTATIONS` | 2 | **1** |
| `REFRESH_STATE` | `scheduled` | **`reenrollment_required`** |
| `VAULT_STORES` | 3 | **2** |
| `REFRESH_STORES` | 3 | **2** |
| `REFRESH_STORED_TOKEN` | — | differs accordingly |

So the second rotation never happens, and the flow answers that the device must
re-enrol rather than that a refresh is scheduled.

**It is not a timeout.** The row ends at 6.8 s of a 12 s budget.

This is why BPR-0877's row-level `PASS_REGULAR_EXPRESSION` being one multi-line
literal matters: the first mismatched line fails the whole thing and CTest
reports the regular expression rather than the line, which is why the original
sweep read this cluster as "`ENROLLMENTS:1` never printed" when it did print.

## Scope

- Establish whether the second rotation is refused or never attempted. Those are
  different faults and the markers above do not distinguish them.
- `reenrollment_required` is a real product answer, not a test artefact. Find
  what makes the device look un-enrolled to the second rotation when the first
  one succeeded and stored.
- Check whether this is Windows-only. The three sibling rows pass on Windows and
  all four pass on macOS and Linux today, which is suggestive but not a
  measurement of *this* assertion on the other platforms.
- Consider splitting the row's pass expression, or at least recording that a
  single multi-line literal reports the regular expression rather than the
  failing line. That cost this run a wrong reading once already.

## Acceptance

- The row passes on Windows, or the remaining difference is a named, understood
  cause with its own record.
- The card says whether the second rotation was refused or never attempted, and
  why the device looked un-enrolled.
- macOS and Linux are unaffected, checked rather than assumed.
