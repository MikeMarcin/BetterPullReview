---
schema: "implement-rfc.card.v1"
id: "BPR-2239"
title: "The freeze tool's own check survives the next-push start"
status: "Done"
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

## Added 2026-11-16: the check also fails when the look opens on a file

Seen after BPR-2231/BPR-2232 (look report-keeps-v001): the freeze tool's
`launch.py --check` also fails when the look is frozen with `--open-file`
(a file, rather than the pull request). It reports "What changed since my
review" as not offered. The same look opened on the pull request passes.
This is a second, deterministic-looking failure of the same check, separate
from the intermittent next-push exit above; whether the check expects the
control in a state the file view does not show, or the file view hides it, is
not yet known.

- **AC-d.** `freeze` with `--open-file` passes its own check, or the check
  states why the control is legitimately absent on a file and does not fail
  the look; with a test or a recorded run on a fixture that holds the surface
  (for example `returning-review-v1` with `--open-file src/engine/renderer.cpp`).


## Done 2026-11-17

Implementation `9a94946b5` on the lane.

- **AC-a**: met. A failed `launch.py --check` keeps its `check.*` scratch
  and prints `CHECK_KEPT:`, `CHECK_LOG:` (the failing start's log) and the
  log's last 40 lines (`CHECK_LOG|`); the freeze prints the check's stderr
  (`CHECK_STDERR|`). `freeze_attended_build_contract`: an exiting image's
  check keeps its scratch and prints `CHECK_LOG| STARTUP_FAILED`; a passed
  check leaves no scratch.
- **AC-b**: the race is closed; its cause was not read from a log. The
  launcher's `stop()` waited only for the first start's leader, and the
  agent runs in a session of its own (`setsid`, `agent_process_launcher`),
  out of reach of the group's SIGTERM, so a helper of the first start could
  still hold the scratch data when the next push began. `stop()` now records
  everything the start began (libproc's child list, which a test's sandbox
  allows where `ps` is refused), ends the group, and waits up to 20 s until
  the group and every recorded process have exited (killing by PID only what
  is left, reported as `CHECK_NOTE`). The contract's lingering stand-in (a
  helper in its own session that writes into the review data 2 s after the
  first start) passes, and fails with the old `stop()` ("raced the first
  start's helper"). The intermittent exit itself did not recur in eight
  next-push checks under load, so AC-a's log is what will name it if it
  ever does.
- **AC-c**: met. `freeze --next-push` on `returning-review-v1` opening
  `src/engine/renderer.cpp` (root `bpr2239-next-push-check.qq37ph2t`, frozen
  from `9a94946b5` while another freeze built and GUI courses ran) passed its
  own check, then six more `launch.py --check` runs in a row passed (first
  head `b20cb443`, next push `cd24836c`, 17 Fixture agent asks each) under
  load averages of 47 to 86 from concurrent builds and course loops. The
  root was removed with `freeze_attended_build.py remove`.
- **AC-d**: met. A look on a file with no push failed because the check
  required What changed since my review, which a look with no earlier review
  rightly does not offer, and asked at `renderer.cpp` whatever the look's
  file. The launcher now sets `BPR_ATTENDED_FIXTURE_AGENT_CHECK=file` there:
  the app asks every other ask at the look's own file (line 1) and names What
  changed as skipped, and the check prints why it is absent. At the next push
  it sends `1` as before. Recorded runs: `returning-review-v1` with
  `--open-file src/engine/renderer.cpp` and no push (12 asks, What changed
  skipped, report=1, menus=6); `commit-selection-v1` with
  `--open-file src/app.cpp` (nine asks); and BPR-2244's freeze
  (`commits-age-v001`), whose own check passed on that file.
  `application_fixture_agent_every_ask` (the next-push course) still passes.

Also: `freeze_attended_build_contract` was red at lane HEAD (its stand-in
lacked `menus=`); it passes. The production app source pin was resealed; it
was already stale at HEAD (BPR-2245, BPR-2247 to BPR-2249 and BPR-2251), and
the new digest was computed on an export of HEAD plus `main.cpp`. Tidy on
`main.cpp` passed; the documentation gate passed.
