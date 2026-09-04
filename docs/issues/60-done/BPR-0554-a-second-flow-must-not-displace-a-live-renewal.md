---
id: BPR-0554
title: A second flow must not displace a live renewal at all
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0553]
---

# A second flow must not displace a live renewal at all

## Why

Recorded by the worker that delivered [[BPR-0553]], which closed the case
where an abandoned second sign-in silently dropped the first session's
renewal. Keeping the *binding* turned out to be only half of "exactly as it
was": the host owns one timer, and a second flow's poll displaces the armed
renewal while it runs. BPR-0553 restores it at the flow's terminal
(`restore_displaced_renewal`, gated on a terminal other than `authorized`,
re-armed through `schedule_refresh` so the wait recomputes from
`AuthorizedAt` rather than firing in the past).

That is strictly better than losing it, but a renewal that falls due
*during* the second flow still fires late — by up to the flow's duration,
and a device code lives 900 seconds against a 300-second renewal lead, so
the access token can expire before the renewal it was waiting for is
re-armed. There is also a transient dishonesty: while the second flow polls,
the observation still reads `scheduled` with no timer behind it.

## Required final state

- A live renewal is not displaced by a second flow: the poll and the renewal
  do not contend for one timer slot, whether by a second timer port, a
  scheduler that holds both deadlines, or the renewal being re-armed
  immediately rather than at the flow's terminal — chosen with a recorded
  reason.
- A renewal falling due while a second flow polls fires on time.
- The observation never reports `scheduled` with nothing armed.
- Host tests cover a renewal due mid-flow and the observation during a flow,
  each failing before the fix.

## Non-goals

- Re-litigating [[BPR-0553]]'s release point, which stands.
- Changing the poll's own timing contract.

## Landed — 2026-09-04

At `5590b406`, reproduced on the merged tree.

**The shape: a scheduler inside the host, holding both deadlines over the one
existing timer port.** `PollDueAt` and `RefreshDueAt` are absolute monotonic
instants; the port holds whichever is earlier; `ArmedDueAt` records what the
timer was armed for, so a deadline that moves while armed is re-armed rather
than left on a stale expiry; and the seconds conversion rounds up so a poll is
never armed inside the interval the server asked for. `schedule_timer` sets a
deadline and arms the earliest, `drop_deadline` releases one and hands the
timer to the other, and `on_timer` clears only the spent deadline, dispatches,
then re-arms whatever waited. [[BPR-0553]]'s `restore_displaced_renewal` is
gone: a terminal now simply drops the flow's deadline, because there is
nothing left to restore.

**Why not the other two.** A second timer port answers with a second object
every platform must compose, adapt, cancel, and shut down, plus a second
lifetime in quiescence — for a question one deadline comparison settles inside
the host — and it would have required a composition and adapter change. Re-arming
immediately is not a fix with one timer at all: it is the same contention
pointed the other way, the renewal taking the timer back from the poll so the
flow never ends, which is exactly why BPR-0553 gated its restore on the
terminal. The port's contract is unchanged; the only header edit is its doc
comment, whose claim that the two schedules never overlap this change made
untrue and which now says the overlap is the host's to resolve.

**Three things fell out, all toward the card.** A cancelled second flow keeps
the renewal, the case BPR-0553 recorded as untestable. `request_reenrollment`
no longer takes a live flow's poll timer, being split into a state-only report
plus a refresh-deadline drop. And releasing the refresh binding drops only the
refresh deadline. Both of the port's refusal answers are preserved: a refused
renewal still reports re-enrollment, a refused poll still fails with a timeout.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A renewal due during a second flow fires on time | `application_device_authorization_refresh` — new case, renewal due two seconds ahead of the flow's first poll | fires mid-flow, the flow is untouched, the poll returns for its last three seconds, `access-2` enrolled; pre-fix `FAILED: the timer holds the renewal, not the poll behind it` | this commit |
| The observation never says `scheduled` with nothing armed | the second new case | passes; pre-fix fails on the timer being unarmed while the observation read `scheduled` — the transient dishonesty caught directly | this commit |
| BPR-0553's two cases still hold | `test_a_second_flow_that_fails_leaves_the_first_renewal_armed`, `test_a_denied_second_flow_gets_the_first_renewal_back_on_the_timer` | unedited and passing; the denied case's remaining lead still reads 3295 s, now because the renewal was never displaced rather than because it was restored | this commit |
| Every device, vault, core, contract, and public-path row | `ctest --preset dev -j 2` over the nineteen-row set | 19/19 | this commit |
| Boundaries, tidy, format | `check_boundaries`; `tidy.py` over both touched sources; `clang-format --Werror` | exit 0; exit 0; exit 0 | this commit |

No composition or port signature changed, so neither digest pin moved.

**Left open.** The three collateral changes are covered only by the
nineteen-row set, with no test naming each. The tie rule — equal deadlines go
to the renewal — is a deliberate choice no test exercises. The timer takes
whole seconds, so a queued deadline can be armed up to a second late; that
precision was never available through the port. Only clang compiled this;
Windows and Linux see it at the next replay.

