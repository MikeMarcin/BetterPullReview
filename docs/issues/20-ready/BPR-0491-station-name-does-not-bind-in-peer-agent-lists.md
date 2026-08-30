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

## Investigation — 2026-08-29, `bpr-main-mac`

Claude Code 2.1.251, macOS 15.7.7, `Michaels-MacBook-Pro.local`.

Three observations, ordered by how well they are established.

**Established: `claude remote-control --help` never returns.** It printed
nothing and was still running after 120 s; it had to be killed. So the flag
contract the launcher depends on — `--name`, `--spawn`, `--continue` — is not
discoverable from the CLI itself, and an operator diagnosing a launch has no
way to ask what the flags mean. This is a usability defect on its own, whatever
the binding cause turns out to be, and it is why this card's first question had
to be answered by reading `launch_remote_station.py` instead.

**Established: two documented setup steps are undone on this machine.**
`docs/development/remote-stations.md` step 3 requires
`"crossSessionInbound": "accept"` in `~/.claude/settings.json` and the removal
of stale `"remoteControlSpawnMode": "same-dir"` from the project entries in
`~/.claude.json`. On `bpr-main-mac` the setting is absent and the stale value is
present for this very project. Both are real drift from the documented state.
Neither is a confirmed cause: the doc scopes both to *worker* machines, and this
host is the integrator. The stale spawn mode is documented as confusing a manual
launch, and no station was launched from this host in this session.

**Hypothesis, not established: the peer list on this host is not live for
cross-machine sessions.** The agent-list contract says other machines' sessions
appear only when Remote Control is connected in the querying session. This is an
ordinary interactive session. Every cross-machine row it shows is either offline
or belongs to another project, and the freshly launched `bpr-win-jaegaw` never
appeared across six polls spanning hours. That is consistent with a stale
directory rather than with a station that failed to register — and it would mean
`remote-stations.md` prescribes a binding check that is only valid from a
Remote-Control-connected session, without saying so.

The reciprocal test decides it and needs the other machine: does `bpr-main-mac`
appear in the *Windows* session's agent list? If it does, registration works and
this is Mac-side visibility, not a launcher defect. If neither host sees the
other, the fault is in registration or in the account directory.

Settings changes are deliberately not applied here. `~/.claude.json` and
`~/.claude/settings.json` are the operator's account configuration, not
repository state, and this card should not silently rewrite them.

## The reciprocal test, and what it falsified — 2026-08-29

The Windows host was asked whether it could see `bpr-main-mac`. It could not,
and it listed its own local peer as `bpr-windows-bc`. Minutes later the Mac
peer list gained two rows it had not held across six earlier polls:

```
bpr-main-mac [b8acfe]      · Remote Control · idle
jaegaw-shiny-fox [fa59e5]  · Remote Control · running
```

That kills the hypothesis recorded above. The peer list **is** live for
cross-machine sessions — it gained entries and reports live idle/running state
— and `--name` **does** bind, because `bpr-main-mac` is present under exactly
its registry name. Neither of the two documented-setup gaps found earlier can
be the cause either, since both are still unrepaired on this host while
`bpr-main-mac` binds anyway. The remaining fact is narrower and stranger: the
Windows launch specifically did not produce a session named `bpr-win-jaegaw`.
The only JaegAW session visible is auto-named `jaegaw-shiny-fox`, and the
Windows host sees itself locally as `bpr-windows-bc` — a name derived from its
checkout directory, not from `--name`.

So the card's question narrows from "does binding work" to "why did binding
work on macOS and not on the Windows launch". Candidates worth separating
before any fix: whether `claude` on Windows is a shim that drops arguments
across `subprocess.run`; whether the interactive `Enable Remote Control? (y/n)`
prompt takes a path that discards `--name`; and whether an already-running
session in that checkout won the name first.

A second thing surfaced that is not this card's subject but should not stay
unrecorded. `bpr-main-mac [b8acfe]` is a real, idle, launcher-started session —
and it is **not** the session doing this work. This session is the ordinary
interactive `betterpullreview-e4`, editing the checkout that station owns. That
is permitted today only by the explicit exception in `remote-routing.md` for
interactive sessions predating the lease contract, held to claim-by-committed-
lane-move, which is how every commit on this card was made. The exception is
documented as something to retire, and the two-sessions-one-checkout state
found here is the concrete argument for retiring it.

## Station identity is checkout-scoped, not session-scoped — 2026-08-29

The operator confirmed that `jaegaw-shiny-fox` is **not** the launcher's
session. It is an ordinary `claude` CLI session started by hand in
`C:/code/git/bpr-windows`, given `/bpr-implement-rfc BPR-0311`. Its name is the
Remote Control auto-name pattern — machine token plus two words — which is what
a session gets when nothing passes `--name`. Only the launcher passes it.

That session answered an identity probe as `bpr-win-jaegaw`, quoting the
station file, the branch, and `read-only-proof`, and it was right to: it read
`.remote-station.json` off the checkout root, which is where the launcher wrote
it. Nothing in the identity guard distinguishes the session the launcher
wrapped from any other session opened in the same directory. Station identity
is a property of the checkout, so an ad-hoc session inherits a station identity
it was never granted, and answers for that station convincingly.

Both processes were live in that one checkout: the launcher holding the writer
lease, and the unwrapped session doing the work. `station_lease.py status`
reported `station-writer: held` with `(no holder record)`, so the guard could
not even name the holder. This is the hole `remote-routing.md` already
describes — "the lock cannot see a session that was never wrapped" — reached in
practice rather than in theory, and reached by the ordinary route of an
operator opening a terminal.

The damage was bounded only by `--read-only`: with `authority: read-only-proof`
in the station file, the identity guard holds the unwrapped session to
read-only, so nothing could reach `main`. Had the station been launched
`lane-commit`, an unwrapped session in that checkout would have believed itself
authorized to commit and push the station branch.

So this card's scope grows by one requirement: binding a station name is not
enough. A session must be able to tell whether *it* is the station, not merely
whether it is standing in the station's directory.

## The likeliest explanation, and the cheapest way to test it — 2026-08-29

Once `jaegaw-shiny-fox` was identified as the hand-started session, the
launcher's own session was missing from every list — and the launcher's banner
says why it might never have existed:

```
·✔︎· Ready · BetterPullReview · windows-lane
    Single session · exits when complete
Code anywhere with the Claude mobile app or https://claude.ai/code?environment=...
```

`claude remote-control --name <n> --spawn session` starts a **server** that
spawns its single named session when a controller connects. Nobody connected to
that environment: the operator worked in a separate terminal instead. If the
named session is created on connection, then there was never a session to carry
the name, and the peer list was right to omit it. The launcher would not be
defective at all — the registry's binding check would simply describe a state
that cannot exist until someone opens the environment.

This fits every observation better than the alternatives: the launcher
demonstrably worked in each part that does not need a live session (station
file written, `station_id` durable, writer lease taken, Remote Control
serving), and `bpr-main-mac` binds under its registry name on a host where a
controller *has* connected.

The test is cheap and decisive, and needs no code change: open the launcher's
environment URL once, let the session spawn, then re-check a peer's agent list
for `bpr-win-jaegaw`. If it appears, this card is a documentation defect —
`remote-stations.md` must say that a `--spawn session` station does not bind
until first connection, and the launcher should say so at launch rather than
printing `Ready`. If it does not appear, the fault is in registration and the
card keeps its current scope.

Until that test runs, this remains a hypothesis. It is recorded here so the
next session does not re-derive it, and it does not change the two established
findings above — `claude remote-control --help` still never returns, and
station identity is still checkout-scoped.
