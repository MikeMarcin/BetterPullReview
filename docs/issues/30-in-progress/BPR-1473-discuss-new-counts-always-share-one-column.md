---
schema: "implement-rfc.card.v1"
id: "BPR-1473"
title: "Discuss's new-post counts always share one column"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1471"]
priority: "P3"
metadata: {"reservation":"docs/issues/_reservations/BPR-1473--BPR-1473--167da41ffd3a.json","origin":"owner request 2026-10-13"}
---

# Discuss's new-post counts always share one column

BPR-1471 lined up the blue new-post counts whether or not a note is
resolved, but an unsent "Your reply" still pushed a note's count left. The
owner asked for the counts to be aligned always.

## Acceptance

- AC1: On every closed note the blue count's right edge is the same, whether
  or not the note is resolved or carries an unsent reply. "Your reply" stands
  left of the count, and a test asserts that geometry.

## Evidence

Pending.
