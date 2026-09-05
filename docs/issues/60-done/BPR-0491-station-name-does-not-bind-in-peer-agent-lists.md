---
schema: "implement-rfc.card.v1"
id: "BPR-0491"
title: "A launched station's name does not bind in peer agent lists"
status: "Done"
rfcs: []
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

### Evidence against that hypothesis

The launcher's own banner argues the other way and is recorded here so the
hypothesis above is not read as settled:

```
Take this session with you and pick up right where you left off on any device.
The session keeps running on this machine. Press Ctrl+C to stop.
```

That is present tense about a session that exists, not a server waiting to
create one. If a session is genuinely running under the launcher and still does
not appear under `bpr-win-jaegaw` in any peer list, then spawn-on-connect is the
wrong explanation and the fault is in registration or in name propagation after
all — the card's original scope.

So the one-step test stays worth running, but its outcome is genuinely open
rather than expected. Whoever runs it should also record what the launcher's
terminal actually shows: a live session, or a server banner and nothing else.
The operator is the only one who can see that, and neither reading has been
confirmed.

## Settled with a control group — 2026-09-01

The spawn-on-connect hypothesis is confirmed, and the "evidence against it"
was a misreading of the banner. Both JaegAW stations were relaunched today
through the v2 launcher, `lane-commit`, each holding its writer lease:
`claude remote-control --permission-mode bypassPermissions --name
bpr-win-jaegaw --spawn session` (pid 30500) and the same for
`bpr-linux-jaegaw` inside WSL2 (pid 267555). Neither name appears in any
peer list, from this Mac or from the hand-started JaegAW session. Both
station files record `"claude": null`.

What makes that a conclusion rather than a correlation, measured read-only by
the hand-started JaegAW session (existence only, nothing opened):

- Every Aito checkout on that machine has a `bridge-pointer.json` under
  `~/.claude/projects/`; neither BetterPullReview checkout does, on Windows or
  in WSL2. The bridge pointer is what the launcher reads to learn the session
  ID; it appears only when a session exists.
- The Aito stations are launched by the same launcher with identical flags,
  differing only in `--name`, and their names bind in peer lists on both
  machines. `aito-acta-win` is already running as `claude remote-control
  --session-id session_…` — the launcher's resume-by-ID path, reachable only
  after a session was once created and recorded.
- The one difference is that somebody has opened the Aito environments and
  nobody has opened the BPR ones (`env_01LjTC49xVQwzUagnazZMgFf` for Windows,
  `env_01FgmY1otxpN3bNBxM9Lowh1` for Linux).

So `--spawn session` starts a server, not a session; the named session is
created on the first controller connection, and until then there is nothing
for a peer list to show. The 2026-08-29 launch was in exactly this state.

The silent-misroute half of the risk is also settled, by trying it: a
`SendMessage` to `bpr-win-jaegaw` from this session was refused with `No
agent named 'bpr-win-jaegaw' is reachable`. An unbound station name does not
resolve to an auto-variant; it fails loudly, so the pre-send check is the
peer list itself and a name is dispatchable exactly when it appears there bare
and exact. What the auto-variant caution still covers is a *second* session
with the same name, and a hand-started session in another checkout answering
an identity probe as the station because it read the station file — that
happened again today (`jaegaw-curried-spark`, a `main` checkout with two local
commits, correctly declined to act as the Windows lane once asked).

### What changed

- `docs/development/remote-stations.md` stops claiming a launched name binds
  and prescribes the check that works: launch, open the environment once, then
  look for the bare exact name in the peer list; refuse anything that is not
  there. The registry rows carry both environment IDs and the
  "session not yet spawned" state.
- `launch_remote_station.py` reports the identity a peer will see and when:
  `spawn_on_connect_notice` prints at every fresh start that the named
  session is created on the first controller connection, that the name is in
  no peer list until then, and that the station file records `"claude": null`
  meanwhile. The session watcher no longer gives up after 30 seconds — the
  window that guaranteed a station connected later would never have its
  session recorded, and would create a fresh session on every relaunch instead
  of resuming. It now runs until the Claude process exits, and after the old
  30-second grace it releases only the *sibling* worktree pointers while the
  stale target pointer stays aside, so a late connection cannot be recorded
  from an old session ID. Four tests cover the late record, the stop without a
  session, the sibling release, and the notice.
- `references/runtime-compatibility.md` records the rule for the portable
  skill; adapters and the portable manifest are regenerated.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The launcher records a session created after the grace window | `python3 -m unittest tests.test_launch_remote_station` in the skill root | 36/36, including `test_watch_outlives_the_grace_and_records_a_late_session`, `test_watch_ends_without_a_session_only_when_claude_stops`, `test_releasing_siblings_keeps_the_stale_target_hidden`, `test_fresh_claude_announces_spawn_on_connect` | this commit |
| An unbound station name fails loudly | `SendMessage` to `bpr-win-jaegaw` from `bpr-main-mac` while no such session exists | refused: `No agent named 'bpr-win-jaegaw' is reachable` | recorded here |
| The portable skill is regenerated and consistent | `render_runtime_adapters.py` then `validate_portable_agents.py` | see the commit's generated files | this commit |
| The documentation gate holds | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |

Residual, recorded in the registry's acceptance checks rather than kept here:
the positive half — a BetterPullReview station name appearing in this Mac's
peer list after its first connection, and its station file gaining
`claude.session_id` — needs the operator to open each environment once. Every
required final state above is met from observed behavior; that observation
confirms the rule on this project's own names.

Also observed and left as-is: `claude remote-control --help` still never
returns (a hung one from 2026-08-29 is still running on this Mac, pid 37121).
It is a CLI defect, not this repository's.

## The launcher was using the wrong form, and the fix is one flag — 2026-09-01

The product owner pointed at the Aito stations, which "just worked", and
asked what this launcher does differently. The Claude command is
byte-identical in both launchers — `claude remote-control --name <station>
--spawn session` — so the difference was only that the Aito environments had
been opened once. That is not a fix; it is the defect. Four probes on this
Mac, each from a gitignored directory inside the trusted checkout, each
watched for 40–75 seconds for the bridge pointer and the peer list:

| Command | Session created at launch? |
|---|---|
| `claude remote-control --name X --spawn session` | no — "Single session · exits when complete", environment URL, no pointer, absent from every peer list |
| `claude remote-control --name X --spawn session --create-session-in-dir` | no — identical |
| `claude remote-control --name X --capacity 1` (default `same-dir`) | no — "Capacity: 0/1 · New sessions will be created in the current directory" |
| `claude --remote-control X` (the interactive form, under a pty) | **yes** — `X · Remote Control · idle` in this session's peer list within seconds, nobody connected |

The `remote-control` subcommand is a *server*; every mode of it creates
sessions when a controller connects, and the documented `--create-session-in-dir`
default did not change that in these probes. The `--remote-control` *flag* on
an interactive session makes the running process the served session, so the
name exists the moment the process does. A station supervisor already runs
Claude in the foreground for its whole life, which is exactly the shape the
interactive form wants.

So the launcher's fresh path now runs `claude --remote-control <station>
--permission-mode <mode>`; the environment-URL step disappears, the bridge
pointer appears at startup, the session ID is recorded immediately, and the
spawn-on-connect notice above becomes a description of the form the launcher
no longer uses. The two JaegAW stations launched today with the server form
have to be relaunched once through the corrected launcher to get a session.
