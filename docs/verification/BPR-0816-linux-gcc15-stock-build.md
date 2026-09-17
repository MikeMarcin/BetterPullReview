# BPR-0816 Linux GCC 15.2 stock build evidence

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-2`), mandate r2, task `task-20260917-lnx-build`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority
  `lane-commit`. Base `20a72bf7`.
- **State.** The Linux half of BPR-0816 is met at `4883dbd9`.
  - Both presets configure fresh and build clean with the stock flags under GCC
    15.2, with no diagnostic. No class is demoted and nothing is suppressed.
  - The focused Linux tests for every touched source show no regression.
  - An independent review found nothing blocking.
- **Open.** The card's second criterion also names the **macOS dev build**,
  which this Linux station cannot run. The card therefore stays In Progress
  with its Linux evidence recorded. Main finishes it after the macOS dev build
  and focused tests pass on the merge.
- **Task outcome.** The stock trees at `4883dbd9` then carried BPR-0622's and
  BPR-0806's Linux courses, BPR-0817's fix (`1c6eaf21`) and BPR-0799's native
  rerun, all without a flag override.

## Station, revisions and host

- **Claim.** `ef3f782b` claims BPR-0816 on base `20a72bf7`.
- **Fix.** `4883dbd9` holds every source change. Every result below that names
  the candidate was produced from a working tree byte-identical in `src/` and
  `tests/` to `4883dbd9`.

| Fact | Value |
|---|---|
| Host | JaegAW, WSL2, kernel `6.18.33.2-microsoft-standard-WSL2`, Ubuntu 26.04 LTS, 20 logical CPUs, 62 GiB |
| Compiler | GCC 15.2.0 (`Ubuntu 15.2.0-16ubuntu1`), `/usr/bin/c++`; glibc `_FORTIFY_SOURCE=3` in Release |
| Build tools | CMake 4.4.2, Ninja 1.13.2 |
| Qt | 6.10.3 `gcc_64` from the project's dependency cache |
| clang proxy | `clang++-22`; `clang-format` 22.1.8 (`/usr/bin/clang-format`); `clang-tidy` 21.1.8 |

## Before: stock builds at `ef3f782b`

Each preset was configured with `cmake --preset <preset> --fresh` (the cache
shows an empty `CMAKE_CXX_FLAGS`) and built with `cmake --build --preset
<preset> -- -k 0`.

| Preset | Result | Failing translation units |
|---|---|---|
| `app-release` | rc 1, 80 s, reached 284 of 425 steps | `profile_startup_admission.cpp`, `syntax_classification_application_host.cpp`, `appearance_preferences_application_host.cpp`, `review_ledger_database.cpp`, `github_mutation_contract.cpp`, `github_capability_probe.cpp` |
| `dev` | rc 1, 171 s, reached 1185 of 1237 steps | `github_capability_probe.cpp`, `profile_registry_storage_tests.cpp`, `queue_enrollment_storage_tests.cpp`, `github_mutation_contract_tests.cpp`, `live_review_submission_provider_tests.cpp`, `workbench_window_tests.cpp`, `dif_runtime_tests.cpp` |

`unix_agent_supervisor.cpp` no longer fails. Station commit `9bbe3604` fixed it
before this task.

**Discovery probe, not evidence.** A keep-going build stops at a failed
library, so separate trees `build/probe-app-release` and `build/probe-dev` were
built completely. Their compiler launcher appended `-Wno-error`. Both built, so
every site printed:

| Graph | Warnings | Where |
|---|---|---|
| `app-release` | 42 `-Wmaybe-uninitialized` | 33 in `profile_startup_admission.cpp`, 4 in `github_mutation_contract.cpp`, 2 in `review_ledger_database.cpp`, 2 in `syntax_classification_application_host.cpp`, 1 in `appearance_preferences_application_host.cpp` |
| `dev` | 14 `-Wdangling-pointer=`, 3 `-Wunused-function` | the five test files and `dif_runtime_tests.cpp` above |

The `-Wdangling-reference` in `github_capability_probe.cpp` is missing from the
probe only because it was already fixed. No site outside the card's list
exists.

## The fixes

Every site is fixed where GCC reports it. There is no `#pragma`, no
`-Wno-error` and no changed warning flag in the diff.

