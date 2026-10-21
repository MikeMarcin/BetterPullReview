---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-U1"
title: "A line chosen while a kept tab revalidates after a reread stays chosen"
status: "Backlog"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1707"]
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"origin":"Observed while building BPR-1707's composed course (2026-10-21)","related_cards":["BPR-1707","BPR-1478","BPR-1527"]}
---

# BPR-PENDING-U1: A line chosen while a kept tab revalidates after a reread stays chosen

[[BPR-1707]] keeps the reviewer's chosen file across the reread that follows
a publication. One narrower gesture is still lost.

## Observed

A refresh that keeps the diff on screen revalidates the active tab: the
composition asks the tab host to load the same view again
(`represent_refreshed_tab`), and when the refreshed publication arrives it is
adopted under its new request without being presented again. A line the
reviewer selects in that window, typically a few hundred milliseconds, is
replaced once the revalidation lands by the tab's remembered reading
position. In BPR-1707's composed course (commit-filter-comment-v1, with C3
hidden), right line 5 was selected after the reread was accepted. The kept
publication then arrived, and the selection observer received right line 10,
the line selected before the publication, so no draft was offered on line 5.
The course now waits for the tab presentation to settle before it selects a
line.

The likely cause is that the reading position observed during the
revalidation is recorded against the previous load's request, so the tab host
keeps the older selection and presents it when the load completes.

## Acceptance

- **AC1.** A line selected while a kept tab revalidates after a reread is
  the line selected once the revalidation lands, under a filter and on Whole
  PR.
- **AC2.** A reading position restored from the window's kept tabs is still
  restored when the reviewer selected nothing meanwhile.
