---
schema: "implement-rfc.card.v1"
id: "BPR-1791"
title: "Windows and Linux offer Use GitHub CLI at first launch, as macOS does"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1791--BPR-1791--99133219d1f4.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0015"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "a10ffdf40f957faf685e03a3141f7e3616204eb3", "evidence": {"kind": "command", "detail": "macOS: gh_cli_transport ctest pass, including test_executable_search_locations_and_selection (macOS, Linux and Windows search order and canonical selection, pure, on any host) and the private pinned-configuration course. The Windows private-configuration branch of that course and the Windows run of the whole suite are pending a Windows host."}}, {"criterion": "AC2", "outcome": "pending", "revision": "a10ffdf40f957faf685e03a3141f7e3616204eb3", "evidence": {"kind": "command", "detail": "macOS, tests-enabled: application_composition_capabilities_product_run (COMPOSITION_CLI_READ:available, CLI_ENTRY:offered) and _inside_boundary (unavailable, absent), application_setup_destination_public_path (CLI discovery, connected CLI destination, CLI source update), application_device_flow_cli_choice, application_inline_publish_cli_consent, qt_composed_cli_read_consent pass. These rows now register on Windows and Linux too; neither has run there (no reachable Linux or Windows build host: ssh jaegaw-wsl refused port 22)."}}, {"criterion": "AC3", "outcome": "pending", "revision": "a10ffdf40f957faf685e03a3141f7e3616204eb3", "evidence": {"kind": "command", "detail": "Not run: native first launch on the owner's Windows machine and on Linux (WSL2 jaegaw-wsl or Ubuntu 26.04) showing Use GitHub CLI and, with a signed-in gh, reading BPR-Test. jaegaw-wsl refused SSH (port 22) from this session; no Windows host was reachable."}}]
---

# BPR-1791: Use GitHub CLI on Windows and Linux

## Request

The owner, 2026-10-27, verbatim:

> Why do I not have the use GitHub CLI option on Windows when I first launch
> it? That should be there

And, moments later:

> It should work for Linux too.

This is the owner's decision to extend RFC-BPR-0015's GitHub CLI enablement
to Windows and Linux. That RFC scoped production enablement to macOS only, "pending
target-native connector, credential, process, read/write and UI evidence".

## Grounded current behavior

- `github_application.cpp` `cli_read_offered` answers true only under
  `__APPLE__`; `cli_read_offered_here` answers false on every non-Apple
  tests-enabled graph. So Windows never wires `set_github_cli_discovery_dispatch`,
  and neither Accounts' **Use GitHub CLI** nor the first-launch sign-in
  surface's CLI offer appears.
- `src/app/src/github_cli_runtime.cpp` (`native_gh_executable`,
  `live_gh_cli_configuration`, the version probe) is `#if defined(__APPLE__)`.
- The GitHub adapter already accepts Windows: `gh_cli_transport.cpp` takes
  `gh_cli_windows_environment_facts` (USERPROFILE, APPDATA, LOCALAPPDATA,
  SystemRoot, empty PATH) and `gh.exe`; the shared process supervision has
  the Windows launch restriction (`src/platform/`). It also has a Linux
  branch (`gh_cli_linux_environment_facts`: HOME, XDG_RUNTIME_DIR).

## Outcome

- On Windows and Linux the first-launch sign-in and Accounts offer **Use GitHub CLI**
  under the same rules as macOS, including the automated-test boundary
  (BPR-0797), consent, and the existing failure states (missing executable,
  unsupported version, ineligible accounts and the rest).
- `gh.exe` is found on PATH, then in the standard install locations
  (`%ProgramFiles%\GitHub CLI`, the per-user winget/scoop locations), and
  canonicalised. On Linux `gh` is found on PATH, then in `/usr/bin`,
  `/usr/local/bin`, `/home/linuxbrew/.linuxbrew/bin`, `~/.local/bin` and the
  snap bin. The environment facts come from the real known folders. The gh
  keyring on Linux goes through Secret Service; a missing one is a reported
  failure, not a hang.
- The RFC-BPR-0015 safety properties carried for macOS hold on both or are
  disclosed: fixed argument arrays, no ambient PATH, redirect and
  account-switch handling, redacted failures, no request bodies logged.
- RFC-BPR-0015 records the amendment (Windows and Linux enabled
  at the owner's decision).

## Acceptance

- AC1. Unit and contract tests for Windows and Linux executable discovery and configuration
  that compile and run on every host (pure parts) and on Windows.
- AC2. The composed course that shows the CLI offer runs on Windows and Linux in a
  tests-enabled build outside the automated boundary (or via its fixture
  hook), and inside the boundary still withholds it.
- AC3. Native evidence: on the owner's Windows machine, and on Linux (WSL2
  `jaegaw-wsl` or Ubuntu 26.04), first launch shows Use GitHub CLI, and with a
  signed-in `gh` it reads BPR-Test. Windows needs access to that machine.

## Implementation (a10ffdf40)

- **One switch.** `src/app/src/github_cli_runtime.hpp` defines
  `BPR_GITHUB_CLI_PLATFORM` for `__APPLE__`, `_WIN32` and `__linux__`. It
  replaces `__APPLE__` in `cli_read_offered`, `cli_read_offered_here`, the
  composition's `opted_in_cli_transport_factory` selection,
  `discover_github_cli_accounts`, the read-consent gate, `github_cli_runtime.cpp`
  and `tour_remote.cpp` (`bpr ... --transport gh`). The BPR-0797 boundary answer,
  the read question, the remembered choice, pinning, strict credentials and every
  discovery failure state are unchanged.
