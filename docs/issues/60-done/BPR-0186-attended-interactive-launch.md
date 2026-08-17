---
id: BPR-0186
title: Attended interactive launch, so a person can inspect the composition
status: Done
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

## Landed, 2026-08-08

`--attended-launch` composes the fixture-backed application, shows the window,
and schedules nothing. Everything else about the composition is unchanged —
same fixture, same window — so what a person inspects is what the automated
courses drive.

It refuses to share the wheel. Combined with a self-test, a render capture, any
of the four probes, a diff-evidence directory, or run with no fixture at all, it
prints `ATTENDED_LAUNCH_CONFIGURATION_FAILED` and exits without composing
anything. The fixture requirement is part of that: without one the composition
takes the real credentialed path, which is not a thing to hold open.

The seam is guarded rather than merely disabled in a tests-disabled build — the
flag does not exist there and neither does the fixture it requires.
`application_composition_contract`, which builds a fresh `BUILD_TESTING=OFF`
Release tree and scans it, still passes.

`application_attended_launch_contract` asserts both halves: the process is still
running past a budget an automated run would have exited at, and each illegal
combination is refused with no capture produced. Shown non-vacuous by scheduling
the run anyway, and separately by removing the refusal.

**A defect in the first draft of that test is worth recording.** With the
refusal removed, a fixture-less attended launch takes the credentialed path and
waits for a person, so the process never exits — and the test raised a
`TimeoutExpired` traceback after two minutes instead of reporting a failure. A
timeout *is* the result there, not an accident, and it now reports as one.

## Still blocked for its original purpose

This card exists so [[BPR-0100]] can be run. It cannot be yet: the window a
person would inspect is still `github_window`, and the shell RFC-BPR-0005
specified is `workbench_window`. [[BPR-0191]] is what changes which window the
application shows, and until it lands an attended inspection would be judging
the wrong surface.

What has changed is that the *mechanism* is no longer missing. When BPR-0191
lands there is a window to hold open.

## Required direct evidence

- The window is composed, shown, and still open after the automated budget
  would have expired.
- Combining it with any automated probe is refused during argument parsing,
  with the refusal named, and no window is shown.
- The Release seam-absence contract still finds no trace of it.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

Unblocks BPR-0100 and every later owner-attended UI inspection.

## Run, 2026-08-08

The product owner launched
`better_pull_review --attended-launch --github-fixture github-com` on the real
macOS composition and reported the window as inspectable. That is this card's
whole claim: before BPR-0191 there was no shell to open, and before this flag a
fixture launch scheduled an automated run and exited while a fixture-less launch
took the real credentialed path. Neither could be looked at.

The only diagnostic emitted was `QSystemTrayIcon::setVisible: No Icon set`,
which is RFC-BPR-0010's notification sink having no icon assigned in a fixture
launch. It is noted rather than fixed here: it belongs to the notification
delivery card, not to whether a window can be inspected.

Judging what is *in* that window is BPR-0100, which is a separate card and
deliberately so.