| File (all portable) | Diagnostic | Finding | Change |
|---|---|---|---|
| `src/adapters/github/src/github_capability_probe.cpp` | `-Wdangling-reference` at 1468 | Heuristic. The reference is bound to a call that received a temporary `QString`. | Look the alias up through the `std::string_view` overload, which the `QString` overload already delegated to. The temporary no longer exists. |
| `src/adapters/github/src/github_mutation_contract.cpp` | `-Wmaybe-uninitialized` at 986/993, 1016/1023, inlined at 1429 | **A real GCC miscompile.** See below. | `.Receipt = application::review_mutation_receipt{ … }` |
| `src/adapters/sqlite/src/review_ledger_database.cpp` | `-Wmaybe-uninitialized` at 1810/1816 | The same miscompile | `.Note = core::supplement_note{ … }` |
| `src/application/src/profile_startup_admission.cpp` | 33 × `-Wmaybe-uninitialized` in the admission move constructor, inlined through `std::expected` | False positive at `-O3` only; `-O2` is clean. The value alternative is read only when `has_value` is true. | `finish` is split into `retire`, `publish`, and one overload each for a failure and for an admission. Each terminal is built in place, so no admission-carrying `expected` is moved on the failure path. |
| `src/application/src/appearance_preferences_application_host.cpp` | `-Wmaybe-uninitialized` at 666/672 | Copying a reassigned `expected<void, E>` into the lane closure copies a union byte that is no longer initialized. | A static `perform_theme_export`, shaped like its neighbours `perform_settings_store` and `perform_theme_import`. It returns the same results: a stop or a throw gives `theme_document_error{ }`, which is `unavailable`. |
| `src/application/src/syntax_classification_application_host.cpp` | `-Wmaybe-uninitialized` at 313/316 | False positive on the moved-from temporary's `std::string` | `reserve` still calls `next_context()` before allocating, then builds the record in place with `emplace_back`. |
| `tests/sqlite_contract/profile_registry_storage_tests.cpp` (97, 195), `tests/sqlite_contract/queue_enrollment_storage_tests.cpp` (550), `tests/github_contract/github_mutation_contract_tests.cpp` (432), `tests/adapters_qt/workbench_window_tests.cpp` (6268) | `-Wdangling-pointer=` | Heuristic. A GIMPLE dump shows GCC keeping a pointer to a braced list for an exception cleanup that runs after the list's lifetime. | The expected value is a named `const` local declared just before its `expect`. Value, operator, short-circuit order and message are unchanged. |
| `tests/github_contract/live_review_submission_provider_tests.cpp` (222) | `-Wdangling-pointer=` | The same heuristic, in a `?:` arm | The fake transport builds the header vector in statements and returns exactly what it returned before. |
| `tests/adapters_qt/dif_runtime_tests.cpp` (169, 174, 183) | `-Wunused-function` | Dead code off macOS. The only caller is inside `#ifdef __APPLE__`. | The three helpers are defined under the same `#ifdef __APPLE__`. |

`workbench_window_tests.cpp` also carries two whitespace-only fixes, at lines
6121 and 6522. `clang-format` 22 required them, and that file already failed
the format check at the base.

**The GCC miscompile.** Take an aggregate whose member is itself an aggregate
holding a `std::string`. If that member is brace-initialized without naming its
type, and a later member's initializer throws, GCC 15.2 destroys the member
twice.

- **Reproducer.** Outside the tree, AddressSanitizer reports `attempting
  double-free` for both shapes at `-O0`. At `-O2` the process aborts with
  `free(): double free`.
