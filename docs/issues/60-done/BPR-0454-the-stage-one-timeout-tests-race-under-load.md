---
id: BPR-0454
title: The stage-one timeout tests race their own helper under matrix load
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The stage-one timeout tests race their own helper under matrix load

`designated_live_macos_capsule_contract` failed once during a full parallel
`dev` matrix on 2026-08-25, in two of [[BPR-0449]]'s stage-one cleanup cases:

```
ERROR: test_stage_one_timeout_terminates_cooperative_process_group
FileNotFoundError: .../fixture/bprh6xjef0w/process-id

FAIL: test_stage_one_timeout_kills_term_ignoring_process_group
assertEqual(delivered, [SIGTERM, SIGKILL])
```

It passed 3/3 immediately afterwards in isolation, and passed again in a second
full matrix on the same commit. So it is a flake, not a regression — but a flake
in a process-termination test is worth more than a rerun, because the thing it
is flaky about is whether cleanup actually happened.

Found during [[BPR-0418]], which is unrelated work and whose change set touches
nothing this fixture reads.

## The race, as far as the evidence goes

Both cases pin `STAGE_ONE_TIMEOUT_SECONDS` to 1.0 and start a helper that is
supposed to write its own pid to `process-id` before blocking
(`tests/scripts/designated_live_macos_capsule_tests.py:482` and `:504`).

- The first failure is the helper losing that race outright: the deadline fired
  and the assertion read `process-id` before the child had written it. That is a
  test-side ordering assumption, not a product defect — but it means the case
  can pass without ever having proved the child was gone, because
  `assert_process_gone` never ran on a real pid.
- The second is tighter and more interesting: `STAGE_ONE_TERM_GRACE_SECONDS` is
  0.2, and the case requires exactly `[SIGTERM, SIGKILL]`. Under load, 200 ms
  between the two signals is a scheduling coin flip.

## What this card should not do

Do not fix it by widening the grace periods until it stops failing. The short
grace is what makes the escalation observable at all, and a version that waits
long enough never to race also never proves the escalation happens promptly.

The likely honest repair is to make the helper's readiness explicit — have the
case wait for `process-id` to exist before starting the timed section, so the
deadline measures termination rather than startup — and to assert the signal
*sequence* rather than an exact list captured within a fixed window.

## Required final state

- The two cases fail when the escalation is wrong and do not fail when the
  machine is busy.
- Whatever readiness the test now assumes is waited for rather than assumed.
- The card states how many consecutive runs under parallel load were used as
  evidence, since one green run proves nothing about a flake.

## Verification

```sh
ctest --preset dev --output-on-failure -R '^designated_live_macos_capsule_contract$'
ctest --preset dev --output-on-failure      # under full parallel load
```

## Done — 2026-08-26

### The diagnosis held, with two corrections to its wording

Both failures are one cause: nothing waited for the child. `blocking_helper`
wrote a `#!/usr/bin/python3` script and `run_stage_one` started its deadline
at the fork, so the 1.0-second window had to cover interpreter startup. Under
a loaded matrix it does not.

Two details in the card above are not literally what the code does, and the
repair is built on the corrected version:

- The first case does **not** silently pass with `assert_process_gone` skipped.
  It raises `FileNotFoundError` — a hard error, which is how it was noticed.
  The real defect is weaker but still real: whether the cleanup claim is
  checked at all was decided by interpreter startup latency, and the failure it
  produced named a missing file rather than anything about termination.
- `[SIGTERM, SIGKILL]` cannot come back *reordered*. `terminate_process_group`
  makes both calls sequentially on one thread, and the recorder is a
  `side_effect` interceptor on `capsule.signal_process_group`, not an
  asynchronous observer — it cannot drop, coalesce, or delay a call that
  happened. The only reachable wrong answer is the short prefix `[SIGTERM]`,
  which is exactly what a child signalled before it installed `SIG_IGN`
  produces.

### Why the readiness wait could not stay inside `run_stage_one`

The card's prescription — wait for readiness before the timed section — has no
home on the `run_stage_one` path. That function owns the spawn, and its
deadline starts at `subprocess.Popen`, so nothing reached *through* it can wait
for the child first. Making it possible would have meant patching
`subprocess.Popen` out from under production code, which buys a readiness
barrier by faking the spawn.

So the two cases were split by what each can prove without assuming anything
about timing:

- `test_stage_one_timeout_terminates_cooperative_process_group` keeps driving
  the real `run_stage_one` and owns the **wiring**: the deadline fires, raises,
  and leaves no live process group. It now takes the group identity from the
  recorded `signal_process_group` call instead of from a file the child has to
  survive long enough to write, so it depends on nothing the child does after
  it is forked. A cooperative child dies on SIGTERM whether or not it finished
  starting, which is why this case needs no barrier at all.
