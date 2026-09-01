# Remote Stations

Machine setup for running BetterPullReview RFC work across computers without
mixing project, platform, or branch context. The contract a session follows
lives in `.agents/skills/implement-rfc/references/remote-routing.md`; this
document is the operator-facing half: what to provision on each machine and
how to launch it.

## The shape

One **station** per platform role per machine, each in its own dedicated
checkout on its own station branch, hosted by its own `claude remote-control
--spawn session` server with a deterministic name (or, for Codex-orchestrated
work, a Codex session launched directly in the same checkout under the same
lease). The **project integrator** — the session on whichever host you are
driving — is the only writer for `main`, `docs/issues/NEXT_ID`, ID
reservations, and the board index, and the only merger of station branches.

Platform policy: Windows work runs in a native-Windows station, Linux work in
a WSL2 or native-Linux station, macOS work in a native-macOS station. JaegAW
therefore hosts **two** stations — native Windows and WSL2 Ubuntu — with two
separate checkouts (the WSL2 one on the ext4 filesystem, not `/mnt/c`). This
supersedes the earlier one-WSL2-shell-drives-MSVC arrangement recorded in
BPR-0472/0475/0476: the Windows proofs move into the native Windows session.

Board lanes `21-ready-win` / `22-ready-mac` / `23-ready-lin` are the
per-platform queues; a station reads only its own platform's lane plus cards
explicitly dispatched to it.

## Station registry

The authoritative list of names in use. Add a row before launching a new
station; never reuse a name for a different checkout.

| Station name | Machine | Environment | Branch | Status |
|---|---|---|---|---|
| `bpr-main-mac` | MacBook Pro | macos | `main` | provisioned 2026-08-29 |
| `bpr-win-jaegaw` | JaegAW | windows-native | `windows-lane` | launched 2026-08-29 (`--read-only`); station file written and writer lease taken, but **the name does not bind** — see [[BPR-0491]] |
| `bpr-linux-jaegaw` | JaegAW (WSL2) | wsl2 | `linux-lane` | **unprovisioned** — host still runs free-text sessions |

A row is aspirational until its host is relaunched through
`scripts/launch_remote_station.py` and the name shows up bound in a peer's
agent list: dispatch to an unbound name resolves to a silent auto-variant and
misroutes rather than failing loudly.

That check has never once succeeded, and 2026-08-29 is when that stopped
being a theory. `bpr-win-jaegaw` launched cleanly — station file written,
writer lease taken, Remote Control serving — and its name never appeared in
the `bpr-main-mac` peer list. The Windows host could not see `bpr-main-mac`
either. The station was reached only by probing the one JaegAW session that
was visible, under the auto-generated name `jaegaw-shiny-fox`, which turned
out to be a hand-started CLI session rather than the launcher's.

Two things follow, and until [[BPR-0491]] resolves them, dispatch here is by
hand-pasted envelope or by probing an auto-named session and confirming its
identity before sending anything binding.

- A visible name is not evidence of which process it is. Confirm identity by
  asking for `.remote-station.json`, the toplevel, the branch, and `HEAD`
  before dispatching, and treat the answer as identifying the *checkout*.
- `.remote-station.json` lives at the checkout root, so every session opened
  in that directory reads it and answers as that station — including one the
  launcher never wrapped and the writer lease cannot see. Launching
  `--read-only` is what bounds that today: it holds such a session to
  read-only work no matter what it believes it is. Retire the current free-text sessions
("BPR windows scope routing", "BPR linux work") as their stations come up.
Add machine suffixes as soon as one platform exists on two machines.

Same-machine wrinkle: a station's `--name` is its cross-machine and
claude.ai/code identity, but peers on the *same* machine see the pre-created
session under a local auto-generated name. Same-machine dispatch therefore
addresses the local name from the agent list; the deterministic name is
authoritative everywhere else, and the station file is authoritative always.

## Per-machine setup

1. **Checkouts.** One checkout per station, always. On JaegAW: the native
   Windows checkout (e.g. `C:\code\git\bpr-windows`) on `windows-lane`, and a
   separate WSL2 clone (e.g. `~/code/bpr-linux`, ext4) on `linux-lane`.
   Create a station branch from `origin/main` the first time:

   ```sh
   git clone https://github.com/MikeMarcin/BetterPullReview.git bpr-linux
   git -C bpr-linux switch -c linux-lane origin/main
   ```

2. **Credentials.** Each station environment gets its own push credential,
   authorized for its station branch only (a deploy key or a scoped PAT for
   GitHub). Until a station's credential lands, launch it with `--read-only`
   (authority `read-only-proof`): it builds, tests, and reports, and its
   evidence returns through the reply and the tracker, exactly as the
   BPR-0472 protocol does today. The WSL2 side currently has no origin
   credential — both JaegAW stations start as `read-only-proof`.

