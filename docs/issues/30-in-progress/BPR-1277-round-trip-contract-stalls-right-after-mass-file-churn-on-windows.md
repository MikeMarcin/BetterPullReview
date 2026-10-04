---
schema: "implement-rfc.card.v1"
id: "BPR-1277"
title: "On Windows the round-trip composition contract stalls at its 5 s draft wait right after mass file churn"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"BPR-1275 closeout, windows-dev at da6e49e9f (bpr-main-win)","reservation":"docs/issues/_reservations/BPR-1277--BPR-1277--d7f9378b6526.json","related_cards":["BPR-1184","BPR-1275"]}
---

# BPR-1277: On Windows the round-trip composition contract stalls at its 5 s draft wait right after mass file churn

## Observation

`application_review_round_trip_composition_contract` failed four times in a row
on Windows `windows-dev` at `da6e49e9f`. The first failure was a `-j4` run; the
rest ran alone on an otherwise idle host. Each stopped at a 5 s wait:
`REVIEW_ROUND_TRIP_DRAFT_STALLED waited_ms≈5005 wait_cap_ms=5000`, once
`SEMANTIC_COMMENT_SERVER_REFRESH_STALLED`. It had passed at `d8fa5bfc2`, and
no commit between the two touches `src/`, the contract or its executable.

## Outcome

Find the cause before changing anything. If the host is the cause, fix the
course that leaves it slow rather than raise the cap. Show 3 of 3 passes right
after a large-project run, and a negative control.

## Cause (bpr-main-win, 2026-10-04)

**Not the host.** The first theory, a slow host after the large-project
course's file churn, was measured and dropped:

- After a 250,001-entry create and delete, the contract ran back to back for
  15 minutes and failed 6 of 70 times, spread from t+12 s to t+695 s.
- A 200-file fresh-file probe saw no slow window.
- The same binary on the same quiet host failed 6 of 40 runs through ctest
  (15%), and about 3–5% of direct launches.

**An ordering race in the product.** The stall never resolves. With the probe
cap raised to 60 s (a local, uncommitted build), the stalled run kept
`review_draft=absent` until the course's whole 40 s budget expired. Local
`std::source_location` tracing captured two failures with the same sequence:

1. The course selects the off-hunk line R:4. A draft is made there and its
   body typed.
2. A new selected-diff request arrives.
   `begin(const workbench_selected_diff_request&)` calls
   `clear_diff_selection()`, which runs `adopt_review_selection(nullopt,
   selection_invalidated)`. That resets the review draft and clears the
   comparison.
3. The comparison then restores an older selection, L:29, so the R:4 draft is
   gone and the selection is on another line and side.

Every run gets three of these requests. In a passing run the third arrives
before the draft exists, and the course re-selects R:4 afterwards. Extra
logging shifts the timing (0 of 80 with one more print).

**What a reviewer would see.** If the selected diff is requested again while
someone is writing a comment, the editor clears and the selection jumps to
another line. It has not yet been checked whether the typed body survives in
the private draft store.