- `test_stage_one_cleanup_kills_a_term_ignoring_process_group` — renamed,
  because it no longer goes through the timeout — owns the **escalation**. Its
  claim requires the child's `SIG_IGN` handler to be installed before the
  graded window opens, so it spawns the child itself, waits for a readiness
  marker, and then calls `terminate_process_group`. The marker is created after
  the handler is installed, so its existence is the proof; a bounded 60-second
  wait replaces an assumption about how fast an interpreter starts. That bound
  is a readiness bound, not a grace period.

`STAGE_ONE_TERM_GRACE_SECONDS` stays at 0.2. Widening it was the one repair the
card ruled out, and the interval is now asserted instead of assumed: SIGKILL
must follow SIGTERM by at least half the grace. The assertion is one-sided, so
load can only push it further from failing, and it catches the regression an
exact-list assertion cannot — an escalation that fires immediately still
delivers `[SIGTERM, SIGKILL]`.

### The tests fail when the claim is false

Four mutations of `scripts/designated_live_macos_capsule.py`, each run against
the repaired cases and then reverted. The production file is byte-identical to
`HEAD` afterwards.

| mutation | expected | result |
|---|---|---|
| escalation removed — no `SIGKILL` after the term grace | escalation case fails | **fails**, `CapsuleFailure: stage-one process leader could not be reaped` |
| term grace not waited out — `term_deadline = time.monotonic()` | escalation case fails on the interval | **fails**, `4.2e-05 not greater than or equal to 0.1` — and the cooperative case fails too, on `[SIGTERM] != [SIGTERM, SIGKILL]` |
| deadline path no longer calls `terminate_process_group` | cooperative case fails | **passes** — see below |
| both cleanup layers removed | cooperative case fails | **fails**, `[] != [SIGTERM]` |

The third mutation passing is not a weak test. `run_stage_one` cleans up in two
places — the deadline handler and the outer `except BaseException` — and with
only the first removed the second still terminates the group. The case asserts
that cleanup *happened*, which it did. Removing both layers fails it. Worth
recording because a future reader will try that same one-line mutation and draw
the wrong conclusion from it.

### Evidence under load

One green run proves nothing about a flake, so:

| condition | runs | result |
|---|---|---|
| both cases, 28 spinning processes on 14 cores | 30 consecutive | 30/30 pass |
| both cases, 56 spinning processes on 14 cores (1-minute load average 54 → 121) | 30 consecutive | 30/30 pass |
| `designated_live_macos_capsule_contract` in isolation | 1 | pass, 25.6 s |
| `designated_live_macos_capsule_contract` inside the full parallel `dev` matrix at `-j 14` — the exact condition that produced the flake | 1 | pass, 34.96 s (matrix 217/218; see below) |

### One leaked helper, and the bound added because of it

A blocked `helper.py` was found still running late in this work, orphaned to
PPID 1, holding a `--probe-launch` command line — so it came from the
cooperative case's `run_stage_one` spawn rather than from the escalation
case's own. It was killed.

Its origin is most likely the deliberate `both cleanup layers removed`
mutation above, which by construction leaves the child alive; the timestamps
are consistent with that window and not with any other. That was not proven,
so the repaired case was tested for it directly instead: 20 consecutive runs
of the cooperative case alone under 40 spinning processes produced **20 passes
and zero surviving helpers**. The repaired case does not leak.

The fixture helper now blocks for a bounded 300 seconds rather than forever.
Every case kills it within seconds, so the ceiling changes nothing any test
observes; what it removes is the possibility that a failing case — or a future
mutation run against the cleanup under test — leaves a blocked interpreter
running until the machine is rebooted.

### One failure in the matrix, and it is not this card's

The full `dev` matrix at `-j 14` returned **217 of 218**, exit 8. The single
failure is `application_epoch_maintenance_public_path`, with
`EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK process failed (1)` — the same
symptom [[BPR-0447]] records. This card's change is a Python test file and
cannot reach a C++ end-to-end contract, so it was not absorbed here.

Following it up moved that card rather than adding one. BPR-0447 was filed as
Windows-only and sat in `21-ready-win/`, unstartable from this host. It
reproduces here: green in isolation, two of three failures under 40 spinning
processes, and every marker the card recorded from Windows identical. The
trigger is parallel load, not a platform, so BPR-0447 is now portable and in
`20-ready/`.
