---
schema: "implement-rfc.card.v1"
id: "BPR-1232"
title: "Windows: qt_markdown_body rows fail contained because their QTemporaryDir denies the AppContainer, and leave it behind"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json","related_cards":["BPR-1121"]}
---

# BPR-1232: Windows: qt_markdown_body rows fail contained because their QTemporaryDir denies the AppContainer, and leave it behind

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`qt_markdown_body_1_0`, `qt_markdown_body_1_25` and `qt_markdown_body_2_0`
fail on Windows, contained:

```
QImage::save: failed to write image - Cannot open device for writing: Access is denied.
FAILED: write designated canary image
QTemporaryDir: Unable to remove "…\Packages\bprautomatedtest\AC\Temp\better_pull_review_markdown_body_view_tests-Hzoaoe" most likely due to the presence of read-only files.
```

`check_resources` in `tests/adapters_qt/markdown_body_view_tests.cpp` makes a
`QTemporaryDir` and writes `canary.png` in it. Contained, the directory is made
in the AppContainer's temporary directory with a protected DACL: this user full
control, and `Everyone` and the low label only read control and attributes.
There is no ACE for the container's SID. So the contained process can neither
write in the directory it just made nor remove it, and each run leaves one
behind. Run directly, the same executable passes.

When this started is not known. It was not bisected. A suspect, not measured,
is Qt 6.11.2's owner-only permissions for `QTemporaryDir` on Windows (Qt pinned
2026-09-21). [[BPR-1121]]'s unwinding is not involved: the directory survives
because of its ACL, not because of an early exit.

## Scope

- Measure when and why the directory gets that DACL.
- Then decide: give the test directory an ACE for the container, or make it
  with the tree's own scratch helper, or mark the rows uncontained with a
  measured pin. The last is a containment decision.

## Acceptance

- The three rows pass on Windows, and leave nothing in the AppContainer's
  temporary directory.
- macOS and Linux still pass.

## Fixed at `491c682f`, verified on Windows (bpr-main-win, 2026-09-29)

The cause is `QTemporaryDir`'s owner-only directory. On Windows, Qt writes
that as the protected DACL recorded above, the same shape
`scripts/contained_temporary_support.py` documents for CPython's
`tempfile`. On Windows the course now makes its resource directory itself.
It uses `QDir::mkdir`, without a mode, at a random name, and `mkdir` refuses a
name that is taken. So the directory inherits the container temporary
directory's entries, and the course removes it. POSIX keeps `QTemporaryDir`.

| At `491c682f`, contained | Result |
|---|---|
| `qt_markdown_body_1_0`, `_1_25`, `_2_0` | pass, 2.38, 2.58 and 3.42 s |
| AppContainer temporary directory after the run | no `better_pull_review_markdown_body_view_tests-` entry added |

The runs before the fix, recorded above, are the control. The same rows failed
at `write designated canary image` and left their directories.

Still open: a macOS and a Linux run, where the course still uses
`QTemporaryDir`, to show the rows still pass there. The card stays in Backlog
until then.

## Linux, the POSIX `QTemporaryDir` path (bpr-main-linux, 2026-09-29)

Ubuntu 26.04 on WSL2, GCC 15.2, Qt 6.11.2 offscreen, `build/dev` Debug at
`e9e65eab2`, which contains `491c682f`. `qt_markdown_body_1_0`, `_1_25` and
`_2_0` pass in 0.32, 0.31 and 0.60 s. A second run of the three, with a
listing of `/tmp` taken before and after, added no entry to `/tmp`. That is
the Linux half of "macOS and Linux still pass". The logs are in
`build/logs/bpr-station-publication/` and are not committed.
