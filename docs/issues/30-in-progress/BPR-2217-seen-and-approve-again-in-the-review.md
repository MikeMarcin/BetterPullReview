---
schema: "implement-rfc.card.v1"
id: "BPR-2217"
title: "Seen and Approve again move from Home into the review"
status: "In Progress"
rfcs: ["RFC-BPR-0098", "RFC-BPR-0116"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2217--BPR-2217--3558698e5e90.json","origin":"owner, 2026-11-16, about BPR-2049","supersedes_surface_of":"BPR-2049"}
---

# Seen and Approve again move from Home into the review

## Origin

The owner, 2026-11-16, about [[BPR-2049]] (Seen / Approve again for an
approval with new commits, until now only a hover action on Home rows):
"Oh well its dumb to put that on home, I'm going to look at the new commits
from within the review."

## Scope and acceptance

- **AC-a.** In the review of a pull request I approved that has new commits
  since, where my approval still stands, the review offers Seen beside
  Approve. Seen marks the new commits seen without submitting anything to
  GitHub, and the review then reads as up to date.
- **AC-b.** Where GitHub dismissed my approval, the review's primary action
  reads Approve again.
- **AC-c.** The Home row's hover Seen / Approve again is removed. Home keeps
  its verdict chip.
- **AC-d.** The states and their wording are decided in the core; Qt only
  presents them.

Load `frontend-design:frontend-design` before the Qt work and say so in the
report. The owner's look on a frozen build (Fixture agent only, a fresh short
session, one Accept/Reject per visible criterion) is filed, not waited on.

## Intended verification

Core tests for the review's follow-up states and words. A composed course on
the fixture GitHub server: the review of an approved pull request with new
commits offers Seen beside Approve, Seen asks GitHub for nothing and the
review then reads up to date; a dismissed approval's review reads Approve
again; Home rows offer no Seen or Approve again.
