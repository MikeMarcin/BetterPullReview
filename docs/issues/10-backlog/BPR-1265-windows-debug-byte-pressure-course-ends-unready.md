---
schema: "implement-rfc.card.v1"
id: "BPR-1265"
title: "Windows Debug: the large-review byte-pressure course ends unready at an epoch refresh"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0967 Windows station Debug run, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1264--BPR-1265--e0e97a5e10af.json","related_cards":["BPR-0967"]}
---

# BPR-1265: Windows Debug byte-pressure course ends unready at an epoch refresh

## Observation

On bpr-main-win at `682ea1392` (windows-dev Debug, MSVC 19.51, Qt 6.11.2),
`application_large_review_cache_bytes` failed after 323.46 s with
`byte-pressure stored head refresh timed out`. Its course clock was 840 s, so
the clock did not end the wait: the byte course's wait also stops when the
epoch status becomes failed, unavailable or cancelled. The same row passes on
Windows Release (280.70 s), Linux Release and Debug, and macOS. The Debug
summary keeps no phase detail, so which terminal ended it is unknown.

[[BPR-0967]] did not scale this away: it doubles Windows Debug course clocks
only because the resources course outran its 90 s clock, a different cause.
From `f61340e17` each report records `Extended.WaitExpiry`: the wait, its
clock, elapsed time, epoch status and whether the clock expired.

## Outcome

Rerun the Windows Debug row at or after `f61340e17`, read `WaitExpiry` and the
`large-review wait:` line, and attribute the ending epoch status to its owner.
Repair it, or record why Debug cannot complete the course and the owner's
decision for the row.

## Direct acceptance

- The row passes in Windows Debug, or the card records the measured cause and
  the owner's decision for the Debug row.
