---
schema: "implement-rfc.card.v1"
id: "BPR-1252"
title: "application_setup_destination_public_path takes 50 s of its 60 s timeout on macOS and times out in a full parallel run"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"found by the red-test backlog milestone integration pass, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1252--BPR-1252--d84fbad1d11b.json","related_cards":["BPR-1200","BPR-1213"]}
acceptance: [{"criterion":"The row passes in a full -j 8 dev run on macOS","outcome":"pass","revision":"b532b1420","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -j 8 --timeout 900 at b532b1420 on macOS 26.6.2 arm64 (Apple M4 Pro), Debug, soft RLIMIT_NOFILE 1,048,576: 575/577 pass in 745 s; application_setup_destination_public_path passes in 17.12 s (was 49.5 s alone, timed out at 60 s in the integration pass). The two failures are the integration pass's known rows: workshop press (parked on BPR-1245-workshop-v001) and BPR-1236's restoration row at its recorded final check."}},{"criterion":"The card records where the time goes and why the timeout is what it is","outcome":"pass","revision":"b532b1420","evidence":{"kind":"observation","detail":"Card sections 'Where the time went', 'Fix' and 'Timeout': per-course and per-wait timings; 33 of 34 s difference between soft limits 1,048,576 and 256 is Git-backed waits, caused by Qt's CloseFileDescriptors sweeping to the soft limit on macOS (probe: 77.5 ms vs 1.4 ms per launch); the 5.3 s device-flow wait is the RFC 8628 interval and stays. TIMEOUT 60 kept: 17.5 s in a full -j 8 run leaves more than three times headroom."}}]
---

# BPR-1252: application_setup_destination_public_path takes 50 s of its 60 s timeout on macOS and times out in a full parallel run

## Observation

