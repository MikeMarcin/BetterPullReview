---
id: BPR-0491
title: A launched station's name does not bind in peer agent lists
status: Ready
rfc: none
depends_on: []
---

# A launched station's name does not bind in peer agent lists

## Why

`docs/development/remote-stations.md` makes name binding the precondition for
every cross-machine dispatch:

> A row is aspirational until its host is relaunched through
> `scripts/launch_remote_station.py` and the name shows up bound in a peer's
> agent list: dispatch to an unbound name resolves to a silent auto-variant
> and misroutes rather than failing loudly.

On 2026-08-29 the Windows host launched cleanly with

```text
python3 scripts\launch_remote_station.py win --machine jaegaw --read-only
```

and reported `Ready · BetterPullReview · windows-lane`. `bpr-win-jaegaw` never
appeared in the `bpr-main-mac` peer agent list — six polls across several
hours. The launcher is not obviously at fault: it does pass `--name` to
`claude remote-control` ([launch_remote_station.py:217](../../../scripts/launch_remote_station.py)).
What the peer list did show is that every cross-machine entry carries a
free-text conversation title (`Aito linux work`, `Windows-lane worktree and
ready-win cards`), which is evidence that the visible identity is the session
title rather than the `--name` value.

The consequence is not cosmetic. Named dispatch is currently unavailable for
every station, so [[BPR-0311]] and every future platform card must be
hand-pasted. The envelope contract permits hand-paste, so this is a degraded
path rather than a dead one — but the registry documents a binding mechanism
that has never been observed to work, and the documented failure mode is a
silent misroute.

## Required final state

- The relationship between `claude remote-control --name`, the session title,
  and the identity a peer's agent list shows is established from observed
  behavior, not inferred.
- Either a launched station binds under its registry name in a peer list on
  another machine, or `docs/development/remote-stations.md` stops claiming it
  does and prescribes the identity check that actually works.
- The silent-misroute risk is addressed: a dispatcher can tell a bound station
  name from an unbound one before sending, rather than after.
- Whatever the outcome, `scripts/launch_remote_station.py` reports the identity
  a peer will actually see, so an operator can confirm binding from the launch
  output instead of asking another machine.

## Notes

Both halves of the check need a real second machine, but the portable half —
what the launcher passes, and what the registry claims — starts here.
Discovered while dispatching [[BPR-0311]] to `bpr-win-jaegaw`; it does not
block that card's target-native work, which proceeds by hand-pasted envelope.
