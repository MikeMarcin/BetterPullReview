---
schema: "implement-rfc.card.v1"
id: "BPR-1437"
title: "Read every command line with QCommandLineParser"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit (item 10: replace with Qt)"}
---

# Read every command line with QCommandLineParser

The 2026-10-13 audit found four hand-written argument readers that had
drifted apart. `bpr tour` refuses a value that is itself an option, but
`bpr cycle` takes `--file --json` as a file named `--json`. The desktop
application ignores unknown options and takes the word after `--ghes-host`
whatever it is, so `--ghes-host --new-window` sets the host to `--new-window`.
Its profile launch flags have a fourth reader of their own. The owner chose
Qt's parser.

## Acceptance

- AC1: One strict reader in `src/app` is built on `QCommandLineParser` from a
  table of flags and value options. It refuses an unknown option (including
  single-dash words, the `--name=value` spelling and a bare `--`), a missing
  value (including when the next word is itself an option) and a repeated
  option.
- AC2: `bpr tour` and `bpr cycle` read through it and keep their per-command
  allow-lists, bounds and error codes. `bpr cycle` refuses `--file --json`.
- AC3: The desktop application reads its arguments once through it, after
  QApplication has removed Qt's own options. Tests-only options are declared
  only in a tests-enabled build. An unknown option, or `--ghes-host
  --new-window`, is refused at startup, and the profile launch refusals keep
  their marker. The checks that must run before QApplication (installed
  runtime, designated-live provenance) remain narrow presence checks. The
  three-word live-candidate test seam becomes one `OWNER/REPOSITORY/NUMBER`
  value.

## Evidence

Pending.