At `main` `f79d5a97d` on macOS arm64 (dev preset, Debug), the full
`ctest --test-dir build/dev -j 8 --timeout 900` run timed out
`application_setup_destination_public_path` at its own `TIMEOUT 60`
(`tests/end_to_end/CMakeLists.txt`). Run alone right after, it passed in
50.6 s. The main stabilization milestone's full run, the day before, passed it.
The course and its registration last changed on 2026-09-29 (BPR-1200's
refused-first-read course and BPR-1213's Overview folders).

## Scope

- Find where the 50 s go: the course's own waits, the fixture, or the
  composition. Measure before choosing.
- Either make the course do its work in less time, or give the row a timeout
  measured against the slowest supported station under a full parallel run,
  with the measurement recorded.

## Acceptance

- The row passes in a full `-j 8` dev run on macOS.
- The card records where the time goes and why the timeout is what it is.

## Where the time went (bpr-main-mac, 2026-10-01)

macOS 26.6.2 on Apple M4 Pro, Apple clang, Qt 6.11.2, `build/dev` Debug at
`0b421e1ad`. The row ran in 49.5 s alone. A temporary per-course and per-wait
timer (not committed) split it:

| Course | Time | Largest waits |
|---|---|---|
| `run_cli_source_update_course` | 28.1 s | update history after relaunch 11.0 s, retained initial revision 9.3 s, smart-Git refusal settles 4.2 s |
| `run_link_location_course` | 11.2 s | exact comparison 9.5 s |
| `run_native_read_refusal_course` | 5.6 s | device account accepted 5.3 s |
| the other seven courses | 4.6 s | none over 0.75 s |

The device-account wait is the recorded grant's `"interval": 5`: the host
waits the server's poll interval, never less than 5 s, as RFC 8628 asks.
That wait belongs to the product, so the course keeps it.

The other long waits were Git. Sampling the course showed its Git worker
spending three quarters of its samples in `QProcess::waitForStarted`, and its
children kept the parent's name for most of their life: slow between fork and
exec. Every Unix launch sets Qt's `CloseFileDescriptors`. macOS has no
`close_range`, so Qt's child closes each number from 3 to its soft
`RLIMIT_NOFILE`, one `close` call each. The shell this ran in, like the
integration pass's, had a soft limit of 1,048,576.

- A probe against the same Qt timed 20 launches of `/usr/bin/true`:
  77.5 ms each with the flag at 1,048,576, 1.4 ms at 256, and 1.2 ms without
  the flag at either limit.
- The whole row, same binary: 48.2 s at 1,048,576 and 14.4 s at 256. Of the
  34 s difference, 33 s is in the Git-backed waits above. The device-account
  wait is 5.3 s at both limits.

So the course's waits were not slow; each Git command and each `gh` request
cost a million system calls before it started. A parallel run made each of
them slower, which is how the row crossed 60 s.

## Fix

`b532b1420`. `bpr/platform/unix_descriptor_sweep.hpp` adds `bound()`, which
the child calls from its child-process modifier. Qt runs the modifier before
it applies the process parameters. `bound()` lists the child's open
descriptors with `proc_pidinfo` and lowers the soft limit to one past the
highest of them, but never below `OPEN_MAX` (10,240), so Qt's sweep still
reaches every open descriptor and stops there. If the list may have been cut
short, or a call is refused, nothing changes and Qt sweeps to the limit as
before. Off macOS it does nothing. The program the child becomes inherits that
limit: 10,240 instead of a higher one, unless a higher descriptor is open.

Git's two launches (guarded and unguarded) and the `gh` CLI runner call it.
They run once per Git command or GitHub request. `CloseFileDescriptors` stays
on every launch. The updater, the tour editor and the agent supervisor launch
at human pace and are unchanged.

The `gh` runner's modifier used to be installed only when the request had a
file-size limit. It is now installed for every launch and sets
`RLIMIT_FSIZE` whenever there is a limit, as before. `architecture_boundaries`
pins that modifier's exact text beside the output spool, so the pin and its
self-test fixture follow the new text.

`github_cli_process_runner` gains a macOS course through the real runner. An
inheritable descriptor at 20,000 under a soft limit of 65,536 is closed in the
child, and the child's limit is 20,001. Without it, the child's limit is
10,240. A parent limit of 256 reaches the child as 256. With the runner's
`bound()` call removed, the course fails, and it fails too when `bound()`
ignores the highest open descriptor (the descriptor at 20,000 survives).

## Timeout

The row stays at `TIMEOUT 60`. After the fix it takes 16.6 s alone at the
raised limit and 17.5 s inside a full `-j 8` run (below). About 5.3 s of
that is the device-flow interval and the rest is ordinary work. So 60 s
leaves more than three times the full-run time. The row is registered on
Windows and Linux as well, where it runs only three courses; this change
gives them no reason to need more.

## Verification

- `ctest --test-dir build/dev -j 8 --timeout 900` at `b532b1420`, soft limit
  1,048,576: 575 of 577 pass in 745 s, and this row passes in 17.12 s.
  The two failures are the integration pass's:
  `application_agent_finding_workshop_press_contract` (parked on
  `BPR-1245-workshop-v001`) and BPR-1236's
  `application_profile_window_review_restoration_public_path`, at its
  recorded final check (`unavailable-anchor failed (1)`). That row now fails
  in 86 s, where the integration pass recorded 456 to 631 s; its failure is
  unchanged. An earlier full run of the uncommitted candidate also passed
  this row (17.53 s) and found the two problems the commit fixes: the
  architecture verifier's `QProcess` owner rule matched a header comment, and
  the format check.
- Focused, at the same candidate: `architecture_boundaries`,
  `architecture_boundary_contract`, `coding_format_check`,
  `github_cli_process_runner`, every `git_*` contract row,
  `agent_workspace_ownership`, `gh_cli_git_smart_transport`, the native Git
  smart-transport rows, `application_refused_first_read_public_path` and
  `application_selected_file_safety_public_path` pass.
- `clang-tidy` on the three touched translation units: the runner and its
  tests are clean; `installed_git_process.cpp` reports two findings on lines
  from `dd88b9c9f0` and `9af5eb2d9d`, not on this change.
- Not run: Windows and Linux. The header does nothing off macOS; on Linux the
  adapters now link the header-only platform target, and the new course is
  macOS-only.

## Integration

`b532b1420` was rebased onto `eea5314bc` before the push and is on `main` as
`d5ba1a6f6`, with the same diff; the finish is `800802d3c`. The full run
above, and the acceptance records, are at `b532b1420`. On the rebased head the
build is clean, and `architecture_boundaries`, `coding_format_check`,
`github_cli_process_runner`, every `git_*` contract row,
`agent_workspace_ownership`, `gh_cli_git_smart_transport`, this row (17.26 s)
and its two sibling rows pass.

## Linux smoke — 2026-10-01

Run by bpr-main-win through `wsl` in the bpr-main-linux checkout
`/home/mmarcin/BetterPullReview`, which was offline, under the owner's
authorization relayed by the coordinator. Nothing was committed from WSL.
Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, Qt 6.11.2, `build/dev`
Debug, at `aac225a4`. `cmake --build --preset dev` printed no `warning:`
line. Rows ran serially through `ctest`, offscreen, with `DISPLAY`,
`WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` unset, and with fresh `HOME`
and XDG roots. No set added a `/tmp` entry. The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\linux\`, and are not
committed.

The four rows the coordinator named for this card's macOS-only fd-sweep header
all pass:

- `github_cli_process_runner`, 1.48 s;
- `git_process_contract`, 5.73 s;
- `architecture_boundaries`, 15.04 s;
- `application_setup_destination_public_path`, 2.45 s.
