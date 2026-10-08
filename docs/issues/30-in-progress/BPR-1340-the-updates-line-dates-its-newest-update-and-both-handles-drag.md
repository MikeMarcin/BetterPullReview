---
schema: "implement-rfc.card.v1"
id: "BPR-1340"
title: "The Updates line dates its newest update, and both handles drag"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1340--BPR-1340--f63619107ddf.json","related_cards":["BPR-0646","BPR-0743","BPR-1129","BPR-1130","BPR-0795","BPR-1330"],"acceptance":["BPR-1340:date-read","BPR-1340:date-reopen","BPR-1340:date-fallback","BPR-1340:drag","BPR-1340:keyboard","BPR-1340:composed","BPR-1340:gates"]}
---

# BPR-1340: The Updates line dates its newest update, and both handles drag

## Owner notes

Owner, chat, 2026-10-08, verbatim: "I have a review that had a couple of
updates and the updates timeline populated but the last mark says date
unavailable and I seem to drag either review head to view different versions".
Read as "I can't seem to drag".

## What exists

- RFC-BPR-0028's Updates line ([[BPR-0646]], [[BPR-0743]], RFC-BPR-0029's
  occurrence evidence) draws one node per update: a force push carries GitHub's
  event time ("Event time"), an update this application observed carries its
  head's committer time ("Commit date"), read from the local Git cache by
  `enrich_commit_dates`. An update with neither said "Date unavailable", and
  `core::epoch_occurrence_less` sorts undated updates after every dated one, so
  an undated update is always the last mark.
- The committer time was read with `git show --no-patch --format=%ct`. `show`
  still diffs the commit against its parent with `--no-patch`. A sparse
  acquisition (`git_smart_acquisition_mode::sparse_commit_trees`, the GitHub
  CLI path for any pull request inside the read limits) stores each wanted head
  with its trees but without its ancestors, so the read failed with "unable to
  parse commit <parent>" and every observed update was recorded undated.
- `enrich_commit_dates` runs on the open of a review and after a new head's
  commit. A new process has no cache open at the open, and only a new head's
  commit opened one, so an update recorded undated stayed "Date unavailable"
  until the author pushed again.
- The two handles are brackets on the line. The whole-PR pair, the default,
  puts the left handle on the newest update's base, seven pixels left of the
  right handle on its head. `epoch_line::handle_at` took the right handle
  whenever a press was within seven pixels of it, so a press on the left
  handle took the right one, and the left handle was a sliver to grab. A drag
  that started on the left handle moved the right handle.
- A step or a drag starts a comparison, and while it resolves the readiness
  line stands in the Updates line's place ([[BPR-0795]], [[BPR-1130]]).
  Hiding the focused line handed focus to whatever Qt found next, so after one
  Left or Right the next key went elsewhere. Alt+1 to Alt+3 (Whole PR, Previous
  upload, My last reviewed) are window shortcuts and were not affected.
- [[BPR-1330]]'s resize-cursor hold is installed only on the summary and drawer
  seams; it eats no event and is not on the line.

## Scope

- **Date read.** The committer time is read with `git log --no-walk
  --format=%ct`, which reads the one commit and nothing it names.
- **Date on reopen.** The open of a review opens the actor's cache when an
  update has no date, so the date is read at the unchanged head; a refusal
  leaves the date unavailable and the open unchanged.
- **Fallback.** An update with neither an event time nor a readable commit date
  (its commit gone from the cache) is labelled "First seen <date>", the time
  this application first saw it, with "Push and commit dates unavailable" in its
  tooltip. The review bar's dates use the same date.
- **Drag.** The handle nearer a press is the one taken; a press exactly between
  them, or on a stop both share, takes the right one, so a pair on one node can
  still be pulled apart.
- **Keyboard.** When the readiness line takes the focused line's place, the line
  takes focus back when it returns, unless focus was moved elsewhere meanwhile.

## Acceptance

- `BPR-1340:date-read`: `git_process_contract` reads the committer time of a
  head stored with its tree and without its parent; it fails against the
  `show` command.
- `BPR-1340:date-reopen`: `application_epoch_host` reopens, in a new host, a
  review whose update was recorded undated, at the same head, and the update is
  dated from its commit with no epoch appended; it fails before the change.
- `BPR-1340:date-fallback`: `qt_epoch_comparison_scale_*` labels an update with
  no date "First seen <date>"; it fails before the change.
- `BPR-1340:drag`: `qt_epoch_comparison_scale_*` drags the left and the right
  handle off the whole-PR pair's shared node and each moves its own side; it
  fails before the change.
- `BPR-1340:keyboard`: `qt_epoch_comparison_scale_*` keeps the keyboard on the
  line across a resolve and leaves focus the reviewer moved; it fails before
  the change.
- `BPR-1340:composed`: `application_epoch_update_line_public_path` drags both
  handles and steps by key through the composed window on the recorded pull
  request with three force-push updates, each time waiting for the published
  comparison, and reopens an undated ordinary update at the same head and
  reads "Commit date"; it fails before the change.
- `BPR-1340:gates`: the neighbouring epoch courses, the format check and the
  documentation gate pass.
