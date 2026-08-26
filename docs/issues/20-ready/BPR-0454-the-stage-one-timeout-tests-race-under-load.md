---
id: BPR-0454
title: The stage-one timeout tests race their own helper under matrix load
status: Ready
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
