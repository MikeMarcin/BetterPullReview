---
schema: "implement-rfc.card.v1"
id: "BPR-1068"
title: "Qualify bundled macOS CLI and native headless safety"
status: "In Progress"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
depends_on: ["BPR-1059", "BPR-1062", "BPR-1063", "BPR-1066", "BPR-1067"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-17","milestone":"verification-closeout","acceptance":["AC44-12"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"},"triage_20261026":{"date":"2026-10-26","disposition":"updated","reason":"Only the owner's Windows device-flow sign-in remains; it waits on the owner, not on an implementer."}}
---

# BPR-1068: Qualify bundled macOS CLI and native headless safety

Pack alias: **PACK-PRTOUR-17** (live `BPR-1068`, allocated at import).

## Outcome

Apply the owner's [bundled-cli-r1 amendment](../../rfcs/amendments/RFC-BPR-0044-bundled-cli-r1.md): qualify `Contents/MacOS/bpr` and its runtime closure in the macOS desktop release, including final-path capabilities, first-run profile/authentication, relocation, signatures/notarization, installed notices, SPDX and corresponding source. No separate CLI release is promised or required for the macOS workflow. Preserve the historical Windows/Linux native safety obligations and their actual evidence. Exercise native console/process/vault/owner channel, editor/temp and output safety. Preserve supported/unsupported transport distinctions rather than equate compiling with service qualification.

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

## macOS station run (2026-10-07)

macOS 26.6.2 (25G83) arm64, Homebrew clang 23.1.1, CMake 4.4.3, Qt 6.11.2 (CPM
cache). Revision `e1463a556`. Built with `cmake --preset tour-cli && cmake
--build --preset tour-cli` and installed with `cmake --install build/tour-cli
--component TourCLI` into an owner-private temporary root, which was deleted
afterwards. Ledger: "Verification closeout — standalone package and native
headless safety, macOS".

| Capability | macOS observation |
|---|---|
| Separately installable `bpr` | `bin/bpr`, `bin/qt.conf`, private QtCore and QtNetwork frameworks, the SecureTransport TLS plugin under `lib/bpr/plugins/tls`. `LC_RPATH @executable_path/../lib`; `otool -L` names only the private Qt frameworks and system frameworks and libraries (Security, CoreFoundation, IOKit, DiskArbitration, UniformTypeIdentifiers, libc++, libSystem); no GUI framework. Ad hoc, linker-signed. No license, notice or SPDX file |
| Release-grade package | None, as on Linux: `package.py` has no `TourCLI` route |
| Discovery | `capabilities` (with `DISPLAY` unset) and `--json`: exit 0. `platform` = macos, Keychain, gh "explicit opt-in", "development install; no release package", "not yet qualified" |
| First-run profile | Offline `open` of BPR-Test #1 on a fresh `--data-root`: exit 2 `target_ambiguous` (no saved target), after it made `state/v1/profile-registry.sqlite3` (profile 1 `Default`) and `review-tours.sqlite3`, directories 0700 and files 0600 |
| Root safety | relative `--data-root`: exit 2 `invalid_profile_root`. An existing 0755 root: exit 7 `profile_unavailable`, left 0755. A symlink to a valid root: exit 7 `profile_unavailable` |
| Machine targeting | `status --json` without `--profile`: exit 2 `target_ambiguous` |
| Sign-in without a terminal | native `connect` with stdin `/dev/null`, `--json`: exit 3 `interaction_required` |
| GHES | native `connect --host ghe.example.com` on a pty: exit 5, "available only on github.com" |
| Native sign-in, live, interrupted | on a pty: GitHub's device URL and a code (redacted), then SIGINT, exit 130, "no remote content writes were issued". No code was entered. On macOS the vault check before the code is a no-op (Keychain answers ready) |
| Online `open`, never signed in | exit 5 `authentication_required` in 0.04 s, against `MikeMarcin/BPR-Test#1` |
| Native sign-in with the Keychain, loopback | Rehearsed (see [[BPR-1302]] "macOS station run"): every native path passes with the grant in a temporary default Keychain; the login Keychain is untouched |
| gh transport | Not run against the owner's own gh, except once by mistake. A check of the vault (`connect --transport gh` with `PATH=/usr/bin:/bin`) found the owner's Homebrew `gh` through the product's fallback directories and connected account 7605339: one `/user` read and no write. The binding went to the temporary root, which was deleted, and nothing went to the Keychain. With a `HOME` that has no `Library/Keychains`, both transports refuse `vault_unavailable` |
| Focused rows | `build/dev-tour-cli`: the twelve rows listed above plus `application_github_host` and `application_epoch_host`, 14 of 14 |

