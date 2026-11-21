---
schema: "implement-rfc.card.v1"
id: "BPR-2341"
title: "A remembered sign-in grant sometimes has not settled before Quit in the restoration course"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2339--BPR-2342--60dd1b567452.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2341: A remembered sign-in grant sometimes has not settled before Quit in the restoration course

## Finding

`application_profile_window_review_restoration_public_path` failed at
"remembered grant settles before Quit" in 2 of 5 Ubuntu Release suites. Not
reproduced in 10 CPU-loaded or 5 disk-loaded runs. A separate missing-grant
control in the same course was a test fault and is fixed (616c85ff8).

On a miss the course now names the three paths the review asked to tell apart
(c46fe08cc, 4a13a281a), and watches up to 60 s more to tell a slow write from
a stuck one:

1. the account session: its status and reason;
2. the refresh: its state, and the refresh store's count;
3. the binding store: whether it is idle, and the vault's store count.

## Acceptance

- AC1. The next miss is read against those three and the stuck path is named.
- AC2. That path is fixed, and the course passes 20 of 20 in Ubuntu Release
  suites.
