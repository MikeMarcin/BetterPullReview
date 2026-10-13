---
schema: "implement-rfc.card.v1"
id: "BPR-1437"
title: "Read every command line with QCommandLineParser"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit (item 10: replace with Qt)"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"57e8d8eed","evidence":{"kind":"observation","detail":"command_line wraps QCommandLineParser and refuses unknown options (single-dash, --name=value, --), missing or option-valued values and repeats; exercised through the tour, cycle and desktop refusal cases.","path":"docs/issues/60-done/BPR-1437-read-command-lines-with-qcommandlineparser.md"}},{"criterion":"AC2","outcome":"pass","revision":"57e8d8eed","evidence":{"kind":"observation","detail":"bpr tour and bpr cycle read through the shared reader with unchanged allow-lists and codes; 10 tour and cycle CLI tests pass, including new refusal cases; the cycle --file --json case fails on the old reader.","path":"docs/issues/60-done/BPR-1437-read-command-lines-with-qcommandlineparser.md"}},{"criterion":"AC3","outcome":"pass","revision":"57e8d8eed","evidence":{"kind":"observation","detail":"The desktop parses once after QApplication with tests-only options gated; LAUNCH_ARGUMENTS_INVALID and PROFILE_LAUNCH_ARGUMENTS_INVALID observed offscreen; profile activation passes with new cases; 67 of 71 launching tests pass, and the 4 failures reproduce with the previous main.cpp.","path":"docs/issues/60-done/BPR-1437-read-command-lines-with-qcommandlineparser.md"}}]
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

- AC1: `src/app/src/command_line.cpp` (`bpr/app/command_line.hpp`) wraps
  `QCommandLineParser`. It refuses `--` and `--name=value` before Qt parses,
  reports Qt's unknown options (so `-j` is unknown), treats a value option
  given last as a missing value, refuses a value that starts with `--`, and
  refuses any name `optionNames()` lists twice.
- AC2: `tour_cli.cpp` and `cycle_cli.cpp` read through it; the cycle table
  comes from core's option catalogue. `tour_cli_public_entry` and
  `cycle_cli_public_path` gain cases for a value that is an option,
  `--name=value`, `--`, a single-dash word and a repeat. The cycle
  `note RC-1 --file --json` case fails on the old reader, which took `--json`
  as the file and answered in human text. All 10 `tour_cli_*`, `cycle_cli_*`
  and `skill_pr_tour_package` tests pass.
- AC3: `main.cpp` parses `QCoreApplication::arguments()` after QApplication
  against `shipped_launch_options`, plus `testing_launch_options` or the
  attended options. Values are kept as UTF-8 for the run. Offscreen runs
  print `LAUNCH_ARGUMENTS_INVALID` for `--ghes-host --new-window`, `--bogus`,
  `stray`, a repeated `--self-test` and `--github-fixture` with no value, and
  `PROFILE_LAUNCH_ARGUMENTS_INVALID` for the profile cases.
  `-platform offscreen --application-icon-probe` passes, so Qt's own options
  are removed first. `application_profile_activation_public_path` passes with
  six new refusal cases. The live-candidate seam takes
  `octocat/Hello-World/1`. `raw_has_argument` remains only for the macOS
  tests-off provenance check before QApplication.
- Desktop tests: 67 of the 71 tests that launch the application pass after
  missing helpers were built. `application_review_round_trip_composition_contract`
  passes after the inventory, manifest and digest reseal, which also covers
  BPR-1431's earlier `github_application.cpp` change.
  `application_uiforetw_selected_diff_contract`,
  `application_workbench_composition_contract`,
  `application_start_review_contract` and
  `application_appearance_theme_customization_contract` fail the same way
  with the previous `main.cpp` rebuilt (`presentation=0`, a theme probe
  stage), so this change does not cause them.
- Gates: changed-line clang-tidy 22 reports no findings, and the format check
  and `verify_documentation.py` pass.
