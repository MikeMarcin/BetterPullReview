---
schema: "implement-rfc.card.v1"
id: "BPR-1791"
title: "Windows and Linux offer Use GitHub CLI at first launch, as macOS does"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1791--BPR-1791--99133219d1f4.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0015"]}
acceptance: []
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

## Acceptance (draft)

- Unit and contract tests for Windows and Linux executable discovery and configuration
  that compile and run on every host (pure parts) and on Windows.
- The composed course that shows the CLI offer runs on Windows and Linux in a
  tests-enabled build outside the automated boundary (or via its fixture
  hook), and inside the boundary still withholds it.
- Native evidence: on the owner's Windows machine, and on Linux (WSL2
  `jaegaw-wsl` or Ubuntu 26.04), first launch shows Use GitHub CLI, and with a
  signed-in `gh` it reads BPR-Test. Windows needs access to that machine.
