---
id: BPR-0482
title: Port the remote-station launcher to Python
status: In Progress
rfc: none
depends_on: []
---

# Port the remote-station launcher to Python

## Why

The owner challenged the operator instruction to run
`scripts/launch_remote_station.sh` under Git Bash on native Windows: the
station tooling should be PowerShell or Python, not a bash script behind a
POSIX shim. The repository agrees with the challenge on inspection — there is
no recorded rationale for the bash choice, every other operational tool in
`scripts/` is Python, the lease wrapper the launcher execs
(`station_lease.py`) is already Python, and BPR-0311's own verification row
invokes `py -3`. The Git Bash acceptance check in
`docs/development/remote-stations.md` has never run, because no Windows
station has been provisioned, so the bash launcher has zero proven Windows
behavior to preserve — while MSYS path mangling, `uname` special-casing, and
process-lifetime semantics under the exclusive writer lease are exactly the
risks the launch path should not carry.

Python is chosen over PowerShell because one implementation serves all four
roles; a PowerShell launcher would be a Windows-only twin kept in sync with
something else on the POSIX hosts.

## Required final state

- `scripts/launch_remote_station.py` reproduces the complete launcher
  contract: role/branch binding, repository-origin refusal, wrong-branch
  refusal, platform detection (macOS, WSL2 vs native Linux via
  `/proc/version`, native Windows), the role-platform guard, deterministic
  station naming, durable `station_id` across relaunches, the exact
  `.remote-station.json` shape, lease-role selection, `--no-server`
  provisioning, and the lease-wrapped `claude remote-control --spawn session`
  launch with `--continue` support.
- The bash launcher is gone; `docs/development/remote-stations.md` and
  `references/remote-routing.md` name the Python invocation (`py -3` on
  native Windows, `python3` elsewhere), and the never-run Git Bash acceptance
  check is replaced by one that matches the Python launcher.
- Refusals and provisioning are exercised in a scratch repository, not
  asserted: wrong branch, wrong role for the platform, foreign origin,
  station-file write, and station-id durability.

