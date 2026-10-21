---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-L5"
title: "A reread after publishing keeps the reviewer's chosen file, and a filter gesture during a refresh is kept"
status: "Backlog"
rfcs: ["RFC-BPR-0089"]
depends_on: []
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"origin":"Observed while building RFC-BPR-0089 M4's composed and live courses (2026-10-21)","related_cards":["BPR-PENDING-L3"]}
---

# BPR-PENDING-L5: A reread after publishing keeps the reviewer's chosen file, and a filter gesture during a refresh is kept

Two observations from RFC-BPR-0089 M4's courses. Neither changes where a
comment goes; both make the reviewer repeat a gesture.

## Observed

- **The reread drops the chosen file.** After a comment is published the
  window reads the pull request again. The diff stays on screen, but the
  workbench then has no selected file, and choosing the same file again does
  nothing until another file is chosen first. It happens on Whole PR too. In
  the composed course the filtered file list could also be published once
  more after the reread, clearing a choice made in between (about one run in
  four before the course chose the file until it stayed chosen).
- **A filter gesture during a refresh is lost.** In the live course, Hide
  selected chosen within about fifteen seconds of Refresh left the chip
  hidden with its old text; the same gesture after the refresh landed
  applied. The course now repeats it.

## Acceptance

- **AC1.** After a publication's reread the file the reviewer had chosen is
  still chosen, on Whole PR and under a filter, and choosing it again shows
  it.
- **AC2.** A commit-filter gesture made while a refresh of the same head is in
  flight applies once the refresh lands, or is refused and says why.
