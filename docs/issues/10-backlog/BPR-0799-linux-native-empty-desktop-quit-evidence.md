---
schema: "implement-rfc.card.v1"
id: "BPR-0799"
title: "Linux: observe the native empty-desktop quit"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: []
platforms: ["linux"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"RFC-BPR-0033 B1, carried as not run since the profile-chooser milestone","acceptance":["B1"]}
---

# BPR-0799: Linux: observe the native empty-desktop quit

## Outcome

RFC-BPR-0033 B1 says that on Linux, closing the last window of the last open
profile quits through the existing drain and Quit path, and that the restore
records still tell that close from a Quit as RFC-BPR-0024 requires. On this
pack's macOS lane only the **injected** policy outcome was ever proven:
`native_empty_desktop_policy()` answers `stay_resident` on macOS, so the quit
branch is never taken there.

Every closeout row in this pack has carried that as **not run, never as
passing**, and said in as many words that no card carrying `platforms` metadata
existed to hold it. This card is that home. It was allocated on main in d8 so
the Linux lane has somewhere to record the observation.

## Acceptance

- On target-native Linux, closing the last window of the last open profile
  reaches quiescence through the existing drain and Quit path with exit code 0,
  and presents no chooser on the way out.
- The restore record distinguishes that close from an explicit Quit: the next
  launch opens a window whose identity differs from the explicitly closed one.
- `native_empty_desktop_policy()` is observed answering `quit` on the target,
  rather than assumed from the `#if`.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run**: no
Linux observation exists for this behaviour, and none is to be inferred from
the injected policy course.

## Boundaries

No GitHub or GHES mutation, credential access or provider change is authorized
by this card. macOS residency is [[BPR-0796]]; Windows is [[BPR-0798]].

## Linux station evidence, 2026-09-17

Station `bpr-linux` recorded this evidence during task
`task-20260917-lnx-evidence` (`dsp-001-r2`), in
[BPR-0799's Linux native evidence](../../verification/BPR-0799-linux-native-empty-desktop-quit.md).
The builds are product source `9bbe3604`, GCC 15.2; the tests-disabled image's
SHA-256 is `da30fc69…f058`.

- **Native policy (criterion 3): observed.**
  `native_empty_desktop_policy()() == quit` holds on the target. It is asserted
  inside `application_profile_management_public_path` on `dev`, whose close and
  relaunch course uses an injected quit policy.
  - That test **fails** under the stock `/tmp` with `FAILED: and an orderly exit
    code`.
  - It passes with a private `TMPDIR`.
- **Close from Quit (criterion 2): observed natively.** The tests-disabled
  image ran on the WSLg display, where Qt chose Wayland, and was closed through
  the caption close.
  - A caption close drops the window row, and the next launch opens a new
    identity: 1, then 2.
  - An explicit Quit keeps window 3, and the next launch restores 3 at a new
    generation.
- **Quit with exit code 0 and no chooser (criterion 1): passes at umask
  `0022`.** Exit 0 came 43–45 ms after the close, with the `op=application
  outcome=shutdown` diagnostic and no chooser window seen.
  - **It fails at umask `0002`.** The data root's parent is created 775, and
    closing the last window then exits **1**.
  - **Cause.** The agent workspace neighbourhood beside the data root is refused
    for a group-writable parent (`prepare_workspace_neighbourhood`). Startup
    recovery then marks cleanup failed.
  - **Negative control.** A parent pre-created 755 under the same umask exits 0.
  - The same refusal is behind the automated failure under `/tmp`.

Not Done. Criterion 1 fails under umask `0002`. Ubuntu's `USERGROUPS_ENAB yes`
plausibly gives desktop logins that umask; no real desktop session was
observed. The defect is reported to the integrator and needs a design decision
in the Unix agent workspace path.

The card was not moved on the lane: any move rewrites its link in
`docs/rfcs/README.md`, which this dispatch does not permit.