- **The fix.** Naming the type avoids it.
- **Other compilers.** Clang 21 and 22 are correct.
- **Here.** In `github_mutation_contract.cpp`, the later member copies can throw
  `bad_alloc`, so an out-of-memory failure there would double-free. In
  `review_ledger_database.cpp`, the exception edge cannot fire in practice.
- **Scope.** The warning appears only at `-O3`. Other unwarned instances of the
  pattern may exist in the repository; none was surveyed.

## After: stock builds at `4883dbd9`

The script `stock-clean.sh` ran `cmake --preset <preset> --fresh`, then `cmake
--build --preset <preset> --clean-first`. The working tree was clean in `src/`,
`tests/`, `cmake/` and the preset files.

| Preset | Configure | Clean build | Steps | Duration | First-party diagnostics | Cache `CMAKE_CXX_FLAGS` |
|---|---|---|---|---|---|---|
| `app-release` | rc 0 | **rc 0** | 429 of 429 | 100 s (07:38:37Z–07:40:17Z) | **none** | empty |
| `dev` | rc 0 | **rc 0** | 1241 of 1241 | 149 s (07:40:17Z–07:42:46Z) | **none** | empty |

- **The one warning line.** The `app-release` log has one `warning:`, from the
  vendored SQLite amalgamation `_bpr/sqlite/3.53.4/sqlite3.c`
  (`-Wstringop-overread`). That third-party C target is not built with the
  repository's warning set and is not a `-Werror` site.
- **Executables.** The `app-release` image
  `build/app-release/src/app/better_pull_review` has SHA-256
  `dd29a6693bde5b8aaa94461764c50d38501a376979112ac689dba7f9b1903b93`, the same
  as the incremental candidate build. The `dev` image has
  `15278e919948f7404930cd46218cb70ba429aebe39b239c2d965cd2b2957309b`.

## Portability proxies and gates

These checks cover the clang and MSVC constraints as far as a Linux host can.
They are not the macOS or Windows build.

- **clang 22 proxy.** `tools/tu.py dev <file> --clang` compiles each of the
  twelve touched translation units with `clang++-22`, using the project's
  clang warning set from `cmake/Warnings.cmake`: `-Wall -Wextra -Wpedantic
  -Wconversion -Wsign-conversion -Wshadow -Wshadow-uncaptured-local -Werror`.
  - **All twelve pass (rc 0).** The run adds one flag,
    `-Wno-missing-designated-field-initializers`: clang 19 and later enable that
    class under `-Wextra`, and it already fires at the base on nine of these
    files.
  - **Nothing new with the flag removed.** The leaf owners compiled the base and
    the candidate without it. No message is new; the candidate has one fewer in
    `syntax_classification_application_host.cpp`.
- **clang-format.** `/usr/bin/clang-format` 22.1.8 through
  `_coding-standard/scripts/format_check.py` on the twelve files: `Formatting
  check passed.` `git diff --check` is clean.
- **clang-tidy.** 21.1.8 through `_coding-standard/scripts/tidy.py
  --build-dir build/dev --paths <twelve files>`: `clang-tidy passed`, 298 s.
  The remaining findings are not on changed lines: glaze option member names,
  `#pragma once` in unchanged headers, and `wait_for_frame` at
  `workbench_window_tests.cpp:7554`.
- **MSVC.** Not compiled. The new constructs are ordinary `/W4`-safe forms: no
  new local shadows another, no unused local, no unreachable code, no
  narrowing. The `#ifdef __APPLE__` block matches its callers exactly, so no
  unreferenced local function remains.

## Focused Linux tests

`ctest --preset dev -j1 --output-on-failure -R <23 names>` ran on the candidate
from 07:19:30Z, with ctest rc 8 after 371 s. Registered names were confirmed
with `-N`. **16 pass:**

