---
schema: "implement-rfc.card.v1"
id: "BPR-1068"
title: "Qualify standalone packages and native headless safety"
status: "In Progress"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
depends_on: ["BPR-1059", "BPR-1062", "BPR-1063", "BPR-1066", "BPR-1067"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-17","milestone":"verification-closeout","acceptance":["AC44-12"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1068: Qualify standalone packages and native headless safety

Pack alias: **PACK-PRTOUR-17** (live `BPR-1068`, allocated at import).

## Outcome

Prove first-run profile/auth setup without a desktop install, and produce tests-disabled standalone CLI packages using existing target-native lanes and dependency/notices policy. Exercise native console/process/vault/owner channel, editor/temp and output safety. Preserve supported/unsupported transport distinctions rather than equate compiling with service qualification.

## Existing ownership seams

Existing packaging and native platform lanes; CLI/headless CMake target; security/dependency verifiers.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Record actual Windows/macOS/Linux observations per shipped capability; absent targets stay Not run. No GUI requirement, boot daemon, token extraction, repo code execution or unqualified CLI transport is introduced.

Acceptance ownership: AC44-12

## Execution boundary

Milestone: **verification-closeout**. Pack alias PACK-PRTOUR-17 was allocated as live `BPR-1068` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.

## Linux station run (bpr-main-linux, 2026-10-06)

Ubuntu 26.04 LTS x86_64 under WSL2, GCC 15.2.0, Qt 6.11.2. Start
`b078d8bdf`; product changes `112665f81` (Secret Service readiness before
sign-in) and `4320b41ca` (capabilities `platform` block). Full commands and
results are in the [ledger](../../verification/RFC-BPR-0044-0046.md),
"Verification closeout — standalone package and native headless safety, Linux".

| Capability | Linux observation |
|---|---|
| Separately installable `bpr` | `tour-cli` preset and the `TourCLI` component install into a private root; `RUNPATH $ORIGIN/../lib`, private Qt and ICU, no GUI library, no system Qt. Runs with no display, no session bus and an invalid QPA |
| Release-grade package | Not possible here, and none exists: `package.py` has no `TourCLI` route (it packages the desktop's `Runtime` component) and refuses Ubuntu 26.04; the install carries no notices, SBOM or source reference |
| First-run profile without the desktop | Offline `open` on a fresh root creates the owner-private registry with profile 1 `Default` and the tour store (0700/0600); root and targeting refusals hold |
| Native sign-in, vault absent or locked | Before `112665f81` the device flow started with nowhere to store the grant. Now exit 5 `vault_unavailable`, "Nothing was requested from GitHub", with the setup or unlock step |
| Native sign-in, unlocked keyring | Passes the check; the GitHub device URL and a code (redacted) appear; interrupted, exit 130. Completion not run (human step below) |
| gh transport, GHES | `transport_unavailable`; `device_flow_unavailable` |
| Online `open` | Does not return: [[BPR-1302]] (filed by this run) |
| Console/process, owner channel, editor/temp, output safety | `tour_cli_public_entry`, `tour_cli_process_commands`, `tour_cli_composition`, `tour_cli_publication`, `tour_cli_terminal_publication`, `sqlite_tour_owner_channel`, `cycle_cli_public_path`, `runtime_process_surface`, `github_cli_process_runner`, the five `linux_secret_service_*` rows, `architecture_boundaries` and its contract: 20 of 20 at `112665f81`; the installed `bpr` passes `tour_cli_tests.py` and `cycle_cli_tests.py` in the network sandbox |
| Feature discovery and docs | `capabilities` names linux, its vault, gh unavailable, "development install; no release package", "not yet qualified". `tour-cli.md` gains "Platform status"; it and `release-packages.md` no longer imply a release package or a Linux system Qt |

`git pull --rebase` renamed the three commits on publication: `112665f81` is `3632c4035` on main, `4320b41ca` is `053c3df17` and the documentation commit is `95cf88b15`. The evidence above was produced on `112665f81` and `4320b41ca`, before the rebase brought unrelated upstream changes.

AC44-12 is not met on Linux: the auth setup cannot be shown complete while
[[BPR-1302]] stands, and no release package exists. Nothing here is a release
or qualification claim.

### Human step, not taken

Completing `bpr tour connect --host github.com --transport native` needs the
owner to open `https://github.com/login/device` and enter the code the
terminal prints. It was not asked while [[BPR-1302]] stood, because the
completed `connect` would have stored the grant and then not returned. BPR-1302
is fixed on Linux (`af4fa4fd8`): the loopback course completes a native
`connect` and the following online `open`. The owner step is now ready (see
"Owner sign-in step" below).

## Owed station runs

Neither run is a release or qualification claim. Never enter a device code;
interrupt the native `connect` once it prints one, and record the outcome.
Since [[BPR-1302]] (`af4fa4fd8`), the online `open` on a never-signed-in
profile returns at once with exit 5 `authentication_required`. If it runs
longer than 30 seconds, that is a BPR-1302 regression on this platform:
interrupt it and record it on BPR-1302.

macOS (Terminal, from the checkout):

```sh
cd /absolute/path/to/BetterPullReview && git pull --rebase --autostash
cmake --preset tour-cli && cmake --build --preset tour-cli
root=$(mktemp -d "$HOME/bpr1068.XXXXXX") && chmod 700 "$root"
cmake --install build/tour-cli --component TourCLI --prefix "$root/install"
otool -L "$root/install/bin/bpr"
env -u DISPLAY "$root/install/bin/bpr" tour capabilities
env -u DISPLAY "$root/install/bin/bpr" tour capabilities --json
"$root/install/bin/bpr" tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --offline --json --data-root "$root/data"; ls -la "$root/data/state/v1"
"$root/install/bin/bpr" tour connect --host github.com --transport native --json --data-root "$root/data" </dev/null
"$root/install/bin/bpr" tour connect --host github.com --transport native --data-root "$root/data"
"$root/install/bin/bpr" tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --json --data-root "$root/data"
cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build build/dev-tour-cli
ctest --test-dir build/dev-tour-cli --output-on-failure -R "^(tour_cli_publication|tour_cli_terminal_publication|tour_cli_composition|tour_cli_process_commands|tour_cli_public_entry|tour_cli_loopback_provider|skill_pr_tour_package|cycle_cli_public_path|cycle_cli_documentation|sqlite_tour_owner_channel|architecture_boundaries|test_network_isolation_manifest)$"
rm -rf "$root"
```

Windows (PowerShell in a native x64 tools environment, from the checkout;
the `TourCLI` install deploys no Qt on Windows, so a start failure such as
exit `-1073741515` is itself the observation to record):

```powershell
Set-Location C:\absolute\path\to\BetterPullReview; git pull --rebase --autostash
cmake --preset tour-cli; cmake --build --preset tour-cli
$root = Join-Path $env:TEMP ("bpr1068-" + [guid]::NewGuid()); New-Item -ItemType Directory $root | Out-Null
cmake --install build/tour-cli --component TourCLI --prefix "$root\install"
& "$root\install\bin\bpr.exe" tour capabilities; "exit $LASTEXITCODE"
& "$root\install\bin\bpr.exe" tour capabilities --json
& "$root\install\bin\bpr.exe" tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --offline --json --data-root "$root\data"; Get-ChildItem "$root\data\state\v1"
& "$root\install\bin\bpr.exe" tour connect --host github.com --transport gh --json --data-root "$root\data"
& "$root\install\bin\bpr.exe" tour connect --host github.com --transport native --data-root "$root\data"
& "$root\install\bin\bpr.exe" tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --json --data-root "$root\data"
cmake --preset windows-dev -B build/windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build build/windows-dev-tour-cli --config Debug
ctest --test-dir build/windows-dev-tour-cli -C Debug --output-on-failure -R "^(tour_cli_publication|tour_cli_terminal_publication|tour_cli_composition|tour_cli_process_commands|tour_cli_public_entry|tour_cli_loopback_provider|skill_pr_tour_package|cycle_cli_public_path|cycle_cli_documentation|sqlite_tour_owner_channel|architecture_boundaries|test_network_isolation_manifest)$"
Remove-Item -Recurse -Force $root
```

Record each command's result, the revision and the OS, Qt and compiler on
this card, and in the ledger section's platform. The capabilities `platform`
block should read macos (Keychain, gh "explicit opt-in") and windows
(Credential Manager, gh "unavailable"), both "not yet qualified".

## Coordinator verification (bpr-main-linux, 2026-10-06)

At `3ddbd244d`, the composition contract failed: `3632c4035` and `053c3df17`
changed pinned `src/app` sources (`platform_credential_vault.{hpp,cpp}`,
`tour_remote.cpp`, `tour_cli.cpp`) without a reseal. The production manifest
is resealed with a dated "BPR-1068 reseal" note. `full_application` and the
mutation pin did not move. In `build/dev-tour-cli`, 17 of 17 pass: the
`tour_cli_*` rows, the five `linux_secret_service_*` rows,
`cycle_cli_public_path`, `skill_pr_tour_package`, `sqlite_tour_owner_channel`,
`architecture_boundaries`, `architecture_boundary_contract`,
`test_network_isolation_manifest` and the composition contract (it also
passes in `build/dev`). The format check passes.

The card stays In Progress. AC44-12 is not met on Linux while [[BPR-1302]]
(online tour reads never return) is open. Windows and macOS are owed, as
listed above.

## Owner sign-in step (prepared 2026-10-07)

This step runs on the Linux station, in a visible WSL terminal on the Windows
PC that hosts it. The browser step can be on any device.

- **Build:** a frozen tests-off `bpr` built from `399aade8e` (contains BPR-1302's
  `af4fa4fd8`) is installed in `/home/mmarcin/bpr-owner-signin/install`.
- **Run:** `bash /home/mmarcin/bpr-owner-signin/run-signin.sh [PR]`. The
  one-page guide is `/home/mmarcin/bpr-owner-signin/INSTRUCTIONS.md`.
- **What the script does:**
  1. Starts a private session bus and a fresh gnome-keyring (the real Secret
     Service) with a random, unkept password, under a temporary root.
  2. Runs `bpr tour connect --host github.com --transport native`; the owner
     enters the code at `https://github.com/login/device`.
  3. Runs a bounded (120 s) online `open` of `MikeMarcin/BPR-Test` pull
     request 1.
  4. Deletes the root, the keyring and the stored grant.
- **Results:** written to `/home/mmarcin/bpr-owner-signin/result.txt`. The
  device code goes to the terminal only, never to that file.
- **Dry run without a terminal** (no code requested): keyring ready,
  capabilities exit 0 with the linux platform block, `connect` exit 3
  `interaction_required`, and cleanup done.

A macOS-built `bpr` would observe the macOS (Keychain) row, not this Linux
row.

## Owner sign-in observed on Linux (2026-10-07)

The owner ran `bash /home/mmarcin/bpr-owner-signin/run-signin.sh` at
2026-10-07T15:17:36Z, in a visible WSL terminal on the Linux station. The setup:
- Ubuntu 26.04 LTS, WSL2 kernel 6.18.33.2.
- The frozen tests-off `bpr` from `399aade8e`, which contains BPR-1302's
  `af4fa4fd8`.
- A private session bus and a fresh gnome-keyring Secret Service.

Results, read from `result.txt`, which was scanned first: no token, device
code or credential in it, and nothing from it is committed beyond what follows.
- Keyring ready; `capabilities` exit 0.
- `bpr tour connect --host github.com --transport native`: the owner entered
  the device code at `github.com/login/device`; exit 0.
- `bpr tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --json` in a
  following process: exit 0 after 14 s, `status: ok`, `complete: true`, account
  `7605339`, repository `MikeMarcin/BPR-Test`, pull request 1, profile 1, draft
  revision 1, `remote_stale: false`. The stored sign-in was kept across
  processes and the online read completed.
- Cleanup ran. The script recorded the temporary keyring, profile and stored
  grant as deleted, and no session folder remains under the root. The user's
  own login keyring holds no items, so nothing was stored there.
- No GitHub write was made.

Linux half of AC44-12 now:
- First-run profile and native sign-in without a desktop are observed
  complete; the online read after sign-in returns.
- A release-grade Linux package is still not possible here: `package.py` has
  no TourCLI route, and this host is outside the Ubuntu 24.04 / RHEL 10
  baseline. The installed TourCLI component is the observed standalone form.
- macOS and Windows halves are owed (commands above).