- **Discovery (adapter, pure).** `gh_cli_executable_file_name`,
  `gh_cli_standard_install_directories` and `select_gh_cli_executable` in
  `src/adapters/github/src/gh_cli_transport.{hpp,cpp}` take the platform as a
  parameter, so all three run on any host. Windows: `%ProgramFiles%\GitHub CLI`,
  `%LOCALAPPDATA%\Programs\GitHub CLI`, `%LOCALAPPDATA%\Microsoft\WinGet\Links`,
  `%USERPROFILE%\scoop\apps\gh\current\bin`. Linux: `/usr/bin`,
  `/usr/local/bin`, `/home/linuxbrew/.linuxbrew/bin`, `~/.local/bin`,
  `/snap/bin`. macOS keeps Homebrew, `/usr/local/bin`, MacPorts.
- **Discovery (host).** `native_gh_executable` asks `QStandardPaths` for `PATH`'s
  `gh`, then each standard directory's, canonicalises each, and takes the first
  that is really `gh`/`gh.exe`. Environment facts: `SHGetKnownFolderPath`
  (Profile, RoamingAppData, LocalAppData, Windows, ProgramFiles) on Windows;
  home, `XDG_RUNTIME_DIR` and its `bus` socket on Linux.
- **Windows private configuration.** `private_account_directory` now has a
  Windows form: the directory and both files are created with
  `bpr::platform::windows_private_object`'s owner-private descriptor, the files
  exclusively and read-only, and the whole directory is removed afterwards. The
  `_WIN32` `start_failure` refusals in `run_gh_cli` and
  `acquire_gh_cli_credential` are gone.
- **Presentation.** The read warning's confinement sentence and the missing-`gh`
  message are per platform.
- **Tests.** The CLI courses of `application_setup_destination_public_path`,
  `application_device_flow_cli_choice`, `application_inline_publish_cli_consent`,
  `application_home_waiting_cli_public_path`,
  `application_personal_discovery_cli_public_path`,
  `application_review_submission_cli_ordinary` and
  `application_review_tour_cli_ordinary_*` register on all three platforms, still
  contained on Windows (no `WINDOWS_UNCONTAINED` without a measured pin). The
  product run expects the CLI offered on all three. `composition_contract_tests.py`
  checks the switch's definition and the gates' new spelling.
  `application_review_round_trip_composition_contract` is resealed
  (`github_application.cpp` full digest and the production manifest).

## Disclosed differences from macOS

- No operating system denies `gh` file writes on Windows or Linux. The unpinned
  `gh --version` and `gh auth status --json hosts` of an explicit Use GitHub CLI
  gesture use the ambient configuration and may let `gh` migrate it. Pinned calls
  read only their private configuration.
- The Windows private configuration's files are read-only, but its directory is
  not write-locked against new entries as the POSIX `0500` one is. It is
  owner-private and removed whole after each call.
- Git-object acquisition (`gh repo clone` into `/private/tmp`) and exact blob reads
  through `gh` stay macOS-only. A Windows or Linux CLI session reads the pull
  request, but full diffs through GitHub CLI are unavailable, and the read warning
  says so. A follow-up card would qualify Git-object acquisition there.
- Linux finds Secret Service only through `$XDG_RUNTIME_DIR/bus`. A session
  without that socket (no systemd user session, or a custom bus address) fails
  configuration and reports "could not be run successfully"; it does not hang.
  A snap-packaged `gh` is not taken: `/snap/bin/gh` canonicalises to
  `/usr/bin/snap`.
- Unchanged on all three: fixed argument arrays, no shell, no ambient `PATH` in
  the child (Windows passes an empty one), redirect and account-switch handling,
  redacted failures, no logged bodies.

## Verification (macOS arm64, dev preset, a10ffdf40)

- Pass: `gh_cli_transport`, `github_cli_process_runner`,
  `application_composition_capabilities_inside_boundary`,
  `application_composition_capabilities_product_run`, `application_public_path`,
  `application_public_path_ghes_cli_unavailable`,
  `application_setup_destination_public_path`, `application_device_flow_cli_choice`,
  `application_inline_publish_cli_consent`, `qt_composed_cli_read_consent`,
  `qt_composed_home_sign_in`, `application_composition_source_contract`,
  `application_review_round_trip_composition_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`, `test_network_isolation_structure_contract`;
  `composition_contract_tests.py -k test_concrete_github_selection_is_owned_by_composition -k test_the_automated_test_boundary_has_one_reader`;
  the documentation gate.
- Failing with the same assertion on the native path, which this change does
  not alter, as well as the CLI one: `application_home_waiting_public_path` and
  `_cli_` ("unopened mixed review/check example stays in Waiting on others"),
  `application_personal_discovery_public_path` and `_cli_` ("an authored pull
  request awaiting reviewers waits on others"). `test_network_isolation_manifest`
  fails on other cards' unpinned `WINDOWS_UNCONTAINED` rows and on
  `gh_cli_git_smart_transport`'s registration.
- Not run: `application_composition_contract` (nested tests-disabled Release
  build; the disk had about 1.4 GB free), `coding-tidy`, any Linux or Windows
  build. The Windows- and Linux-only code (known folders, the Windows private
  configuration, the Linux session facts) has not been compiled.
- No live GitHub call was made.

## Pending

- AC2 and AC3 on Linux: `ssh -o BatchMode=yes jaegaw-wsl true` was refused on
  port 22 from this session.
- AC1, AC2 and AC3 on Windows: no Windows build host was reachable.

