# Remote Stations

Machine setup for running BetterPullReview RFC work across computers without
mixing project, platform, or branch context. The contract a session follows
lives in `.agents/skills/implement-rfc/references/remote-routing.md`; this
document is the operator-facing half: what to provision on each machine and
how to launch it.

## Station management and hook removal (2026-10-11)

The owner removed automatic implement-rfc station hooks after they obstructed
ordinary work and MASK recovery. `.codex/hooks.json` no longer installs the
station prompt, tool or Stop guard. Reload project hooks in the host after
pulling this change. The old hooks are not a security boundary, and their
turn-admission and retired-endpoint fences no longer apply in this consumer.

Use the existing project script from this checkout:

```sh
python3 scripts/launch_remote_station.py status --json
python3 scripts/launch_remote_station.py status --observe-runtime --json
python3 scripts/launch_remote_station.py takeover --json
```

Use `py -3` in native Windows. Status is observational and starts no runtime.
Takeover transfers ownership to a replacement chat/local mode and may stop the
previous writer; continuing the current station needs no takeover. If its
automatic stop adapter is unavailable, follow its reported manual steps and
retry with `takeover --confirm-runtime-stopped --json` only after observing the
listed writers and commands stop. A free or stale supervisor lease does not
prove runtime quiescence. Removing hooks does not stop the station, change its
identity or alter MASK's state. Explicit launches still acquire the writer lease;
agents must reconcile actual writers before conflicting checkout work.

Shared-source policy and further hook controls are tracked in AgentSkills
ASK-0105. This consumer change uses the existing pinned status/takeover helper
and requires no skill upgrade or source publication.


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
| `bpr-win` | JaegAW | windows-native | win | `windows-lane` | launched 2026-09-02 through the corrected launcher (`py -3 scripts/launch_remote_station.py`, [[BPR-0491]], [[BPR-0508]], [[BPR-0509]]) with `lane-commit` and the writer lease; the interactive session is the station. Succeeds `bpr-win-jaegaw`, whose server-form launch never created a session |
| `bpr-linux` | JaegAW (WSL2) | wsl2 | linux | `linux-lane` | launched 2026-09-02 through the corrected launcher with `lane-commit` and the writer lease; the interactive session is the station. Succeeds `bpr-linux-jaegaw`, same history |
| `bpr-main-win` | JaegAW | windows-native | main | `main` | `stn-a3502cc1`, checkout `C:/code/git/BetterPullReview`, `lane-commit`; identity confirmed 2026-09-23 at `2218ef1f` holding the project-integrator lease on its checkout. Dispatched by the Mac integrator for Windows-only cards; pulls/rebases before each push and asks the integrator for issue IDs |
| `bpr-main-linux` | JaegAW (WSL2) | wsl2 | main | `main` | `stn-5c7459ed`, checkout `/home/mmarcin/BetterPullReview`, `lane-commit`; identity confirmed 2026-09-23 at `2218ef1f`. Dispatched by the Mac integrator for Linux and cross-platform cards under the same rules |

The station file format changed to `implement-rfc.remote-station.v2` on
2026-09-01 (workstream instead of role, plus the Codex task and Claude session
identities the supervisor records). A station still carrying a v1 file keeps
its `station_id` on relaunch; until then the Codex hook guard
(`.codex/hooks.json`) stays inactive for that checkout and says so on stderr,
because a v1 file carries no Codex task binding to enforce.

A row is real once its endpoint is reachable under the exact recorded name.
The 2026-09-02 interactive launches recorded above are historical evidence
([[BPR-0491]]). The current launcher uses the compact Remote Control server
interface; a fresh session can wait for a controller connection before it
appears in peer lists. Verify real visibility after connecting.


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
   `~/.claude.json` (the launcher passes `--spawn session` explicitly, but
   the stale value should not survive to confuse a manual server launch).

