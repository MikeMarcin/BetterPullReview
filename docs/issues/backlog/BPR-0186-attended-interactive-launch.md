---
id: BPR-0186
title: Attended interactive launch, so a person can inspect the composition
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0097, BPR-0191]
---

# Attended interactive launch, so a person can inspect the composition

No launch of this application can currently be looked at by a person against
fixture data.

`--github-fixture github-com` composes the fixture-backed application and then
schedules an automated run that drives itself to `shutdown` and exits — see the
`selfTest || renderPath || !fixtureName.empty()` condition at the tail of
`main`. A launch with no fixture goes to the real credentialed path instead.
There is no third option, so every owner-attended UI card on this board has
been waiting on a window that never stays open.

## What this slice does

A launch that composes the fixture-backed application, shows the window, and
schedules nothing — the person is the run.

## What must not be done

Do not make this a mode the production path can reach. It exists to hold open a
window over fixture data under `BPR_BUILD_TESTING`, alongside the probes
already there, and the tests-disabled Release seam-absence contract must keep
passing unchanged.

Do not let it silently coexist with a probe. An attended launch and an
automated run are contradictory instructions about who is driving; refuse the
pair at argument parsing the way the existing probe combinations are refused,
rather than letting one quietly win.

## Required direct evidence

- The window is composed, shown, and still open after the automated budget
  would have expired.
- Combining it with any automated probe is refused during argument parsing,
  with the refusal named, and no window is shown.
- The Release seam-absence contract still finds no trace of it.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

Unblocks BPR-0100 and every later owner-attended UI inspection.
