# Remote Stations

Machine setup for running BetterPullReview RFC work across computers without
mixing project, platform, or branch context. The contract a session follows
lives in `.agents/skills/implement-rfc/references/remote-routing.md`; this
document is the operator-facing half: what to provision on each machine and
how to launch it.

## The shape

One **station** per workstream checkout per machine. One foreground supervisor
holds the checkout's writer lease and exposes that same durable station through
two provider endpoints with one deterministic name: a Claude Remote Control
session and a persistent Codex task reachable through Codex Managed Remote.
Workstreams are declared in `.agents/skills/implement-rfc/config/project.json`:
`main` is the integrator, and `win`, `linux`, and `mac` are platform-bound
verification lanes on `windows-lane`, `linux-lane`, and `mac-lane`. The
**project integrator** — the session on whichever host you are driving — is
the only writer for `main`, `docs/issues/NEXT_ID`, ID reservations, and the
board index, and the only merger of station branches. Both endpoints may be
connected, but dispatch through one provider at a time.

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

| Station name | Machine | Environment | Workstream | Branch | Status |
|---|---|---|---|---|---|
| `bpr-main-mac` | MacBook Pro | macos | main (integrator) | `main` | provisioned 2026-08-29 under the v1 station file; relaunch through the v2 launcher before dispatching through Codex |
| `bpr-win-jaegaw` | JaegAW | windows-native | win | `windows-lane` | relaunched 2026-09-01 under v2 with `lane-commit` and the writer lease, but with the server form the launcher used until [[BPR-0491]]: **no session exists**; relaunch once through the corrected launcher |
| `bpr-linux-jaegaw` | JaegAW (WSL2) | wsl2 | linux | `linux-lane` | relaunched 2026-09-01 under v2 with `lane-commit` and the writer lease, same server-form launch: **no session exists**; relaunch once through the corrected launcher |

The station file format changed to `implement-rfc.remote-station.v2` on
2026-09-01 (workstream instead of role, plus the Codex task and Claude session
identities the supervisor records). A station still carrying a v1 file keeps
its `station_id` on relaunch; until then the Codex hook guard
(`.codex/hooks.json`) stays inactive for that checkout and says so on stderr,
because a v1 file carries no Codex task binding to enforce.

A row is real once its host is launched through
`scripts/launch_remote_station.py` and the name shows up, bare and exact, in
a peer's agent list — which now happens at launch. The launcher runs the
interactive form, `claude --remote-control <station>`: the running process is
the served session, so the name binds the moment the supervisor starts and
nobody has to open anything. Until 2026-09-01 it ran the `claude
remote-control --spawn session` server instead, and every mode of that
server creates its session only when a controller connects — so every
station launched before the fix sat at "Ready" with an environment URL and
no session, absent from every peer list, and the 2026-08-29 launch of
`bpr-win-jaegaw` was that state misread as a registration failure. The
same-launcher control group that settled it, and the four flag probes that
found the working form, are on [[BPR-0491]]. A station launched before the
fix is relaunched once through the corrected launcher; nothing else changes.

Dispatch is not the silent misroute this document once feared. Sending to a
name that no live session carries is refused loudly (`No agent named
'bpr-win-jaegaw' is reachable`), so the pre-send check is the peer list
itself: a station name is dispatchable exactly when it appears there, bare
and exact. Two cautions survive:

- A visible name is not evidence of which process it is. Before sending
  anything binding, ask for `.remote-station.json`, the toplevel, the branch,
  and `HEAD`, and treat the answer as identifying the *checkout*. A
  hand-started session in another checkout on the same machine appears under
  an auto-generated name (`jaegaw-shiny-fox`, `jaegaw-curried-spark`) and
  will answer.
- `.remote-station.json` lives at the checkout root, so every session opened
  in that directory reads it and answers as that station — including one the
  launcher never wrapped and the writer lease cannot see. Only the session
  the launcher spawned holds the lease; an unwrapped session in a
  `lane-commit` checkout must not edit it.

Retire the free-text sessions as their stations come up. Add machine suffixes
as soon as one platform exists on two machines.

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
   any stale `"remoteControlSpawnMode"` from the project entries in
   `~/.claude.json` (the launcher's interactive form does not read it, but
   the stale value should not survive to confuse a manual server launch).