The tests-off binary has no `BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK` string.
It does link one `_for_test` symbol,
`make_github_cli_process_runner_for_test`. That symbol is the gh runner's
existing unconditional seam, which `architecture_boundaries` pins. It is
unreachable from the product and is not BPR-1302's seam.

## Owner sign-in step, macOS (prepared 2026-10-07)

- **Build:** a frozen tests-off `bpr` (TourCLI install) from `e1463a556`, in
  `/Users/jaeger/bpr-owner-signin/install`. Its sha256 is `fab6b0ab…`.
- **Run:** `bash /Users/jaeger/bpr-owner-signin/run-signin.sh [PR]`, with
  the guide in `/Users/jaeger/bpr-owner-signin/INSTRUCTIONS.md`. The script:
  1. Makes the user's default Keychain a new Keychain file under a session
     folder, with a random password that is never written down.
  2. Runs native `connect`; the owner enters the code at
     `https://github.com/login/device`.
  3. Runs a 120 s bounded `open` of `MikeMarcin/BPR-Test` pull request 1 in a
     following process.
  4. On every exit, restores the previous default Keychain and search list,
     and deletes the temporary Keychain and the session folder.
- **Why a temporary Keychain:** the login Keychain already holds the desktop
  app's items for profile 1, account 7605339 on github.com. A fresh tour
  profile 1 signed in as the same account produces the same service and
  account names. So it would overwrite the desktop's sign-in, and removing
  the item afterwards would delete it.
- **Results:** in `/Users/jaeger/bpr-owner-signin/result.txt`: exit codes,
  timings, item counts and named non-secret fields. The device code goes to
  the terminal only.
- **Checks already run:**
  - Without a terminal: capabilities exit 0, `connect` exit 3, the open exit
    5 in 0.13 s, the default Keychain restored, and the login items
    unchanged.
  - On a pty with Ctrl-C at the code prompt: exit 130 and the same cleanup.
  - The loopback rehearsal of the same Keychain arrangement.

## macOS owner sign-in (2026-10-07)

The owner ran `/Users/jaeger/bpr-owner-signin/run-signin.sh` (bpr sha256
fab6b0ab…4d6f, built from e1463a556, tour-cli, tests off, Release) at
15:42Z. capabilities exit 0 (macos, Keychain, gh explicit opt-in, not yet
qualified); native connect exit 0 (account 7605339) into a temporary default
Keychain; `open` of MikeMarcin/BPR-Test#1 in a new process exit 0 after
13.83 s (bound 120 s), complete, not stale, nothing written. Cleanup: default
Keychain restored, the login Keychain's four BetterPullReview items
unchanged, session folder (temporary Keychain, profile, grant) removed.

## Owner sign-in step, Windows (prepared 2026-10-09)

The Windows station stopped its native `connect` at the device code, because
only the owner may enter it. That run is not recorded on this card. The
owner's step is in the JaegAW wizard `bpr-jaegaw-windows-20261009-v001`
(session `5ce00a47-40a3-4a06-a683-958beb074838`). Its state is
`/Users/jaeger/.mask/wizard/bpr-jaegaw-windows-20261009-v001`, its host is
served from the Mac, and its offline copy is
`/Users/jaeger/.mask/wizard/bpr-jaegaw-windows-20261009-v001-offline.html`.
Steps 2 and 15 to 17 belong to this card. The rest is [[BPR-1094]]'s R33.

- **Script:** `tests/scripts/windows_tour_owner_signin.ps1` (`ec16c550b`),
  run from a frozen root. The owner never runs it from a checkout.
