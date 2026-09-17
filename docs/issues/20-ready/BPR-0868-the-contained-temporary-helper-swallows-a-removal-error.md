---
schema: "implement-rfc.card.v1"
id: "BPR-0868"
title: "The contained temporary helper swallows a removal error that tempfile would have raised"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-sweep-fixes review of BPR-0854 item 1","found_at":"e84eaba2","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0868: The contained temporary helper swallows a removal error that tempfile would have raised

## Defect

`scripts/contained_temporary_support.py` catches `OSError` when it removes its
directory. `tempfile.TemporaryDirectory`, which the courses used before, raises.
So moving a course onto the helper quietly narrows what a failed cleanup does:
what would have failed the row now passes silently.

An `rfc-reviewer` on the station confirmed this and bounded it:

- **On POSIX it cannot turn a failure into a pass.** No course asserts on
  removal, and every course prints its `_OK` marker before the directory is
  removed, so a removal error arrives after the verdict either way.
- **On Windows it can hide something real.** A directory held open by a
  lingering handle — the ordinary Windows sharing case, and the case the
  contained support exists for — would leak instead of failing the row. The
  course would pass and leave the leak in `%TEMP%`.

[[BPR-0854]] item 1 moved thirteen call sites onto this helper, five of which
run inside the Windows AppContainer, so the Windows half of this is now
reachable by more courses than before. It did not create the behaviour.

## Scope

- Decide what a failed removal should mean, and make the helper say it. The
  likely answer is that it should surface: a temporary directory that cannot be
  removed on Windows is usually a handle the course leaked, which is worth
  knowing about and is exactly what a contained-temporary helper is for.
- If some caller genuinely needs tolerance, make that explicit at the call
  site rather than the default for every caller.
- Either way the helper should not be silent: at minimum report the path and
  the error, so a leak is visible in the run's output even if it does not fail
  the row.
- Check this against the Windows runs [[BPR-0854]] still owes for its five
  contained courses; if one of them is already leaking a directory, this card
  is how it would be noticed.

Settle alongside [[BPR-0867]], which is the same helper's interpreter-version
gap.

## Acceptance

- A removal error is either raised or reported by name, and the card records
  which and why.
- No caller depends on silent tolerance, or the ones that do say so at the call
  site.
- The five Windows contained courses from BPR-0854 item 1 run without leaking a
  directory, and a deliberately held handle is shown to be reported.