4. **Launch.** From the station checkout, with the host's own Python —
   `py -3` in a native-Windows shell (PowerShell or cmd), `python3`
   everywhere else:

   ```powershell
   py -3 scripts/launch_remote_station.py                 # start or resume both
   py -3 scripts/launch_remote_station.py --fresh         # fresh context in both
   py -3 scripts/launch_remote_station.py --fresh codex   # fresh Codex only
   py -3 scripts/launch_remote_station.py --fresh claude  # fresh Claude only
   py -3 scripts/launch_remote_station.py --no-claude     # Codex only
   py -3 scripts/launch_remote_station.py --no-codex      # Claude only
   ```

   On macOS, Linux, and WSL2 use `python3` instead of `py -3`. Native Windows
   runs directly in PowerShell. `--help-all` explains machine naming, read-only
   authority, pairing, and troubleshooting; older flags remain accepted.

   `--fresh` resets context while keeping the station identity and name.
   Codex's previous task is archived, not deleted. With a service disabled,
   `--fresh` resets only the enabled service and retains the skipped binding.
   Codex-only mode holds the writer lease until Ctrl+C; keep the terminal open.
   Skipping a service does not stop an already running endpoint.

   Claude uses the compact `claude remote-control --name ... --spawn session`
   interface. A new session may appear only after a controller connects; the
   launcher watches for its exact bridge session ID until Claude exits.
   Restarts use that recorded ID, never a worktree-wide `--continue` guess.
   `--require-claude-resume` is the strict troubleshooting option; normal
   launches already attempt resumption automatically.

   Codex bootstrap failures include the server's error message, such as an
   upgrade requirement. If Desktop owns an idle task's writer, archive that
   exact task in Desktop and retry the fresh command. The launcher recognizes
   the archived catalog even when `thread/read` omits the archived flag.


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

   **What you will see.** Claude shows the compact Remote Control status,
   session link, and QR-code controls. Drive the session from claude.ai/code or
   the mobile app. Codex-only mode instead prints its task identity and waits
   for Ctrl+C while holding the writer lease.

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

The RFC-BPR-0017 course adds one requirement to the same procedure: the
verified-KSyntaxHighlighting nested configure wants `perl`, and the only
`perl` on the JaegAW host is Git for Windows'. Append Git's `usr/bin` to
`PATH` — never prepend it — because that directory also ships a `link.exe`,
and the symbol reader behind the surface checks shells `link /MAP` and must
find MSVC's first. `bpr-win` found this on 2026-09-02 when the first
RFC-0017 course died at `configure-failed:windows-dev:missing:
PERL_EXECUTABLE`; the [[BPR-0036]] PATH addendum is what fixes it.

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

## Linux hosts: the Secret Service `login` collection must be unlocked

The `linux_secret_service_native*` rows no longer need this. Since
[[BPR-1117]] each runs on a private bus with a test-only keyring that its
harness starts, and refuses the host's own session bus
(`docs/development/build-and-test.md`, the Linux Secret Service section).
Nothing below should be done for them. What follows was recorded for them
and for the RFC-BPR-0016 vault rows, and that card changed nothing for the
latter.

Every RFC-BPR-0016 vault row on Linux talks to the real
`org.freedesktop.secrets` service and targets the
collection the `default` alias names — the `login` collection. The vault
refuses to prompt by construction, so a locked collection makes those rows
fail with `github_credential_vault_failure::locked` and nothing on the host
will ever unlock it: on a stock desktop PAM unlocks it at login with the user's
password, and a WSL2 host has no such login. Recorded from the JaegAW WSL2
host, 2026-09-02 (BPR-0310):

- Check first, and again before every course run, since a one-shot unlock is
  per daemon and re-locks whenever gnome-keyring restarts:

  ```bash
  DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus gdbus call --session \
    --dest org.freedesktop.secrets \
    --object-path /org/freedesktop/secrets/collection/login \
    --method org.freedesktop.DBus.Properties.Get \
    org.freedesktop.Secret.Collection Locked
  ```

  `(<false>,)` is the state the rows need.
- gnome-keyring 50 rejects an empty password: the widely quoted
  `echo -n "" | gnome-keyring-daemon --unlock` creates nothing and prints no
  error. A blank-password, always-unlocked keyring is not available.
- `--unlock` alone does not touch a daemon that is already running; only
  `--replace --unlock` does, and under systemd the
  `gnome-keyring-daemon.socket` unit socket-activates the daemon, so a
  hand-started one cannot take the bus name.
- The durable shape is a systemd user override
  (`~/.config/systemd/user/gnome-keyring-daemon.service.d/`) whose
  `ExecStart` adds `--unlock` with a password file on stdin, so the
  collection unlocks at every session start. The password file is
  plaintext-equivalent at rest: it buys a working Secret Service for the
  fixtures, not secrecy, so store nothing real in that keyring. If the file
  goes missing the service fails to start rather than prompting, and
  Secret Service clients then report no service at all.
- A `login.keyring` whose password nobody knows cannot be recovered; move it
  aside and let the new collection be created.

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
- after a controller connects, the station appears in another
  machine's agent list under its exact name (no auto-variant suffix);
- a well-formed envelope is accepted and a mismatched one refused by name;
- the mobile app can drive the station;
- a normal restart reattaches using the recorded session identity;
- on native Windows, `py -3 scripts/launch_remote_station.py` detects
  `windows-native` from PowerShell or cmd with no POSIX shell involved;
  under WSL2 it detects `wsl2` and refuses role `win`.
