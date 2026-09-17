---
schema: "implement-rfc.card.v1"
id: "BPR-0862"
title: "The project formatting gate is red on main for seventeen files, and no sweep row would notice"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005 integrator, from an rfc-worker's incidental report on BPR-0860","found_at":"7d548233","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0862: The project formatting gate is red on main for seventeen files, and no sweep row would notice

## Defect

The repo-wide `coding-format-check` target
(`_coding-standard/scripts/format_check.py`) fails at main `7d548233`. It
checks 694 files and names seventeen:

- `src/adapters/qt/include/bpr/qt/conversation_cards_view.hpp`
- `src/adapters/qt/include/bpr/qt/github_session_panel.hpp`
- `src/adapters/qt/include/bpr/qt/home_list.hpp`
- `src/adapters/qt/include/bpr/qt/info_glyph.hpp`
- `src/adapters/qt/include/bpr/qt/profile_management_window.hpp`
- `src/adapters/qt/include/bpr/qt/server_card.hpp`
- `src/adapters/qt/include/bpr/qt/workbench_window.hpp`
- `src/adapters/qt/src/workbench_landing.cpp`
- `src/app/include/bpr/app/github_application.hpp`
- `src/app/src/profile_desktop.cpp`
- `src/application/include/bpr/application/home_view_state.hpp`
- `src/application/src/device_authorization_application_host.cpp`
- `src/application/src/github_application_host.cpp`
- `tests/adapters_qt/composed_github_tests.cpp`
- `tests/application/device_authorization_refresh_tests.cpp`
- `tests/application/workbench_application_host_tests.cpp`
- `tests/end_to_end/review_submission_composition_public_path.cpp`

**It is not a clang-format version drift.** A pristine `git archive HEAD`
export was checked twice, with Homebrew clang-format 22.1.8 (the project's
declared `MINIMUM_CLANG_FORMAT_VERSION`) and 23.1.0. Both flag the **identical
seventeen files**. The committed bytes are simply unformatted under every
supported version, so reformatting cannot break the older one. Most findings
are `AlignConsecutiveDeclarations` columns in member blocks.

**Why it went unnoticed.** `coding-format-check` is a build target, not a
ctest row. The one ctest row with a matching name,
`coding_standard_format_gate_contract` (#26), passes: it pins the gate
script's own behaviour, not the project's formatting. So a full ctest sweep —
the check this run added after eleven rows stayed red for days — cannot see
this class of rot at all.

`ce62ed52` is the most recent commit touching one of the named files; the
debt accumulated across several commits whose integration passes ran focused
checks.

## Scope

- Reformat the seventeen files with the project's own gate, and prove the
  change is whitespace-only: `git diff --ignore-all-space` must be empty.
  Commit it separately from any behaviour change, as `14723207` did for an
  earlier three-file drift.
- `src/app/include/bpr/app/github_application.hpp` and
  `src/app/src/profile_desktop.cpp` are in `bpr_production_app_sources`, so
  the change moves the round-trip contract's manifest digest. Reseal it in
  the same commit with the recorded review, per the procedure at the top of
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`.
  For a whitespace-only change the recorded review is the
  `--ignore-all-space` emptiness, named in the commit.
- Close the observation gap: give the repo-wide formatting gate a
  sweep-visible check, or name here why it must stay a build-target-only gate.
  Whatever the answer, a reader of a green full sweep should not be able to
  conclude the project is formatted when it is not.

## Acceptance

- `coding-format-check` passes at the fixed head under clang-format 22 and 23.
- The formatting commit is whitespace-only by `git diff --ignore-all-space`.
- The round-trip composition contract passes with resealed pins.
- A full ctest sweep would fail if these seventeen files regressed, or the
  card records the decision not to make that so and why.
