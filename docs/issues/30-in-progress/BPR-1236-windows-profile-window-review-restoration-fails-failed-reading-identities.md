---
schema: "implement-rfc.card.v1"
id: "BPR-1236"
title: "Windows: application_profile_window_review_restoration_public_path fails at 'failed reading restoration preserves exact recorded identities and sibling'"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
---

# BPR-1236: Windows: application_profile_window_review_restoration_public_path fails at 'failed reading restoration preserves exact recorded identities and sibling'

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`application_profile_window_review_restoration_public_path` fails on Windows
after 185.50 s:

```
FAILED: failed reading restoration preserves exact recorded identities and sibling
QThreadStorage: entry 2 destroyed before end of thread 0x285c1c47fa0
QThreadStorage: entry 1 destroyed before end of thread 0x285c1c47fa0
```

The `QThreadStorage` lines follow the failure. They are shutdown noise, not
its cause. The row reached its restoration course, so the application started
and the earlier steps passed. The failing step is the one where a restoration
read fails, and it expects the recorded identities and the sibling window to
survive exactly.

This was observed once, in a serial run of [[BPR-1121]]'s selection. It was not
rerun or traced. Many sources this course composes changed from 2026-09-26 to
28: the review ledger ([[BPR-1211]]) and RFC-BPR-0023 and 0045.

## Scope

- Rerun it on Windows, and on macOS or Linux at the same revision, to tell a
  Windows-only failure from a shared one.
- Trace the failing expectation to the identity or sibling that differs, and
  fix it where it is owned.

## Acceptance

- The row passes on Windows.
- macOS and Linux still pass.

## Traced on Windows (bpr-main-win, 2026-09-30)

At `e9e65eab` and later `main`, `build/windows-dev`. The row failed in 4 of 4
runs, always at the final check, `failed reading restoration preserves exact
recorded identities and sibling`. The phase that fails varies: three runs
reported `unavailable-anchor failed (1)` and one `unavailable-file failed (1)`.

A probe, not committed, compared the retained records with the ones the
course planted before the phase. It showed the same difference every time:

- Record 0, the window whose reading cannot be restored, is unchanged: window,
  destination, position and tabs are all equal.
- Record 1, the sibling that the course resumes and restores, keeps its
  window, destination and position. But its tabs differ. There is one tab
  both before and after, `Active` is 0 on both, and the window comparison is
  equal. The tab's comparison and file are equal. But the tab now has a
  `Selection`, and the recorded tab had none (`hasSelA=1 hasSelB=0`).

So restoring the sibling's reading writes a selection into its active tab that
the recorded tab did not carry. It is not yet known which side is wrong. The
restore may write a selection it should not, or the recording may save the tab
before its selection arrived. The first is a restoration defect. The second
means the recorded state was already inconsistent. That is the owner's call.
Nothing was changed. Whether macOS and Linux fail the same way at this
revision is still to be run.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

`application_profile_window_review_restoration_public_path` fails on Linux the same way as on Windows: `unavailable-anchor failed (1)`, then `FAILED: failed reading restoration preserves exact recorded identities and sibling`, after 63.9 s. So the defect is not Windows-only, and the owner's question above, whether the restore or the recording is wrong, applies on Linux too. Nothing was changed.

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## macOS — red-test backlog milestone, 2026-09-30

macOS 26 (Darwin 25.6) arm64, Apple clang, Qt 6.11.2 offscreen, `build/dev` (Debug, `-Werror`) at `69493e235`, which contains the Windows fix.

`application_profile_window_review_restoration_public_path` **passes** on macOS through `ctest`, 615.46 s (run beside five other rows). So the failure reproduces on Windows (4 of 4) and Linux (1 of 1) but not in this one Mac run. Nothing was changed; the owner's question above stands. Whether the Mac differs by timing (the Mac run is ten times slower than Linux's under load) or by platform was not investigated here.

### macOS again, at `f79d5a97d` (red-test backlog integration pass)

The row **fails** on macOS twice at `f79d5a97d`: in the full `-j 8` run
(631 s) and alone (456 s). Alone it reports `unavailable-file failed (1)`, then
`FAILED: failed reading restoration preserves exact recorded identities and
sibling`, the same final check as Windows and Linux. So macOS passed 1 of 3
runs, and the failure is shared by all three platforms. Whether the single Mac
pass at `69493e235` came from timing or from the revision was not investigated.
Nothing was changed; the owner's question above stands.

