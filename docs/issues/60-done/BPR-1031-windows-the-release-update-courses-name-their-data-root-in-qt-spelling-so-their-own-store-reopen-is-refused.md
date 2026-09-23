---
schema: "implement-rfc.card.v1"
id: "BPR-1031"
title: "Windows: the release-update courses name their data root in Qt's '/' spelling, so their own reopen of the recovery store is refused (application_release_update_public_path, release_update_recovery)"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win under BPR-1020","found_at":"38cc26d7","rechecked_at":"7bf7de5b","reserved_title":"application_attended_recent_replay and application_release_update_public_path fail on agent-workspace-recovery repair-required at quit (BPR-0875's mechanism in C++ courses)","related_cards":["BPR-1020","BPR-0875","BPR-0806","BPR-0833"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1029--BPR-1031--9a8ec6194169.json"}
acceptance: [{"criterion":"AC1 application_release_update_public_path passes on the Windows station","outcome":"pass","revision":"3d7093f7","evidence":{"kind":"command","detail":"bpr-main-win (MSVC 19.51 Debug, Qt 6.11.2), uncontained as registered, offscreen, at 2b14139f with the BPR-1029/1030/1031 changes (the tree of 3d7093f7; BPR-1031's own commit is 3d7093f7): passes 7.48 s (was: 'both windows retain distinct text durably' after an exit-0 quit; reverted probes showed both drafts saved and the course's own load refused at open_state_directory with invalid_path, because canonical_windows_root refuses the '/' in a toStdString root).","path":"docs/issues/60-done/BPR-1031-windows-the-release-update-courses-name-their-data-root-in-qt-spelling-so-their-own-store-reopen-is-refused.md#measured-and-fixed-in-the-courses-bpr-main-win-2026-09-23"}},{"criterion":"AC2 release_update_recovery passes on Windows as registered; if pinned, the pin names its measured first contained failure and test_network_isolation_manifest reports no finding","outcome":"pass","revision":"3d7093f7","evidence":{"kind":"command","detail":"Uncontained with the native root the row passes (0.07 s; before the root fix it failed 'fresh store is empty'). Contained it stops at 'disposable private state'; a reverted probe showed the WRITE_DAC open of its QTemporaryDir under AC\\Temp refused with error 5, as BPR-0833 measured. Pinned dos-volume-name with keyword and dictionary entry in one commit; test_network_isolation_manifest passes (4.42 s) and test_network_isolation_structure_contract passes (2.41 s).","path":"docs/issues/60-done/BPR-1031-windows-the-release-update-courses-name-their-data-root-in-qt-spelling-so-their-own-store-reopen-is-refused.md#verification"}},{"criterion":"AC3 macOS and Linux keep their paths: the conversions are Windows-only, or are the identity there","outcome":"pass","revision":"3d7093f7","evidence":{"kind":"observation","detail":"native_filesystem_path returns the same UTF-8 bytes as toStdString off Windows; the recovery row's conversion is inside #if defined(_WIN32) with the original line kept in #else; WINDOWS_UNCONTAINED and the pinned dictionary are read only on the windows-appcontainer backend. No macOS or Linux run was made from this station.","path":"docs/issues/60-done/BPR-1031-windows-the-release-update-courses-name-their-data-root-in-qt-spelling-so-their-own-store-reopen-is-refused.md#measured-and-fixed-in-the-courses-bpr-main-win-2026-09-23"}}]
---

# BPR-1031: Windows: the release-update courses name their data root in Qt's '/' spelling, so their own reopen of the recovery store is refused (application_release_update_public_path, release_update_recovery)

## The premise moved, so the title did

The ID was reserved (`15a976e8`) for "application_attended_recent_replay and
application_release_update_public_path fail on agent-workspace-recovery
repair-required at quit (BPR-0875's mechanism in C++ courses)". That was
[[BPR-1020]]'s observation at `38cc26d7`. Both rows printed
`PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:…:repair-required`, and
the update course failed at "all update shutdown flushes succeed".

At `7bf7de5b` neither half of that holds:

- Since `14a18b45` ([[BPR-0817]], recorded on [[BPR-0875]]), a refused
  recovery no longer fails the quit. The update course now prints
  `AGENT_WORKSPACE_RECOVERY_REFUSED:<temp>\agent-workspace-ownership-v1:repair-required`,
  quits with exit 0, and fails later at "both windows retain distinct text
  durably".
- `application_attended_recent_replay` passed at `c4130c3a` (25.52 s, recorded
  on BPR-0875).

This card takes the failure that is actually there.

## Defect

`application_release_update_public_path` saves both windows' draft text
through the update barrier and quits cleanly. It then opens the recovery store
itself, with `make_release_update_recovery_store(root)->load()`, and that load
fails. The drafts are on disk. The course's own open is refused.

The course builds its root as
`std::filesystem::path{ QFileInfo{…}.canonicalFilePath().toStdString() }`.
Qt spells Windows paths with '/', and the storage root validator
(`canonical_windows_root`) refuses any '/' as an invalid path. That refusal is
deliberate ([[BPR-0806]]). `bpr::app::native_filesystem_path` converts at the
point a path leaves Qt, and the application uses it, so the desktop admits the
root. The course's own reopen does not convert, so the store refuses it.
`make_storage_root_owner_private` normalises its argument first, which is why
the course's first step succeeds with the same spelling.

`release_update_recovery` builds its root the same way and fails the same
way. Registered contained, it stops earlier, at its first step.

## Scope

- Name the root natively in both courses.
- Measure `release_update_recovery` contained and uncontained, and register
  it for what it measures.

## Acceptance

- AC1 `application_release_update_public_path` passes on the Windows station.
- AC2 `release_update_recovery` passes on Windows as registered. If it is
  pinned, the pin names its measured first contained failure, and
  `test_network_isolation_manifest` reports no finding.
- AC3 macOS and Linux keep their paths: the conversions are Windows-only, or
  are the identity there.

## Measured and fixed in the courses (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`. Windows 11 Home 10.0.26200, MSVC 19.51 Debug,
Qt 6.11.2, `build/dev`. Logs are under `build/logs/bpr-1031-main-win/` and
`build/logs/bpr-1029-1031-final-main-win/` on the station and are not
committed. Both rows are console or offscreen.

**Where the load fails.** Reverted probes in the recovery store showed the
desktop's own sequence working. The first open created a fresh store. The
injected obstruction refused the first save, as the course intends. The
second save reopened the retained store and committed both drafts. The
course's own `load()` after `desktop.reset()` then failed at
`open_state_directory` with `invalid_path`. `canonical_windows_root` refuses
the root because it contains '/'.

**The fix.** `application_release_update_public_path` builds its root with
`bpr::app::native_filesystem_path`, the conversion [[BPR-0806]] put at the
point a path leaves Qt. `release_update_recovery` links only the SQLite
adapter and Qt Core, so on Windows it makes the same conversion inline:
`QDir::toNativeSeparators(…).toStdWString()`. The wide form keeps a
non-ASCII profile path intact. On POSIX both paths are unchanged:
`native_filesystem_path` is the same UTF-8 bytes there, and the inline
conversion is `_WIN32` only.

**`release_update_recovery` is pinned.** Uncontained with the fix it passes.
Contained it stops at its first step, "disposable private state". A reverted
probe showed why: the `WRITE_DAC` open of its `QTemporaryDir` under
`...\Packages\bprautomatedtest\AC\Temp` is refused with error 5. That is the
refusal [[BPR-0833]] measured for the same narrowing, and it is pinned the same
way, `dos-volume-name`. The keyword (`tests/end_to_end/CMakeLists.txt`) and
the dictionary entry (`scripts/verify_test_network_isolation.py`) land in one
commit.

**What is still printed, and why it is not this card.** The update course
prints
`AGENT_WORKSPACE_RECOVERY_REFUSED:<temp>\agent-workspace-ownership-v1:repair-required`
at quit, because its data root sits directly under `%TEMP%`. Since
`14a18b45` that refusal no longer fails the quit, and the course asserts
nothing about it. Whether such courses should get a private parent is the
data-root arrangement question on [[BPR-0874]].

**`application_attended_recent_replay`, the reserved title's other row.** It
passed 7 of 8 runs today (25.25–27.22 s). The eighth failed at "UIforETW
retained comparison ready" after `EPOCH_EFFECT_FAILED:revision-prepare:6`,
which is `epoch_failure`'s Git-subprocess code. [[BPR-1020]] saw the same line
once with code 0 at `38cc26d7`. That is an intermittent failure of the Git
subprocess behind epoch preparation, not the quit mechanism this ID was
reserved for, so it is not this card's. An ID has been requested for it,
together with the same step's two intermittent failures in the maintenance
course ([[BPR-1030]]).

### Verification

Measured at `2b14139f` with the BPR-1029, BPR-1030 and BPR-1031 changes
applied, which is the source tree those three cards' commits produce:

| Row | Before (`7bf7de5b`) | After |
|---|---|---|
| `application_release_update_public_path` | `FAILED: both windows retain distinct text durably`, after an exit-0 quit | Passes, 7.48 s |
| `release_update_recovery` (contained) | `FAILED: disposable private state` | Pinned uncontained; passes, 0.07 s |
| `release_update_recovery` (uncontained, before the root fix) | `FAILED: fresh store is empty` | — |
| `test_network_isolation_manifest` | Passes | Passes, 4.42 s, with the new pin |
| `test_network_isolation_structure_contract` | Passes | Passes, 2.41 s |
| `application_attended_recent_replay` | Passes | Passes, 27.22 s |

`clang-format --dry-run --Werror` is clean on the touched files.
