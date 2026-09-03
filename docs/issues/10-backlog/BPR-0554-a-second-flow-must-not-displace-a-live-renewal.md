---
id: BPR-0554
title: A second flow must not displace a live renewal at all
status: Backlog
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