- **Prepare (step 2):** a single PowerShell line. It runs `git fetch` in the
  station's checkout `C:\code\git\BetterPullReview`, which touches no working
  file, then `git archive` of `ec16c550b` into
  `C:\Users\mikem\bpr1068-signin-v001\src`. Then the script's `prepare`:
  1. enters the x64 MSVC environment;
  2. builds the `tour-cli` preset (tests off, Release), using the station's
     dependency cache;
  3. installs `TourCLI` into `...\install`;
  4. runs windeployqt there, because the Windows TourCLI install carries no
     Qt. This is a test arrangement for this run, not a package;
  5. records bpr.exe's SHA-256 and checks that `tour capabilities` starts.
     It prints `PREPARED <sha256>`.
- **Sign-in (step 16):** `signin` checks the digest, then:
  1. Uses a fresh `--data-root` under `%TEMP%` and runs `capabilities
     --json`.
  2. Lists the target names of BetterPullReview's generic credentials, never
     a secret.
  3. If a production Default-profile target already exists (`...v2/`
     credential or `...v1/` refresh, as a signed-in desktop's would be), it
     prints `SIGNIN_STOPPED` before any request. A fresh root's Default
     profile would write the same targets, the same collision the macOS step
     avoided with a temporary Keychain.
  4. Otherwise it runs native `connect` unpiped in the owner's console. The
     owner enters the code at `https://github.com/login/device`.
  5. Runs a 120 s bounded `open` of `MikeMarcin/BPR-Test#1` in a following
     process.
  6. On every exit, deletes exactly the targets added since step 2, checks
     that the set left matches the set it found, and removes the data root.
- **Result:** `C:\Users\mikem\bpr1068-signin-v001\result.txt` holds exit codes,
  durations, counts and whitelisted `open` fields (status, complete,
  remote_stale, account, repository, number, revision). The owner pastes it
  into step 17. The code and tokens never reach the file, the wizard or this
  card.
- **Not checked:** the script hasn't run on Windows. The Mac has no
  PowerShell, so a prepare or script failure comes back through step 15.

## Windows station run (bpr-main-win, 2026-10-09)

Run on Windows 11 Home 10.0.26200 with MSVC 14.51 (cl from VS 18,
`Hostx64/x64`) and Qt 6.11.2, at `58f09ba68` (build) and `f992c0cf9`
(calls). The commands are the card's, with these differences:

- no `--autostash`;
- every `bpr` call bounded;
- device codes are redacted.

No device code was requested from a working binary, and none was entered.

| Command | Result |
|---|---|
| `cmake --preset tour-cli`, `cmake --build --preset tour-cli` | exit 0 |
| `cmake --install build/tour-cli --component TourCLI` | exit 0; `bin\bpr.exe` and `bin\better_pull_review_agent_supervisor.exe`, no Qt |
| `tour capabilities`, `capabilities --json`, `open --offline`, `connect --transport gh --json`, `connect --transport native`, `open` | each exits `-1073741515` (`0xC0000135`, a DLL not found) within 123 ms, printing nothing. The TourCLI install deploys no Qt on Windows, as the card expected; nothing is created under the data root |
| `windows-dev-tour-cli` Debug build, then the twelve-row selection | build exit 0, no warning; **12 of 12 pass** (`tour_cli_composition` 38.2 s, `cycle_cli_public_path` 27.7 s, `architecture_boundaries` 39.8 s, `test_network_isolation_manifest` 22.0 s, the rest under 9 s) |

The same calls were then made with the owner's prepared `bpr.exe` (below),
which has Qt beside it:

- **`tour capabilities --json`** exits 0. Its `platform` block reads:
  `os` windows, `native_vault` Credential Manager, `gh_transport`
  unavailable, `package` "development install; no release package",
  `qualification` "not yet qualified".
- **`tour open … --offline --json`** on a fresh data root exits 2,
  `target_ambiguous`. It creates `state\v1\profile-registry.sqlite3` and
  `review-tours.sqlite3`.
- **The online `tour open`** on that never-signed-in root returns at once
  with exit 5, `authentication_required`, in 54 ms. That is not a BPR-1302
  regression.

The native connect was not run with this binary, because it would ask GitHub
for a code.

### The owner's Windows sign-in script, checked on the station

`tests/scripts/windows_tour_owner_signin.ps1` was run from a frozen root:

- **The root.** `C:\Users\mikem\bpr1068-signin-v001` holds a `git archive` of
  `f992c0cf9` (which carries `ec16c550b`'s script unchanged) and `commit.txt`.
- **`prepare` passes end to end, unchanged.** It ran in Windows PowerShell
  5.1, entering the x64 developer shell through `vswhere`, using the
  station's dependency cache. It configured, built and installed `tour-cli`,
  windeployqt added `Qt6Core`, `Qt6Network`, `networkinformation\` and
  `tls\`, the `capabilities` check passed, and it printed
  `PREPARED ca27a028974e…b243`.
- **`signin` up to the device flow.** A probe loads the script's own
  Credential Manager code and its Default-profile prefixes. It runs only the
  name listing (target names and types, never a secret) and the clash check:
  - 0 `org.betterpullreview.github-native-*` targets exist for this user;
  - so 0 are Default-profile targets.

  So on JaegAW `signin` would **not** print SIGNIN_STOPPED. It would go on to
  the device flow. The script's two prefixes are exactly the product's
  production base and refresh prefixes (`windows_credential_manager.cpp`).

`signin` itself was not started. Its connect is the owner's step.

## Triage, 2026-10-26

Nobody has worked this card since 2026-10-09, and
what is left is the owner's step. The Linux and macOS owner sign-ins are
observed. On Windows the station prepared and checked
`tests/scripts/windows_tour_owner_signin.ps1` up to the device flow.
Running `signin` and entering the device code is the owner's, through the
existing JaegAW wizard `bpr-jaegaw-windows-20261009-v001` (`d5ce5d2af`).
The card moves to Parked on `human-review`. Before handing the wizard back
to the owner, rebuild the frozen `tour-cli` root from current `main`: the
`f992c0cf9` root predates three releases.

## Bundled macOS CLI amendment — 2026-10-27

Owner amendment: [bundled-cli-r1](../../rfcs/amendments/RFC-BPR-0044-bundled-cli-r1.md).
Implementation `480f379eb` plus verification corrections `223098b11` and
`e114ca220`; clean package source `e0d07290a9d10b56d8c44a2a6e0e0dcc6792f71b`.
The current macOS obligation supersedes a standalone release prerequisite; the
historical platform observations above retain their original meaning.

- Normal and agent-free presets build `bpr`; automation-off omits it. The
  package producer deploys it at `Contents/MacOS/bpr` and includes its closure,
  nested signature checks and ordinary legal/source/security inventories.
- Universal tests-disabled local app preparation passes. Its final bundle and
  relocated Unicode/space path both pass capabilities, headless closure,
  first-run profile privacy, noninteractive authentication and not-signed-in
  online refusal. The CLI SHA-256 is
  `a8a855c7ee8ea833b4c8071e45f7d336baefe456388ec393f5b9ed12e1fe9d77`.
- Full package/security mechanics pass: 53 inventoried files, 20 binaries,
  539 first-party compile units, 540 hardened compile units; installed notices,
  71 Qt package records and 22 corresponding-source archives verified. The
  separate source/ZIP/SPDX preflight passes. Ad-hoc signatures verify.
- Package/graph tests 14/14, source-release tests 24/24, security controls
  22/22, integration checks 4/4, documentation contracts 67/67, skill smoke
  against the final CLI and clang-format 22 pass. Prior semantics evidence is
  reused unchanged. No dedicated Debug timing course was selected.
- Canonical AgentSkills `64938f9` updates only `pr-tour`; MASK authoring links
  deploy it for this checkout. The package and deployed bytes agree. There
  was no push and no observed running-agent reload claim.

Pending: Developer ID/hardened-runtime/timestamp, notarization/stapling and
Gatekeeper for actual release bytes; native authentication from a bundled
candidate; existing Windows device-flow and other platform/native safety,
trust/legal/vulnerability and BPR-1069 acceptance. No release is published.

Attended review identity: `090216984c73ddb7006b51c64ba42120`,
state `/Users/jaeger/bpr-attended/rfc0044-bundled-cli-r1-00nugkp6/review-state`, consumed cursor 3; wizard session
`e65bdc93-cf11-4c25-951d-2798edb7148f` in sibling `wizard-state`.
The protected auth candidate remains the earlier local source `223098b11`,
with the same CLI bytes but its own mimalloc binary; it is not relabelled as
the later clean package. No auth or review verdict exists yet. Capability URLs
are retained only in external state. Full evidence and the clean immutable
package live in `/Users/jaeger/bpr-attended/rfc0044-bundled-cli-r1-00nugkp6`.
