---
schema: "implement-rfc.card.v1"
id: "BPR-1277"
title: "On Windows the round-trip composition contract stalls at its 5 s draft wait right after mass file churn"
status: "Ready"
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
