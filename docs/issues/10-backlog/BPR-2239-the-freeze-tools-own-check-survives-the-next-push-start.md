---
schema: "implement-rfc.card.v1"
id: "BPR-2239"
title: "The freeze tool's own check survives the next-push start"
status: "Backlog"
rfcs: []
depends_on: []
metadata: {"reservation":"docs/issues/_reservations/BPR-2239--BPR-2239--fa295f8c59a5.json","origin":"BPR-2219's looks review-report-v002 and v003, 2026-11-16; filed by BPR-2236's worker"}
---

# The freeze tool's own check survives the next-push start

## What happened

`scripts/freeze_attended_build.py freeze --next-push` runs the new root's
`launch.py --check` before it seals the root. For looks review-report-v002
(root frozen from `84bfffc6bd0c`) and review-report-v003 (`37347e7979de`),
that check passed the first head, then failed at the second start, the next
push (`--epoch-fixture-phase reuse` on the same scratch data): the
application exited within seconds. The same `launch.py --check`, run by hand
on the kept root, passed whole both times, and that hand run was recorded as
the launch evidence ([[BPR-2219]]'s card, "Owner look, round 2" and
"round 3").

## What is known

- It did not happen for look tones-v001 ([[BPR-2236]], frozen from
  `87e489594672` on 2026-11-16, same fixture `returning-review-v1`, same
  `--open-file src/engine/renderer.cpp --next-push`): the freeze's own check
  passed the first head, the next push, and the Fixture agent's 17 asks. So
  it is intermittent, not a property of the fixture or the check's order.
- The evidence is gone each time: the check makes its scratch directory in
  the root (`check.*`), writes each start's log there (`single.log`,
  `reuse.log`), and removes the whole directory in `finally`, failed or not.
  The freeze captures the check's stdout and prints it, but drops its
  stderr. So the exiting start's log, which says why, has never been read.
- Differences between the freeze's run and a hand run: the freeze runs the
  check right after the build, unsealed, with stdout and stderr piped, and
  on a machine still loaded by the build (and by other agents' builds); the
  hand run comes later, sealed, on a terminal.
- `stop()` sends SIGTERM to the first start's process group and waits only
  for its leader. Candidates for the second start's early exit, unconfirmed:
  a child of the first start (the fixture agent, a Git or `gh` helper) still
  holding the scratch data's lock or database when the second starts; the
  first start killed mid-write of the epoch fixture data the reuse phase
  reads; a startup timeout under load.

## Scope and acceptance

- **AC-a.** A failed check keeps its scratch directory, and the freeze prints
  the path of the failing start's log and its last lines (and the check's
  stderr), so the cause is read the first time it happens.
- **AC-b.** The cause is found from such a log, and the check's second start
  no longer races the first: the first start's whole process group has
  exited, and its data is quiescent, before the second begins.
- **AC-c.** `freeze --next-push` on `returning-review-v1` passes its own check
  repeatedly under a concurrent build (a loop of several freezes or of the
  check alone).
