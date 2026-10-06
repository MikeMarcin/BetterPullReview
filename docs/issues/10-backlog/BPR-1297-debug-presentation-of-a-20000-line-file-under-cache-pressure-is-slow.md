---
schema: "implement-rfc.card.v1"
id: "BPR-1297"
title: "Debug presentation of a 20,000-line file under cache pressure takes 135–372 s per selection"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"BPR-1265 Windows Debug runs, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1297--BPR-1297--6e67e2e347b2.json","related_cards":["BPR-1265","BPR-1264"]}
---

# BPR-1297: Debug presentation of a 20,000-line file under cache pressure takes 135–372 s per selection

Found by bpr-main-win while fixing [[BPR-1265]]: Windows 11 Home 10.0.26200,
MSVC 14.51, Qt 6.11.2, offscreen, `build/windows-dev` Debug, at `3036a08af`
and later. It has not been measured on macOS or Linux. This is a low-priority
investigation and gates nothing. Since [[BPR-1265]],
`application_large_review_cache_bytes` runs only in optimized builds, and a
Debug run reports Skipped.

## Observation

Before [[BPR-1265]]'s job-assignment fix, the course's refreshes failed
early. Once they no longer did, three Debug runs of
`application_large_review_cache_bytes` each reached the
`cache_byte_selection` phases. There the course clock (2,520 s, the Windows
Debug scale of 3) expired at selection 6, 7 or 11.

| Phase | Debug, per phase | Note |
|---|---|---|
| `byte_epoch_refresh_{2..32}` | 25–48 s each | Grows through the course |
| `byte_first_head_selection` | 3.9 s | |
| `cache_byte_selection_{1..}` | 135–372 s each | More than ten per course |

One selection, `cache_byte_selection_2`, in detail:

- 135.7 s and 3,315 event-loop pulses;
- only 528 pulses with diff work, so 2,787 had none;
- P95 pulse interval 61.3 ms, the largest 588 ms;
- at most one diff job at a time.

So for most of the phase the GUI thread is busy presenting the 20,000-line
file, under the byte course's 30 MB cache pressure, not computing its diff.
In an optimized build the whole course takes 280–316 s.

The one recorded Debug pass of this course, at `f61340e17`, took 1,626 s in
all. That was before [[BPR-1264]]'s bounded fill (`ac0d9d1d4`) and the later
`diff_workbench` changes (`5feb7f3b3`, `201c96f21`, `b109295f4`). A clean
comparison against the bounded fill was not possible at current `main`,
because reversing `ac0d9d1d4` conflicts with those later changes.

## Outcome

Profile the Debug presentation of one cache-pressure selection. Candidates
include the bounded fill's batches, `QTextDocument` layout, classification
and syntax colouring, and MSVC's checked iterators. Then decide whether this
is a Debug-only cost, or a regression the optimized build hides and that is
worth fixing.

## Direct acceptance

- The card records where a Debug selection's time goes, measured.
- It records either a fix, with a before-and-after measurement, or the
  owner's decision that the cost is Debug-only and stays.
