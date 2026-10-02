---
schema: "implement-rfc.card.v1"
id: "BPR-1266"
title: "bpr writes Qt's C-locale notice to standard error, breaking the CLI's stderr contract in a C locale"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1167's Linux run, 2026-10-02","reservation":"docs/issues/_reservations/BPR-1266--BPR-1267--82b4691d051e.json","related_cards":["BPR-1167","BPR-1267"]}
---

# BPR-1266: bpr writes Qt's C-locale notice to standard error, breaking the CLI's stderr contract in a C locale

Found by bpr-main-linux on 2026-10-02, at `91f25b215` (see [[BPR-1167]]'s Linux note).

## Observation

`cycle_cli_public_path` runs `bpr` with an empty environment (no `LANG`).
On Linux its first command, `bpr cycle capabilities`, fails the course's
"success writes nothing to standard error":

```
Detected locale "C" with character encoding "ANSI_X3.4-1968", which is not UTF-8.
Qt depends on a UTF-8 locale, and has switched to "C.UTF-8" instead.
If this causes problems, reconfigure your locale. See the locale(1) manual
for more information.
```

`QCoreApplication`'s construction applies the environment's locale. Finding
LC_CTYPE not UTF-8, it switches LC_CTYPE to a UTF-8 locale itself and prints
this notice with `qWarning`. So every successful `bpr` command run in a C
locale writes to standard error, against the CLI's contract. macOS prints no
such notice.

## Scope

- Fix it in the product (`src/app/src/tour_main.cpp`), not in the course.
  The course keeps its empty environment.
- Do no blanket filtering of standard error. Qt's other locale message, that
  it *failed* to switch to UTF-8, still reaches standard error.

## Acceptance

- `cycle_cli_public_path` passes on Linux with the course's empty
  environment.
- Negative control: without the fix, the course fails on the notice.
- macOS is unaffected, or the change is run there.
