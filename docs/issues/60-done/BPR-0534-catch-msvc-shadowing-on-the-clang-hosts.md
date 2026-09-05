---
schema: "implement-rfc.card.v1"
id: "BPR-0534"
title: "Catch MSVC's shadowing error on the clang hosts"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: []
---

# Catch MSVC's shadowing error on the clang hosts

## Why

`src/app/main.cpp` landed on 2026-09-03 with an inner `auto passed`
declared while an outer `passed` was live ([[BPR-0316]] part one). Apple
Clang and GCC built it silently under the tree's warning set; MSVC raised
C4456 and, with warnings as errors, refused to build the Windows dev tree,
which blocked [[BPR-0315]] and [[BPR-0515]] on the station until the
integrator renamed it. The same class — a warning one compiler treats as an
error and the others never emit — has now cost a station round trip four
times this week (C4189 under [[BPR-0518]], C4996 under [[BPR-0519]],
quoting and CRLF under [[BPR-0522]], and this).

## Required final state

- The clang and GCC warning sets enable local-scope shadowing diagnostics
  (`-Wshadow` or the narrowest flag that matches C4456's scope) as errors in
  the same configurations that treat warnings as errors on MSVC.
- Every existing shadow the flag surfaces is fixed by renaming, not by
  narrowing the flag; the count is recorded on this card.
- `docs/development/build-and-test.md` names the cross-compiler warning
  classes the tree treats as errors and where each is enforced.

## Non-goals

- Enabling every MSVC-only diagnostic on clang; only the shadowing class this
  card names.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception; the card has no dependencies. The C4456 that
motivated it was a nested `auto passed` in [[BPR-0316]]'s live-candidate
verdict, invisible to clang until the Windows station built it. The
integrator first surveys the tree in a scratch build with `-Wshadow`
demoted to a warning, records the count, then enables the flag as an
error and renames every site it names.

## What this slice delivered — 2026-09-03

`cmake/Warnings.cmake`'s `bpr_enable_warnings` now adds `-Wshadow` and
`-Wshadow-uncaptured-local` on clang and AppleClang and `-Wshadow=local` on
GCC ahead of `-Werror`, so a local that hides an earlier local or a
parameter fails every first-party target on every host the way C4456
already failed it under `/W4 /WX`. GCC's local-scope form is the deliberate
choice: its plain `-Wshadow` also rejects a local hiding a member or a
global, which MSVC does not. Clang needed the second flag because its
`-Wshadow` alone skips the commonest shape in this tree — a lambda local
hiding an enclosing local the lambda does not capture — which GCC's
`-Wshadow=local` and C4456 both report.

The count came in two rounds, and the second corrected the first. A scratch
clang build with `-Wshadow` demoted named one site,
`src/app/src/designated_live_macos_capsule_identity.cpp:207` (`relative`
hiding the moved-from `relative` above; renamed `childPrefix`). The Linux
station (task `task-20260903-0534-lin`, GCC 15.2.0) then reported the
flag-as-error build failing on four more, all inner lambdas re-declaring an
enclosing name for a related but different value: `input` in
`tests/fixtures/agent/agent_canary_main.cpp:549` (a stream, renamed
`stream`), `state` in `tests/core/epoch_policy_tests.cpp:2175` (renamed
`pairState`), and in `src/application/src/epoch_application_host.cpp` the
locked `control` at `:1524` (renamed `lockedControl`) and the comparison
anchor `identity` at `:5789` (renamed `anchorIdentity`, two identity types
under one name). The inner name was renamed in every case; the outer kept
its meaning. A second clang survey with `-Wshadow-uncaptured-local` demoted
and `-k 0` then named 47 site(s): fifteen files, from one in the GitHub provider and one in the native HTTPS transport to fourteen in the review-queue enrollment and eight in the epoch host, every one an inner lambda re-declaring an enclosing name. The final
GCC rerun at `629b8f49` reported zero shadow diagnostics by message text (`BUILD_EXIT=0`, 861/861 edges built keep-going, GCC 15.2.0, `-Wshadow-uncaptured-local` absent from all 284 GCC command lines).

The station also found that the task's prescribed greps were a false
green: GCC labels a promoted warning `[-Werror=shadow=local]`, which never
contains `Wshadow`, and a single-pass ninja build stops at the first
failing object, so the count it prescribed would have read a failing build
as clean. The rerun task greps the message text and builds with `-k 0`;
the lesson is recorded outside the tree.

`docs/development/build-and-test.md` gained "Warning classes every compiler
treats as errors" under the coding standard, naming the enforcement point,
the shadowing and narrowing classes with their flag on each compiler, and
the MSVC-only classes (C4189, C4996) that have failed the Windows station
before and are not covered.

| Claim | Command | Result | Commit |
| --- | --- | --- | --- |
| The tree builds clean under the new errors on clang | `cmake --preset dev && cmake --build --preset dev` after every rename | exit 0, zero shadow diagnostics | this commit |
| The clang sites were real and are gone | scratch builds with the flags demoted (`-Wno-error=shadow`, then `-Wno-error=shadow-uncaptured-local` with `-k 0`) before each rename; the dev rebuild after | 1 → 0, then 47 → 0 | this commit |
| The GCC sites were real and are gone | Linux station: fresh `dev` configure and `-k 0` build at the merged head, every `shadows a previous` line quoted | four at `fbfdc920`; zero shadow diagnostics by message text (`BUILD_EXIT=0`, 861/861 edges built keep-going, GCC 15.2.0, `-Wshadow-uncaptured-local` absent from all 284 GCC command lines) at `629b8f49` | the station's `task-20260903-0534-lin-2` reply; lane at 629b8f49 |
| The renamed code still proves what it proved | `ctest --preset dev -R '^(core_epoch_policy\|core_agent_session\|application_epoch_host\|application_epoch_contract\|application_epoch_catalog_contract\|qt_agent_launch_canary)$'` and `ctest --preset dev -R 'capsule\|^application_composition_contract$'` | 6/6 epoch, session, and canary rows for the GCC round; 29/29 composition-selected rows for the second clang round; 3/3 | this commit |
| The touched files tidy and format clean | `tidy.py --warnings-as-errors --paths <files>`; `clang-format --dry-run --Werror` | exit 0 on every touched file, in chunks; `FORMAT_OK` | this commit |
| The composition pins did not drift | `ctest --preset dev -R '^application_review_round_trip_composition_contract$'` | passed first time (the digest covers `github_application.cpp`, untouched) | fbfdc920 |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |

One ordering lesson from the rerun: the head it built, `629b8f49`, was
pushed one integrator step before its two composition pins were recomputed
(`7ceef3ed`), so the station's `application_composition_contract` failed on
the first pin and would have failed again on the second after a 250-second
round trip. The station computed both correct values read-only and they
matched the integrator's repin exactly; the RFC-0016 replay envelopes name
a base commit whose pins are already correct.