- `github_capability_probe`, `github_mutation_contract`
- `live_review_mutation_provider`, `live_review_mutation_restart_recovery`
- `live_review_submission_provider`, `live_review_submission_restart_recovery`
- `application_threaded_reply_contract`
- `sqlite_review_ledger_storage`, `application_review_ledger_host`
- `application_profile_startup_admission`, `application_profile_startup_public_path`
- `application_appearance_host`, `application_appearance_profile_consumers`
- `sqlite_profile_registry_storage`, `sqlite_queue_enrollment_storage`
- `qt_dif_runtime`

**7 fail, and fail identically on the pre-change tree.** The same seven ran
through `ctest --test-dir build/probe-dev`. That tree is `ef3f782b` plus only
the one-line capability-probe change, built by the launcher that appends
`-Wno-error`, so code generation is unchanged. The run started at
07:26:04Z, took 356 s and failed 7 of 7. With durations, temporary paths and
tick counts normalized, the two outputs match line for line. Only the stage
tick counts differ.

| Test | Failure, before and after |
|---|---|
| `qt_workbench_window_scale_100`, `_125`, `_200` | `FAILED: the positions yield before the number does and keep their rule once it has gone` (`workbench_window_tests.cpp:1531`, which the diff does not touch) |
| `application_review_ledger_composition_contract` | Seed passes. Reopen fails with `REVIEW_LEDGER_COMPOSITION_REOPEN_FAILED`: the store load succeeds but holds no receipt. |
| `application_appearance_theme_customization_contract` | Reaches `export-and-directory-refusal-acknowledged`, then `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`, then an 80 s timeout |
| `application_appearance_gesture_contract` | `EPOCH_FIXTURE_SEED_FAILED`, `RUN_BUDGET_EXPIRED … stage=4` |
| `application_appearance_store_fallback_contract` | `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`, then a timeout |

These are pre-existing Linux failures, reported to the integrator and not
carded here. `syntax_classification_application_host.cpp` has no registered
test. Nothing in `src/` or `tests/` constructs that host, so its evidence is the
compile alone.

## Independent review

An `rfc-reviewer` reviewed the twelve-file diff read-only and found **no
blocking finding**.

- **Checked equivalent.** Every `finish` caller maps to the correct overload.
  The moved cancelled override is unobservable when no projection exists.
  `perform_theme_export` returns what the inline block returned. `reserve`
  keeps its identity ordering. The capability-probe lookup is identical. All
  five test rewrites keep value, operator, short-circuit order and message. The
  `__APPLE__` guard matches its callers.
- **Double free reproduced independently.** The reviewer reran the reproducer
  and confirmed the in-source comment is accurate.
- **Non-blocking.**
  - The comment names no GCC bug number.
  - The fix is a correctness workaround, not only a warning fix; this file
    says so.
  - The seven focused failures needed a baseline; it is recorded above.
  - The two whitespace-only lines are called out above.

## Against the card's acceptance

- **Criterion 1, both presets build stock on the Linux station: met at
  `4883dbd9`.** See "After" above. No class is demoted.
- **Criterion 2, the macOS dev build and focused tests still pass: Linux half
  only.**
  - The focused Linux tests show no change against the pre-change tree.
  - The clang 22 proxy, the format gate and the tidy gate pass.
  - **The macOS dev build and the macOS focused tests were not run.** This
    station cannot run them, and they remain open for main.

## Limitations

- **WSL2.** Builds ran in a WSL2 utility VM, not a full distribution kernel.
- **Proxies.** clang 22 on Linux stands in for AppleClang, and neither MSVC nor
  AppleClang compiled the change.
- **Logs.** Logs, the discovery trees and the reproducers stay outside the
  checkout under `/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/`
  (`stock.summary`, `*.build-*.log`, `probe-*.build.log`,
  `clang-proxy-candidate.log`, `tidy-0816.log`, `focused-0816.ctest.log`,
  `baseline-0816.ctest.log`, `tools/tu.py`, and the double-free reproducer
  `repro/asan.cpp`). None is committed. The discovery trees `build/probe-*`
  were deleted after use.
- **Boundaries.** No GitHub or GHES request, credential read, live agent or
  GitHub Actions run was used.
