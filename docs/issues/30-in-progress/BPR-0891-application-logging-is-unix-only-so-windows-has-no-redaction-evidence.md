---
schema: "implement-rfc.card.v1"
id: "BPR-0891"
title: "application_logging is registered on UNIX only, so Windows has no evidence that logs exclude tokens"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable src/app and gates partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0891: application_logging is registered on UNIX only, so Windows has no evidence that logs exclude tokens

## Defect

`tests/end_to_end/CMakeLists.txt:2497-2505` wraps the `application_logging`
row in `if(UNIX)` **with no comment**. The test body
(`application_logging_tests.cpp:42-88`) uses only `std::filesystem`, a
`QTemporaryDir` and the composition — nothing POSIX-specific — so the gate
looks incidental rather than reasoned.

What it pins is a security property: that logs exclude the access token, the
refresh token, the data-root path and repository names (`:87-88`). **Windows
therefore has no automated evidence of any of it.**

Contrast `application_profile_activation_public_path` at `:2647`, whose
UNIX-only gate *is* justified — by the Windows stub at
`src/adapters/sqlite/src/profile_activation.cpp:270-275`. That is what a
reasoned platform gate looks like, and this one has no equivalent.

There is a second-order reason this matters now: [[BPR-0885]] records that the
diagnostics log does not open on Windows at all. If that is fixed, Windows
starts writing logs with no redaction contract behind them. The two should
land together, in that order.

## Scope

- Register the row on Windows. The owner-private root pattern it needs already
  exists at `github_application_liveness_tests.cpp:50-62` ([[BPR-0832]]).
- If it genuinely cannot run there, write the reason into the `if(UNIX)` as a
  comment, the way the activation row does, and say where the Windows evidence
  for redaction comes from instead.
- While there, check the other unexplained platform gates in the same file.
  An unexplained `if(UNIX)` around a security property is worth a sweep, not
  just a fix.

## Acceptance

- Windows has automated evidence that logs exclude the access token, refresh
  token, data-root path and repository names, or a recorded reason why not.
- The gate carries its reason in a comment either way.
- macOS and Linux keep the row.

## The premise is stale; Windows evidence, strengthened (bpr-main-win, 2026-09-23)

**The row already runs on Windows.** `19a3dfd7` (RFC-BPR-0014's
release-maintenance slice, 2026-09-20) removed the unexplained `if(UNIX)` and
gave Windows a native diagnostics sink in `src/adapters/logging/src/quill_log.cpp`.
The registration now carries its reason in a comment: "The same native sink
and canary course now run on Windows; no platform privacy waiver." It is
registered with `WINDOWS_UNCONTAINED` and pinned. macOS and Linux keep the
row, which is now unconditional.

**What Windows writes, read rather than assumed.** A local dump of the
course's `logs/diagnostics.log` on this station at `cbf944ee`, not committed,
shows 43 typed records: `op=`, `outcome=`, `request=`, `http_status=`,
`epoch_stage=`, `failure=`, `page_operation=` and similar fields. It contains
no `token`, no `C:`, no backslash and no forward slash at all, no `Users`,
`AppData` or `Temp`, no user name, no `octo` and no `review-repository`, and
not the temporary root's random leaf in any case.

**The Windows gap in the assertion, closed.** The course looked for the data
root only in its native spelling (`text.find(root.string())`). On Windows a
log could also write that path with forward slashes or in another case and
pass that check. The course now also looks for the root's random leaf,
which is unique to the run, after folding case. That finds the root however
it is spelled. It also looks for the repository's own name,
`review-repository`, beside its owner's.

A local trial that appended `root.generic_string()` (forward slashes) to the
log text before both checks showed the difference. The old native-spelling
check passed that text. The new one failed with "logs exclude the data root
however it is spelled", exit 1
(`build/logs/bpr-0891-main-win/application_logging-negative-trial.log`).

| Check on this station (MSVC 19.51 Debug, Qt 6.11.2) | Result |
| --- | --- |
| `application_logging` at `cbf944ee`, before the change | Passed, 7.65 s |
| `application_logging` with the strengthened checks | Passed 3 of 3 (`--repeat until-fail:3`), about 7.5 s each |
| Negative trial (forward-slash root spliced into the text) | Failed as it should, on the new check only |
| `test_network_isolation_manifest`, `test_network_isolation_structure_contract` | Passed |
| `format_check.py` on the test | Passed |

**The sweep of the file's other platform gates.** Every `if(UNIX)`,
`if(APPLE)`, `if(WIN32)` and platform-name test in
`tests/end_to_end/CMakeLists.txt` was read:

- Already explained by a comment: the vault and device-flow expectation
  selectors (lines 1 and 37), the CLI read (552), public preview (779), the
  agent crash-recovery courses (`UNIX OR WIN32`, [[BPR-0622]]), the native
  notification platform (`APPLE` uses cocoa), the linked-spelling root cause
  row (`UNIX`, [[BPR-0843]]), the Windows startup and title-row block
  (`WIN32`), and the macOS menu-bar course (`APPLE`).
- The Linux `QT_STYLE_OVERRIDE=Windows` in the fixture environment is the
  Fusion cached-painter workaround that `tests/AGENTS.md` and
  `tests/adapters_qt/CMakeLists.txt` document. It is not a security gate.
- **Two gates had no reason, and now have one.** The in-place updater's
  transaction and native staging rows are `APPLE` because the updater is
  macOS only: elsewhere `release_update_staging.cpp` answers
  `unsupported_platform`, and Windows and Linux keep manual distribution.
  That is not a waiver. The large-review rows are a measurement gate, not a
  security gate. Four of them sample through the Mac-only memory sampler,
  which throws on any other platform. The comment says plainly that the two
  history rows take no sample and have no recorded reason to stay on macOS
  alone.

None of the gates guards a security property without a reason.

**Related, not changed here.** [[BPR-0885]] (Ready) still says that Windows
has no diagnostics log at all. `19a3dfd7` gave Windows one, and this course
reads it, so that card's first item looks stale. Its root-walk and
failure-reason items were not checked here.
