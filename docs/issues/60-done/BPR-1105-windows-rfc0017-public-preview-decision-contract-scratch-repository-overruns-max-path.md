---
schema: "implement-rfc.card.v1"
id: "BPR-1105"
title: "Windows (low): rfc0017_public_preview_decision_contract's scratch repository overruns MAX_PATH when %TEMP% is about 26 characters longer"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1051's Windows measurement","found_at":"bcd57840","related_cards":["BPR-1051","BPR-0622"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
acceptance: [{"criterion":"AC1 The failing call and the length at which it starts to fail are measured on Windows","outcome":"pass","revision":"984b6e50","evidence":{"kind":"command","detail":"bpr-main-win, registered row via ctest --preset dev with TEMP/TMP at the station's temp directory (33 characters) plus a padded child: passes at extras 0-28 (deepest scratch path up to 259), fails from 29 (path 260) at the template's git add -A with 'Filename too long'; 13 of 44 tests fail at extra 45. The deepest file is 198 characters below TEMP; the validator's own git calls (rev-parse, cat-file) do not stat the working tree.","path":"docs/issues/60-done/BPR-1105-windows-rfc0017-public-preview-decision-contract-scratch-repository-overruns-max-path.md#measured-and-the-scratch-git-takes-long-paths-bpr-main-win-2026-09-24"}},{"criterion":"AC2 The row passes on Windows with TEMP at the station's own directory and at one well past the measured limit","outcome":"pass","revision":"984b6e50","evidence":{"kind":"command","detail":"run_scratch_git passes -c core.longpaths=true on Windows: the row passes 44 of 44 tests at extras 0, 29, 30, 45 and 100 (TEMP 33 to 133 characters, deepest path 231 to 331), 4.4 to 4.9 s each, leaving nothing in the padded directory.","path":"docs/issues/60-done/BPR-1105-windows-rfc0017-public-preview-decision-contract-scratch-repository-overruns-max-path.md#measured-and-the-scratch-git-takes-long-paths-bpr-main-win-2026-09-24"}},{"criterion":"AC3 Negative control: without the fix the row fails again past the limit","outcome":"pass","revision":"984b6e50","evidence":{"kind":"command","detail":"The same station and TEMP lengths without the fix (7f347d65): fails at extras 29, 30 and 45 with scratch git ['add', '-A'] ... Filename too long; passes at 28 and below.","path":"docs/issues/60-done/BPR-1105-windows-rfc0017-public-preview-decision-contract-scratch-repository-overruns-max-path.md#measured-and-the-scratch-git-takes-long-paths-bpr-main-win-2026-09-24"}},{"criterion":"AC4 macOS and Linux keep their path, or the change is run there","outcome":"pass","revision":"984b6e50","evidence":{"kind":"freeform","detail":"The flag is added only where os.name == 'nt' (SCRATCH_GIT_LONG_PATHS is empty elsewhere), so the macOS and Linux git command lines are unchanged."}}]
---

# BPR-1105: Windows (low): rfc0017_public_preview_decision_contract's scratch repository overruns MAX_PATH when %TEMP% is about 26 characters longer

## Defect

`rfc0017_public_preview_decision_contract` builds one scratch Git repository
under the temporary directory and copies it for each test. The scratch path
mirrors the real review directory, so a file in it sits about 200 characters
below `%TEMP%`:

```text
<TEMP>\bpr<16 hex>\template\docs\verification\reviews\rfc0017-public-preview-decision-v001\offline-package\subjects\001-bpr-0312-public-preview-definitions\identity\01-fixture-graphql-refusal.json
```

With the station's own `%TEMP%` (`C:\Users\mikem\AppData\Local\Temp`, 33
characters) the row passes. In [[BPR-1051]]'s Windows runs at `bcd57840`,
`TEMP` pointed at a new directory 45 characters longer, and every test
failed. The first failure is the template's `git add -A`:

