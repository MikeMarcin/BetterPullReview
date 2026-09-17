---
schema: "implement-rfc.card.v1"
id: "BPR-0862"
title: "The project formatting gate is red on main for seventeen files, and no sweep row would notice"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005 integrator, from an rfc-worker's incidental report on BPR-0860","found_at":"7d548233","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"coding-format-check passes at the fixed head under clang-format 22 and 23","outcome":"pass","revision":"working tree at 70a52630 plus the seventeen reformatted files","evidence":{"kind":"command","detail":"cmake --build build/r2-dev --target coding-format-check exits 0 with clang-format 23.1.0, and _coding-standard/scripts/format_check.py --clang-format llvm@22 reports 'Checking 694 file(s) ... Formatting check passed.' with 22.1.8, the declared MINIMUM_CLANG_FORMAT_VERSION. Log build/r2-logs/dsp5-0862-build.log."}},{"criterion":"The formatting commit is whitespace-only by git diff --ignore-all-space","outcome":"pass","revision":"working tree at 70a52630 plus the seventeen reformatted files","evidence":{"kind":"observation","detail":"Qualified and recorded on the card rather than claimed flat: fifteen of the seventeen files are empty under git diff --ignore-all-space. Two carry one alphabetical #include re-sort each inside an existing block (server_card.hpp in workbench_window.hpp; <QPushButton> in review_submission_composition_public_path.cpp). No other token changed. Because an include move is behaviour-preserving only for a self-contained header, that was verified by building every target in the tree, exit 0, build/r2-logs/dsp5-0862-build.log."}},{"criterion":"The round-trip composition contract passes with resealed pins","outcome":"pass","revision":"working tree at 70a52630 plus the reseal","evidence":{"kind":"command","detail":"The production manifest digest moved because github_application.hpp and profile_desktop.cpp are pinned; it is resealed to 9e83a672... in this card's commit on the review recorded on the card. The full-application digest is unchanged. application_review_round_trip_composition_contract passes in 11.14 s, in the 13/13 set at build/r2-logs/dsp5-0862-tests.log."}},{"criterion":"A full ctest sweep would fail if these seventeen files regressed, or the card records the decision not to make that so and why","outcome":"pass","revision":"working tree at 70a52630","evidence":{"kind":"observation","detail":"The decision is recorded on the card under its own heading: the row is deliberately deferred to BPR-0865 because the gate hard-fails without clang-format 22 or newer, so a ctest row would turn a station's sweep red for a toolchain reason rather than for formatting rot. Only the macOS versions are known; the dsp-005 station envelopes collect clang-format --version from bpr-win and bpr-linux, and BPR-0865 carries the guarded row with real version data. The named precondition, not an omission."}}]
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

## Fix and reseal (dsp-005, Mac)

**The reformat.** `_coding-standard/scripts/format.py` was run over the tree
with clang-format 23.1.0. It rewrote exactly the seventeen files named above.

**It is not purely whitespace, and the two exceptions matter.** Fifteen files
are whitespace-only: `git diff --ignore-all-space` shows nothing for them. Two
carry one alphabetical `#include` re-sort each, inside a single existing
include block:

- `src/adapters/qt/include/bpr/qt/workbench_window.hpp`: `server_card.hpp`
  moves after `review_submission_editor.hpp`.
- `tests/end_to_end/review_submission_composition_public_path.cpp`:
  `<QPushButton>` moves after `<QPlainTextEdit>`.

An include move is behaviour-preserving only if the header is self-contained,
so that is checked rather than assumed: **every target in the tree builds**
(`build/r2-logs/dsp5-0862-build.log`, exit 0), which is what proves
`server_card.hpp` and `<QPushButton>` do not depend on what preceded them.
No other token changed anywhere in the seventeen files.

**Both supported versions now agree.** At the fixed head the gate passes under
clang-format 23.1.0 (`coding-format-check`, exit 0) and under 22.1.8, the
declared `MINIMUM_CLANG_FORMAT_VERSION` — "Checking 694 file(s) …  Formatting
check passed." So the reformat cannot have traded the newer version's verdict
for the older one's.

**Reseal.** `src/app/include/bpr/app/github_application.hpp` and
`src/app/src/profile_desktop.cpp` are in `bpr_production_app_sources`, so the
production manifest digest moved and is resealed in this card's commit. The
full-application digest is unchanged, because `github_application.cpp` was not
one of the seventeen. The recorded review for the reseal is the evidence above:
fifteen files empty under `--ignore-all-space`, two single include moves read
and named, and a clean build of every target.

**Checks.** Thirteen rows covering the reformatted surfaces pass 13/13 in
`build/r2-logs/dsp5-0862-tests.log` (74.15 s): the resealed
`application_review_round_trip_composition_contract`, `architecture_boundaries`,
`test_network_isolation_manifest`, `application_device_authorization_refresh`,
`application_profile_management_host`, `qt_composed_github_scale_{100,125,200}`,
`qt_conversation_cards_view`, `qt_home_list_scale_100`,
`qt_profile_management_window`, `application_review_submission_composition_public_path`
and `application_workbench_composition_contract`. The documentation gate and
`git diff --check` pass.

## Criterion 4: the sweep-visibility decision, and why the row is deferred

**Decision: do not add the ctest row in this card.** The gate hard-fails
without a supported tool — `_coding_standard.py` refuses when `clang-format`
is absent, unreadable or older than 22, because pre-22 releases render `{}`
where this tree holds `{ }` and would call hundreds of files dirty. A ctest row
would therefore turn a station's whole sweep red for a **toolchain** reason
rather than for formatting rot, and the two are not the same finding.

Only this machine's versions are known (22.1.8 and 23.1.0, both clean). The
Windows and Linux stations' versions are not, and a full sweep is about to be
asked of both, so registering the row now would risk a station round trip to
discover a version number. The dsp-005 station envelopes ask each station for
`clang-format --version`; [[BPR-0865]] carries the row itself, guarded to skip
rather than fail where no supported tool exists, and records the decision with
real version data either way.

This is a deliberate deferral with a named precondition, not an omission.
