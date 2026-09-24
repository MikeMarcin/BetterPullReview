---
schema: "implement-rfc.card.v1"
id: "BPR-1105"
title: "Windows (low): rfc0017_public_preview_decision_contract's scratch repository overruns MAX_PATH when %TEMP% is about 26 characters longer"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1051's Windows measurement","found_at":"bcd57840","related_cards":["BPR-1051","BPR-0622"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
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