```text
AssertionError: scratch git ['add', '-A'] failed: fatal: unable to stat
'docs/verification/reviews/rfc0017-public-preview-decision-v001/offline-package/subjects/001-bpr-0312-public-preview-definitions/identity/01-fixture-graphql-refusal.json': Filename too long
```

The others then fail at `RFC0017_PUBLIC_PREVIEW_DECISION_REFUSED:head commit
could not be read from git`, because the template has no commit. With a
directory 9 characters longer, the row passed (4 s). The logs are station
records under `build/logs/bpr-1051-main-win/` (`ac5-*`, `ac5short-*`) and are
not committed.

A user or CI machine whose temporary directory is that much deeper fails the
row for a reason that has nothing to do with what it checks.

## Scope

- Measure where the row starts to fail.
- Prefer a fix that removes the limit, by shortening the scratch layout or
  using long-path-safe calls, over documenting it.
- Keep the row's checks and the macOS and Linux behaviour unchanged.

## Acceptance

- AC1 The failing call and the length at which it starts to fail are
  measured on Windows.
- AC2 The row passes on Windows with `TEMP` at the station's own directory
  and at one well past the measured limit.
- AC3 Negative control: without the fix the row fails again past the limit.
- AC4 macOS and Linux keep their path, or the change is run there.

## Measured, and the scratch Git takes long paths (bpr-main-win, 2026-09-24)

Run `bpr-main-win-20260923`, `build/dev`, at `7f347d65`. Each run is the
registered row, `ctest --preset dev -j1 -V -R`, with `TEMP` and `TMP` set to
the station's temporary directory plus one padded child of the stated extra
length. The logs are station records under `build/logs/bpr-1105-main-win/`
and are not committed. `LongPathsEnabled` is 1 on this host.

**The limit (AC1).** The deepest scratch file is 198 characters below
`TEMP`: `\bpr<16 hex>\template\` and the 168-character review path above. The
call that fails is the template's `git add -A`, with `Filename too long`:

| Extra characters | `TEMP` length | Deepest path | Row |
| ---: | ---: | ---: | --- |
| 0 | 33 | 231 | passes, 44 tests |
| 10, 20, 24, 26 | 43 to 59 | 241 to 257 | passes |
| 28 | 61 | 259 | passes |
| 29 | 62 | 260 | fails: `git add -A` ... `Filename too long` |
| 30, 45 | 63, 78 | 261, 276 | fails the same way; 13 of 44 tests at 45 |

So the row fails once a path reaches MAX_PATH, 260 characters, and the
station's own `TEMP` leaves 28 characters of margin. Python wrote the same
files without complaint; only Git refused them. The validator under test runs
only `rev-parse` and `cat-file`, which never stat the working tree, so its
own calls are not affected.

**The fix.** `run_scratch_git` passes `-c core.longpaths=true` on Windows,
the setting [[BPR-0622]] gave the cache inspection for the same reason. Git
for Windows then uses long-path calls for the scratch repository. On other
platforms the command is unchanged. The row's checks are unchanged.

**The row passes past the limit (AC2).** With the fix: extra 0, 29, 30, 45
and 100 characters (`TEMP` 33 to 133, deepest path 231 to 331) all pass, 44
of 44 tests each, in 4.4 to 4.9 s, and leave nothing in the padded
directory.

**Negative control (AC3).** The table above is the row without the fix, on
the same station and `TEMP` lengths: it fails from 29 characters on.

**AC4.** The flag is added only where `os.name == "nt"`, so the macOS and
Linux command is byte for byte what it was.

**Not fixed here.** On a Windows host with `LongPathsEnabled` 0, Python's
own writes are limited to MAX_PATH as well, so the row is expected to fail
there past the same limit, in the fixture builder rather than in Git. That was
not run. Only the system setting removes that limit; shortening the scratch
names the test controls (`bpr<16 hex>`, `template`) would buy fewer than 20
characters.
