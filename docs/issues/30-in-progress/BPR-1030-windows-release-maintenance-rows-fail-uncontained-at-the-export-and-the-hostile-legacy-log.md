---
schema: "implement-rfc.card.v1"
id: "BPR-1030"
title: "Windows: sqlite_release_maintenance_files and application_release_maintenance_public_path fail uncontained (export preview bytes; hostile legacy diagnostics in the native log)"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win under BPR-1020","found_at":"38cc26d7","rechecked_at":"7bf7de5b","related_cards":["BPR-1020","BPR-0900"],"related_rfcs":["RFC-BPR-0014"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1029--BPR-1031--9a8ec6194169.json"}
---

# BPR-1030: Windows: sqlite_release_maintenance_files and application_release_maintenance_public_path fail uncontained (export preview bytes; hostile legacy diagnostics in the native log)

## Defect

[[BPR-1020]] pinned both rows `WINDOWS_UNCONTAINED` for their contained
refusal, and found that they fail uncontained as well:

- `sqlite_release_maintenance_files` fails at "public host exports exactly
  preview bytes".
- `application_release_maintenance_public_path` fails at "hostile legacy
  diagnostics actually reach the selected native log".

Rechecked at `7bf7de5b` on the Windows station: both still fail at those
lines.

## Scope

- Find why the Windows export does not publish the previewed bytes.
- Find why the course's hostile legacy lines do not reach the log on Windows.
- Carry each row as far as it goes and name the next failure, if one follows.

## Acceptance

- AC1 `sqlite_release_maintenance_files` passes on the Windows station.
- AC2 `application_release_maintenance_public_path` passes its hostile-log,
  preview and export steps on Windows. The preview excludes the legacy bodies,
  and the export writes exactly the preview bytes to the non-ASCII save path.
- AC3 Any later failure in that row is named with its measured cause and
  handed to its own record.
- AC4 macOS and Linux keep their paths: every change is Windows-only code or
  a Windows-only course branch.

## Two causes, one in the product and one in the course (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`. Windows 11 Home 10.0.26200, MSVC 19.51 Debug,
Qt 6.11.2, `build/dev`. Logs are under `build/logs/bpr-1030-main-win/` and
`build/logs/bpr-1029-1031-final-main-win/` on the station and are not
committed. Both rows are console or offscreen and uncontained, as registered.

### The export: a dangling owner SID in the Windows descriptor (product)

A reverted probe split the compound check. The host ended in phase `failed`
with reason `io_failure`, and the destination was empty. Probes on each
`io_failure` site named the call: `CreateFileW` for the temporary
`.bpr-export-<pid>-0` failed with **1337, `ERROR_INVALID_SID`**.

`create_private_file` in `src/adapters/sqlite/src/release_maintenance_files.cpp`
called `attributes.initialize(security(), …)`. `security()` returns a
`private_security` by value. `private_security_attributes::initialize` puts
the SIDs into its ACL by copy, but `SetSecurityDescriptorOwner` stores only a
**pointer** into the `private_security`. The temporary died at the end of that
full expression, so the descriptor handed to `CreateFileW` named a freed owner
SID. The MSVC debug heap fills freed memory with `0xDD`, so every Debug export
failed. A Release build reads whatever the freed bytes hold at that moment,
so there the failure is undefined behaviour rather than a guaranteed refusal.
This is the Windows **Export diagnostics** path in Settings, not only a test.

Fixed in two places:

- `create_private_file` holds the `private_security` in a local that outlives
  the `CreateFileW`.
- `private_security_attributes` deletes `initialize(const private_security&&, …)`,
  so the same mistake cannot compile anywhere. Compiling the old call against
  it gives `error C2280: … attempting to reference a deleted function`. Every
  other caller (`quill_log.cpp`, `catalog_database.cpp`,
  `profile_activation.cpp` and the header's own helpers) already passed an
  lvalue.

### The hostile log: the running logger is the only writer on Windows (course)

On Windows the diagnostics logger opens `logs/diagnostics.log` with
`FILE_SHARE_READ` only (`src/adapters/logging/src/quill_log.cpp`), on purpose:
"A second writer or a replace/delete opener cannot coexist with this handle."
The course appends its hostile legacy lines with an `std::ofstream` while the
desktop runs. On POSIX that append succeeds. On Windows the open is refused,
so nothing reaches the log.

The Windows branch of the course now writes the lines before the desktop
starts. It creates the log with the product's own `quill_log`, so the file
carries the product's descriptor and no test code writes one. It closes that
logger, appends the lines in binary mode, and then starts the desktop. The
logger reopens the existing file, admits its descriptor, and appends after the
lines. The POSIX branch is unchanged.

### Where the row now stops: the purge inventory refuses the Git isolation's directories

With both fixes the row passes the hostile-log check and the keyboard preview.
The preview is metadata only and excludes the `BPR_CANARY_*` and `legacy`
bodies. The export writes exactly the preview bytes to `diagnostic-é.txt`,
and a second export to that existing file is refused. The row then signs in,
opens the fixture pull request and its diff, adds a second window, retires
both windows, revalidates a changed inventory, and clears diagnostics. It
stops at **"native full-profile inventory returns"**.

A reverted `source_location` probe on `fail` found where. Purge preparation
raises `invalid_authority` from `held_identity`, because
`validate_private_security(<root>\home, directory)` refuses the directory's
descriptor. The panel shows that reason as "The profile or prepared selection
changed", the text it shares with `selection_changed`.

`home`, `xdg`, `tmp`, `work`, `templates`, `hooks`, `incoming` and
`repositories` are created by the installed Git owner's
`make_private_directory` (`src/adapters/git/src/installed_git_process.cpp`).
On Windows that is `std::filesystem::create_directory`, and its
`private_directory` check there asks only "not a reparse point". The
directories therefore inherit the root's entries. The purge inventory accepts
only an exact protected owner-private descriptor on a directory. So on Windows
a profile whose Git isolation exists cannot be purged. The two adapters
disagree about what a private directory is on Windows. Deciding which side
moves is a security-policy question. It is outside this card's two named
failures, and an ID has been requested for it.

### Intermittent, and not this card's

- In 2 of 9 runs the row stopped earlier, at "real Git producer publishes an
  exact epoch", after `EPOCH_EFFECT_FAILED:revision-prepare:0`. That was once
  before and once after the rebase onto `7d68d5a3`.
- After the rebase, the executable from the full build exited `0xc0000409`
  with no output at about 16 s, 6 times in a row within six minutes. The same
  source relinked target-only did not crash in 7 runs: 5 with a step trace
  and 2 without. What crashed is not established. With no debugger on the
  station, the next step is a crash dump or the CRT-diagnostics build
  [[BPR-1027]] used.

### Verification

Measured at `2b14139f` with the BPR-1029, BPR-1030 and BPR-1031 changes
applied, which is the source tree those three cards' commits produce:

| Row | Before (`7bf7de5b`) | After |
|---|---|---|
| `sqlite_release_maintenance_files` | `FAILED: public host exports exactly preview bytes` | Passes, 0.68 s |
| `application_release_maintenance_public_path` | `FAILED: hostile legacy diagnostics actually reach the selected native log`, 1.84 s | Passes every step through diagnostics clear; stops at `native full-profile inventory returns` (above) |
| `test_network_isolation_manifest` | Passes | Passes, 4.42 s |

Every product change is inside `#if defined(_WIN32)` code or a Windows-only
header, and the course change is a `_WIN32` branch that leaves the POSIX
append as it was. `clang-format --dry-run --Werror` is clean on the touched
files.