## Linux again: bpr-main-linux, 2026-10-01

At `db021262a`, the row failed in bpr-main-win's BPR-0905 Linux full suite and
again run serially on its own, with `unavailable-anchor failed (1)` and then the
same final check. The failure is shared by all three platforms, not
Windows-only. The card's platforms now name all three.

## Cause, fix and Windows evidence (bpr-main-win, 2026-10-05)

**Reproduced** on current `main` (`568db6cc5`), `build/windows-dev` Debug:
`unavailable-anchor failed (1)`, then `FAILED: failed reading restoration
preserves exact recorded identities and sibling`, after 261.8 s.

**Cause, in the recording.** The restore was right. A window keeps two
copies of its reading: the record's `Position` and its active document tab
inside `Tabs` ([[BPR-1132]]). The comparison surface's selection sink wrote
`Position` in the same turn (`retain_reading_position`). The tab's copy
followed only later: `schedule_tab_presentation`, then on the decision lane
`advance_tab_presentation`, then `observe_tab_reading`, then the tab host,
then `retain_tabs`.

`seed_auth` selects the second window's reading line and goes to Home at
once. Once Home showed, the scheduled observation no longer saw that tab's
load on the surface, so the tab kept no selection while `Position` had one.
That record disagreed with itself. Restoring it later presented the reading
in the active tab, and Quit then saved a consistent record, which differed
from the inconsistent seed. That is the `hasSelA=1 hasSelB=0` the 2026-09-30
probe saw. Whether the scheduled observation runs before Home is a matter of
timing, which is why macOS once passed.

**Fix** (`src/app/src/github_application.cpp`). The selection sink and the
reading-gesture sink call `observe_tab_reading()` in the same turn as
`retain_reading_position()`, as the tab-gesture dispatch already does. So
the active tab's reading is queued to the tab host with the selection of
that moment. `observe_tab_reading` now returns early before the tab host
exists and once the window is closing. Without that guard, a selection
during startup crashed the seed (`0xC0000005`). The round-trip composition
pins are resealed for this change (note "BPR-1236 reseal"); the mutation pin
is unchanged.

**Failing-before control.** The course now also requires, after the seed's
Quit, that each record's active tab equals its accepted reading ("durable
active tab equals the accepted reading"). Without the fix the row fails in
its first phase: `auth-seed failed (1)`, `FAILED: durable active tab equals
the accepted reading`. With the fix the whole course passes.

**Windows, after** (`build/windows-dev` Debug, whole tree, no `warning C` or
`warning LNK` line):

- `application_profile_window_review_restoration_public_path` passes alone
  (435.3 s) and again under `-j4` (444.4 s). Before the fix it failed in 4 of
  4 runs on 2026-09-30, and again today.
- 15 of 15 related rows pass under `-j4`:
  - the document-tab rows: `core_document_tabs`,
    `application_document_tab_host` and `qt_document_tabs_scale_{100,125,200}`;
  - the restoration rows: `core_`, `application_` and
    `sqlite_window_review_restoration*`, `profile_window_restoration_fixture_contract`
    and `application_document_tabs_restoration_public_path`;
  - `application_diff_workbench_public_path`,
    `application_label_authoring_contract`,
    `application_review_round_trip_composition_contract` and
    `architecture_boundaries`.
- clang-format 22.1.3 is clean on the changed files.

**Owed before Done:** the change is portable, but this card's acceptance
asks for macOS and Linux runs of the row, and neither has been run with the
fix yet.

**Note: Linux verification at `240a49688` (holds `cf5e3d7c4`; station
`bpr-main-linux`, WSL2, GCC 15.2.0, `build/dev` rebuilt whole with no
warnings, `-j1`).** 6 of 6 passed:
`application_profile_window_review_restoration_public_path` (108.6 s),
`application_document_tabs_restoration_public_path` (15.6 s),
`qt_document_tabs_scale_{100,125,200}` and
`application_review_round_trip_composition_contract`. The restoration row
passed 3 of 3 (108.6, 109.6 and 107.7 s). Before the fix it failed
intermittently on Linux with "failed reading restoration preserves exact
recorded identities and sibling": at `1d8b92642`, and once at `ac0d9d1d4`
after passing once there.