4. **Launch.** From the station checkout, with the host's own Python —
   `py -3` in a native-Windows shell (PowerShell or cmd), `python3`
   everywhere else:

   ```sh
   py -3 scripts/launch_remote_station.py                    # native Windows station
   python3 scripts/launch_remote_station.py                  # WSL2/Linux station, or the Mac integrator
   py -3 scripts/launch_remote_station.py --continue         # reattach after restart
   py -3 scripts/launch_remote_station.py --fresh-station    # forget this station and start over
   python3 scripts/launch_remote_station.py --no-server      # Codex: provision only
   ```

   That is the whole command. The workstream is inferred: from
   `.remote-station.json` when the checkout was launched before, otherwise
   from the checkout's branch (`windows-lane` is `win`, `linux-lane` is
   `linux`, `main` is `main`). Passing it explicitly is allowed and refuses
   if it disagrees with the checkout. So is the machine suffix: the JaegAW
   stations were registered with `--machine jaegaw` before that rule was read
   closely, and the launcher now reads that suffix back out of the station
   file, so their names stay stable without anyone retyping it. `--machine`
   remains available for the case the rule is actually for — the same
   platform on more than one machine — and refuses when it contradicts what
   the checkout already recorded. `--read-only` is for a checkout with no
   push credential for its station branch.

   **Starting over.** Every identity in `.remote-station.json` is durable on
   purpose, so when a station's recorded bindings go wrong there is no
   ordinary launch that clears them. `--fresh-station` is the one that does:
   it archives the recorded Codex task and anything else answering to the
   station name in this checkout, drops the recorded `station_id`, Codex task
   and Claude session, and provisions the workstream's canonical name again —
   without the inferred machine suffix, so it is also how a station is
   deliberately renamed. Nothing is deleted; archiving is Codex's own
   reversible form of forgetting, and the abandoned task stays readable in
   the sidebar. It refuses next to `--continue` and `--no-server`, which
   respectively need the session it discards and cannot release the old task
   at all.

   **What you will see.** After the Codex task is provisioned, Claude opens
   in that terminal. That interactive session *is* the station: it is the
   only form that exists, and is named, from the moment it starts, which is
   what makes the name bind. Leave it running. Work arrives as messages from
   the integrator; you can also drive it from claude.ai/code, the mobile
   app, or by typing there. The old server form showed a status screen
   instead — and never created a session.

   The launcher refuses a wrong repository, wrong branch, or a
   platform-bound workstream that does not match the host (WSL2 counts as
   Linux, native Windows as Windows), writes the untracked
   `.remote-station.json` (keeping its durable `station_id` across
   relaunches), provisions the persistent Codex task, and starts the Claude
   server under the exclusive writer lease — `station_lease.py` in the
   skill — so a second writer in the same checkout is denied mechanically.
   `--pair-codex` prints a Codex Remote pairing code on Unix hosts; native
   Windows pairs through Codex Desktop. `--no-server` is the deprecated
   direct-Codex path that writes the station file only. The
   `scripts/launch_remote_station.py` at the repository root forwards to the
   skill's launcher.

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

## Windows hosts: the supported platform-course procedure

Recorded under BPR-0493 from the first complete `--fresh` runs of BPR-0311's
ledger command on the JaegAW host (2026-09-02), so the next Windows station
learns these from the document rather than from a failed run each.

1. **Smart App Control is off.** That is the supported state, per the section
   above; the course does not detect an enforcing host, it fails at configure
   the way that section describes. A run on an enforcing host is unsupported,
   not merely slow.
2. **Run the ledger command from a Visual Studio developer environment** —
   the "x64 Native Tools Command Prompt", or a shell that has run
   `vcvars64.bat`. Two independent steps need it: `windows-app-release` is a
   Ninja preset and an ordinary shell has no `ninja` on `PATH` (`CMake was
   unable to find a build program corresponding to "Ninja"`), and the
   symbol reader behind `check_platform_surfaces` shells `link /MAP` and
   `undname`, which only that environment provides. The failure without it
   is `configure-failed:windows-app-release` at the release step, thirteen
   minutes in.
3. **Delete the build root, not just its cache.** `--fresh` clears the CMake
   cache; it does not clear the dependency cache under
   `build/<tree>/_bpr/`, which keeps earlier configuration logs and the
   objects they describe. When the run must prove a clean tree, remove the
   whole build root first.
4. **Retire hand-written presets that the repository has since committed.**
   A gitignored `CMakeUserPresets.json` that still carries a preset the
   repository now ships (`windows-app-release` was one) makes *every* preset
   command on the host fail with `Duplicate preset`, `windows-dev` included.
   Remove the superseded pair before running anything.
5. **The shipped artifact's launch environment is the course's job.** The
   course launches the artifact outside CTest and gives those launches the Qt
   runtime directory the registered tests declare (BPR-0511); an operator
   should not prepend it by hand, and a run that needed a hand-repaired
   `PATH` is not a ledger row.

The multi-config `windows-dev` tree and the single-config
`windows-app-release` tree resolve from their own caches in one run; the
course reads the configuration each tree declares (BPR-0492) and the
dependency provider builds only the configurations the tree selects
(BPR-0495).

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
- at launch, with no controller connected, the station appears in another
  machine's agent list under its exact name (no auto-variant suffix);
- a well-formed envelope is accepted and a mismatched one refused by name;
- the mobile app can drive the station;
- `--continue` reattaches after a reboot with the same station identity;
- on native Windows, `py -3 scripts/launch_remote_station.py` detects
  `windows-native` from PowerShell or cmd with no POSIX shell involved;
  under WSL2 it detects `wsl2` and refuses role `win`.
