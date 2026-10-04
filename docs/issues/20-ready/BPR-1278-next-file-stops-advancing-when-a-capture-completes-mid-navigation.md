---
schema: "implement-rfc.card.v1"
id: "BPR-1278"
title: "Next file stops advancing when a capture completes mid-navigation"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1274 Windows Debug large-project row at 8d838c696 (bpr-main-win)","reservation":"docs/issues/_reservations/BPR-1278--BPR-1278--d362f90a34fd.json","related_cards":["BPR-1274","BPR-1184"]}
---

# BPR-1278: Next file stops advancing when a capture completes mid-navigation

## Observation

[[BPR-1274]]'s Windows Debug run of `application_review_cycle_large_project`
(`build/windows-dev-tour-cli` at `8d838c696`) failed in its 2,000-file pass on
the tree corpus, in the course's navigate-during-capture step:

```text
navigate-during-capture …: Next file reads the file after d000/s0/f000008.txt
```

The window's state then was `source d000/s0/f000008.txt @ R1`, `revisions
[R1, R2]`, `newer: true`. The capture had ended while Next file was being
pressed, and that press never moved off the file. [[BPR-1184]]'s earlier
Windows Debug run (at `e9c7eaa98`, before BPR-1274's change) stalled silently
at the same step, request 69.

## Outcome

Reproduce it deterministically by landing the capture's completion between a
Next file press and its result, find the cause and fix it, so that Next file
always advances on the revision being read and never jumps to the new capture
unasked. Add a negative control. Run the large-project Debug row and the
navigation tests.
