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
