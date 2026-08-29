# Remote Stations

Machine setup for running BetterPullReview RFC work across computers without
mixing project, platform, or branch context. The contract a session follows
lives in `.agents/skills/bpr-implement-rfc/references/remote-routing.md`; this
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

| Station name | Machine | Environment | Branch |
|---|---|---|---|
| `bpr-main-mac` | MacBook Pro | macos | `main` |
| `bpr-win-jaegaw` | JaegAW | windows-native | `windows-lane` |
| `bpr-linux-jaegaw` | JaegAW (WSL2) | wsl2 | `linux-lane` |

Add machine suffixes as soon as one platform exists on two machines: remote
messaging resolves duplicate names to silent auto-variants, which misroutes
without an error.

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

4. **Launch.** From the station checkout (Git Bash on native Windows, the
   WSL2 shell for the WSL2 station):

   ```sh
   scripts/launch_remote_station.sh win --machine jaegaw --read-only
   scripts/launch_remote_station.sh linux --machine jaegaw --read-only
   scripts/launch_remote_station.sh main             # the integrator itself
   scripts/launch_remote_station.sh win --continue   # reattach after restart
   scripts/launch_remote_station.sh linux --no-server # Codex: provision only
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
- on native Windows, the launcher runs under Git Bash and detects
  `windows-native`; under WSL2 it detects `wsl2` and refuses role `win`.
