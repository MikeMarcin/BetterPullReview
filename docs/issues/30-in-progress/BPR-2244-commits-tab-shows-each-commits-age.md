---
schema: "implement-rfc.card.v1"
id: "BPR-2244"
title: "The Commits tab shows how old each commit is"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"enhancement","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner request, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2244--BPR-2244--3d9221f87624.json","related":["BPR-1407","BPR-1575"],"acceptance":["BPR-2244:read","BPR-2244:row","BPR-2244:look"]}
---

# BPR-2244: The Commits tab shows how old each commit is

## Request

The owner, 2026-11-17: "In the commits tab, it would be nice to see how old
the commit was. Maybe days ago like we have for the home screen on PR age."

## Scope

- The GitHub reads (recorded and paged) keep each commit's committer date
  (`commit.committer.date`) as seconds; a record without one, or with one in
  another shape, leaves the age unknown and refuses nothing.
- The commit filter's projection carries it on each row.
- Each Commits tab row says the age on its detail line, in the words the
  window already uses for ages ("just now", "2 min ago", "3 hr ago",
  "2 days ago", `refresh_age_words` over the core's `refresh_age_at`). The
  clock is the host's (the Home clock the window is given); core reads none.
- The row's accessible name includes "committed 2 days ago"; its tooltip
  names the exact local date and time.

## Acceptance

- `BPR-2244:read`: both GitHub readers keep the committer date, and an
  absent or malformed date leaves it unknown without failing the read.
- `BPR-2244:row`: a row shows the age to the host's clock, its accessible
  name and tooltip carry the age and exact date, and a later clock updates
  the words without resetting the model.
- `BPR-2244:look`: the owner judges the row's look in a frozen build
  (attended; pending).

## Evidence

Recorded at the end of the slice.