3. **Claude settings.** On each worker machine, set
   `"crossSessionInbound": "accept"` in `~/.claude/settings.json`, and remove
   any stale `"remoteControlSpawnMode": "same-dir"` from the project entries
   in `~/.claude.json` (the launcher passes `--spawn session` explicitly, but
   the stale value should not survive to confuse a manual launch).

4. **Launch.** From the station checkout, with the host's own Python —
   `py -3` in a native-Windows shell (PowerShell or cmd), `python3`
   everywhere else:

   ```sh
   py -3 scripts/launch_remote_station.py win --machine jaegaw --read-only
   python3 scripts/launch_remote_station.py linux --machine jaegaw --read-only
   python3 scripts/launch_remote_station.py main             # the integrator itself
   py -3 scripts/launch_remote_station.py win --continue     # reattach after restart
   python3 scripts/launch_remote_station.py linux --no-server # Codex: provision only
   ```

   The launcher refuses a wrong repository, wrong branch, or a role that
   does not match the host (WSL2 counts as Linux, native Windows as
   Windows), writes the untracked `.remote-station.json` (keeping its
   durable `station_id` across relaunches), and starts the server under the
   exclusive writer lease — `station_lease.py` in the skill pack — so a
   second writer in the same checkout is denied mechanically. For Codex it
   prints the lease-wrapped launch command instead.

5. **Driving.** Every station server appears at claude.ai/code and in the
   mobile app; driving one directly from your phone is normal operation. The
   session's identity guard still applies: ask a station for out-of-scope
   work and it names the right station instead of doing it.

## Windows hosts: Smart App Control must be off

A Windows station cannot run the platform verification courses while Smart App
Control is enforcing, and it is **on by default on a clean Windows 11
install**. Check before provisioning:

```powershell
(Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy').VerifiedAndReputablePolicyState
(Get-CimInstance -Namespace root\Microsoft\Windows\DeviceGuard `
    -ClassName Win32_DeviceGuard).UsermodeCodeIntegrityPolicyEnforcementStatus
```

`1` and `2` mean enforcing. The symptom is a configure that dies around six
minutes in with `MSB8066` and exit code `1073751882`, and a run of the shipped
artifact failing with `WinError 4551`, "An Application Control policy has
blocked this file". Smart App Control refuses every freshly linked unsigned
executable until a reputation verdict lands — 15–40 minutes, sometimes longer.

What makes it fatal rather than merely slow is that the refused image is
`katehighlightingindexer.exe`, a build tool the syntax-highlighting dependency
generates and runs during configure. It is third-party, it is relinked from
source in every fresh dependency cache, and this project cannot sign it. The
courses require `--fresh`, so the flag that makes the evidence trustworthy is
the flag that guarantees the run cannot finish (BPR-0493).

There is no narrower fix, and each of these was checked rather than assumed:

- Smart App Control has no allowlist. Its UI is On/Off only, and the active
  policies under `CodeIntegrity\CiPolicies\Active` are Microsoft-signed WDAC
  policies.
- "Run anyway" is SmartScreen, a different subsystem, and does not affect
  code-integrity blocking.
- Defender's `-ExclusionPath` is also a different subsystem and does not
  affect it.
- A Dev Drive does not help either, for the same reason: its benefit is
  Defender antivirus performance mode, while this is kernel code integrity,
  enforced regardless of volume.

So provisioning a Windows station means turning Smart App Control off, in
Windows Security under App & browser control. **That is irreversible** —
restoring it requires a Windows reset — and it is a permanent reduction in
that machine's protection, so it is a decision for whoever owns the machine
and not one an agent makes or is asked to make. Prefer a dedicated build host
over a personal machine for exactly this reason.

## Dispatch and merge in one paragraph

The integrator reserves an ID block on `main`
(`python3 scripts/allocate_issue_id.py --count 8 --reserve-for
bpr-win-jaegaw`), commits counter + receipt, pushes, and sends the station a
`remote-task` envelope quoting the reservation commit (format in
`remote-routing.md`). The station validates the envelope, works only its own
platform queue, commits and pushes only its station branch (or, read-only,
returns evidence in the reply), and answers with a `remote-reply` envelope.
The integrator merges station branches serially, running
`python3 scripts/verify_documentation.py` after each, and confirms any
proposed `Auto-Verified` status on `main`.

## Acceptance checks that need real worker machines

Deferred from local verification; run once per newly provisioned machine:

- the station credential pushes its station branch and is refused on `main`;
- the station appears in another machine's agent list under its exact name
  (no auto-variant suffix);
- a well-formed envelope is accepted and a mismatched one refused by name;
- the mobile app can drive the station;
- `--continue` reattaches after a reboot with the same station identity;
- on native Windows, `py -3 scripts/launch_remote_station.py` detects
  `windows-native` from PowerShell or cmd with no POSIX shell involved;
  under WSL2 it detects `wsl2` and refuses role `win`.
