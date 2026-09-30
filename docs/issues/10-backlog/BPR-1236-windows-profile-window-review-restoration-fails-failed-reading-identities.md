---
schema: "implement-rfc.card.v1"
id: "BPR-1236"
title: "Windows: application_profile_window_review_restoration_public_path fails at 'failed reading restoration preserves exact recorded identities and sibling'"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
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
