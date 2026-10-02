---
schema: "implement-rfc.card.v1"
id: "BPR-1268"
title: "coding_format_check is red at main on tour_cli.cpp"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"focused BPR-1170 run, build/dev-tour-cli, macOS arm64, 2026-10-02","reservation":"docs/issues/_reservations/BPR-1268--BPR-1268--b12edcc4fd36.json","related_cards":["BPR-1168","BPR-1243","BPR-1170"]}
---

# BPR-1268: coding_format_check is red at main on tour_cli.cpp

## Observation

At `1b8fd4a5c` (main), `ctest --test-dir build/dev-tour-cli -R
"^coding_format_check$"` fails with one file needing formatting:
`src/app/src/tour_cli.cpp`, line 649, the long `capabilities` presentation
initializer whose `export_policy` conditional continues on line 650.
Homebrew clang-format 23.1.1 wants a different run of leading whitespace before
`: "invalid" } } },` on line 650; nothing else differs. The file last changed in
`7e5a093cc` (BPR-1168). BPR-1170 changed no C++ file and found this as an
existing failure.

## Outcome

`coding_format_check` passes on macOS at main: format the file with the
configured clang-format, or break the initializer so its layout no longer
depends on the formatter's alignment of a very long line. Rerun the tour CLI rows
that compile it (`tour_cli_*`, `cycle_cli_public_path`) and the format row.
