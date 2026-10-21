---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-J5"
title: "Open a generated file's diff from the generated row on request"
status: "Backlog"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-PENDING-J3"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"milestone":"RFC-BPR-0089 M2","origin":"RFC-BPR-0089 'Generated files are set aside': 'Expanding it lists them, and opening one shows its diff on request.'","related_cards":["BPR-PENDING-J3"]}
---

# BPR-PENDING-J5: Open a generated file's diff from the generated row on request

RFC-BPR-0089 says the generated row lists the files set aside, "and opening one
shows its diff on request", while they stay out of Next file, the counts and
preparation.

## Today

The generated row ([[BPR-PENDING-J3]]) lists each file set aside and why, and
its context menu offers Review anyway, Show as normal and Follow
.gitattributes. Activating a listed file opens nothing: the workbench reads
only files in its actionable order, and a set-aside file is not in it. Review
anyway is the way to read one now; it returns the file to the walk.

## Acceptance

- **AC1.** Activating a file in the generated row shows its diff, read on
  request, without adding it to Next file, the remaining counts or
  preparation ahead, and without writing a mark.
- **AC2.** Leaving the file (or the row's list) returns the review to its set;
  the file is still set aside.
