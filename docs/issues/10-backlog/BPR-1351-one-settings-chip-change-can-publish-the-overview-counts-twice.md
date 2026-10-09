---
schema: "implement-rfc.card.v1"
id: "BPR-1351"
title: "One Settings chip change can publish the Overview counts twice"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"bpr-main-linux, BPR-1094's large-review course, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","related_rfcs":["RFC-BPR-0049"],"related_cards":["BPR-1094","BPR-1088","BPR-1089"]}
---

# BPR-1351: One Settings chip change can publish the Overview counts twice

## Finding

In [[BPR-1094]]'s large-review course at `269f7ccff` (515 files, 1,000
threads, the real application, offscreen), 23 Settings chip changes
published the Overview counts 24 times. No GitHub request was made, no body
was parsed and no conversation was tallied. Counts and focus were unaffected.
It was not investigated further.

## Outcome

Each chip change publishes the counts once, or a second publication is shown
to be a deliberate, idempotent republish, documented as such.

## Acceptance

- Find which change published twice and why. Candidates are a settings
  publication echo, the BPR-1088 change-back path, or a window
  republication.
- A test counts publications per chip change and asserts one, with a
  failing-before control. Alternatively, the card records why two is
  correct.
- BPR-1094's structural assertions (zero requests, zero parses, zero
  tallies) still hold.

## Boundaries

No live GitHub; offscreen only.
