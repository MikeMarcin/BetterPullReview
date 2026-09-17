---
schema: "implement-rfc.card.v1"
id: "BPR-0843"
title: "A non-canonical epoch fixture root prints STARTUP_FAILED without a cause"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0830 macOS half","found_at":"866c5b906309f7b45cf9ed46b04d07662d8fcb75","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0843: A non-canonical epoch fixture root prints STARTUP_FAILED without a cause

## Defect

While running [[BPR-0830]]'s macOS half, a `--self-test --github-fixture
github-com --epoch-fixture-root R` run whose `R` was spelled through a
symbolic link (`/var/folders/…` rather than `/private/var/folders/…`) printed
only `STARTUP_FAILED`, in all 10 seed and reuse phases. The same directory
spelled canonically passed
(`build/r2-logs/dsp3-0830-s2attempt1-noncanonical-*.log`,
`dsp3-0830-diag-canonical-seed.log`). The profile storage root requires a
canonical path (`src/adapters/sqlite/src/profile_storage_root.cpp` ~53). That
refusal is correct, but nothing in the output names it. [[BPR-0834]] found
the same bare refusal on Windows, where it needed a reverted diagnostic to
read `startup_failure=5`.

## Scope

- Make an automated run's startup refusal name its `startup_failure` value
  (for example `STARTUP_FAILED:registry` or a following line). Keep the
  refusal and the markers other courses match, and change no interactive
  behaviour.
- Check which courses match `STARTUP_FAILED` exactly, and keep them passing.

## Acceptance

- A non-canonical fixture root's run names the refusal's cause.
- The courses that match `STARTUP_FAILED` still pass on macOS.

## Current frontier (dsp-003, bpr-linux, task-20260917-lnx-epoch-and-startup)

- **Outcome:** implemented at `0c66037f03a3781000218a3d3544d8c6fcd9e38d` on
  `linux-lane`. Acceptance 1 passes on Linux. Acceptance 2 names macOS, so
  the card stays In Progress until main runs it there.
- **Changed paths:** `src/app/main.cpp`, `tests/end_to_end/CMakeLists.txt` and
  the new `tests/end_to_end/epoch_fixture_noncanonical_root_contract.cmake`.
- **Logs:** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-epoch-and-startup/`
  (`pre-0843-*`, `post-0843-*`, `course-0843-*`, `batch-0843*`, `recheck-*`,
  `tidy-0843-main.log`, `pinned-digests-0843.log`). None is committed.
- **Still open:** the macOS runs listed under "For macOS and Windows", and the
  round-trip contract's reviewed reseal on main.

## Fix (dsp-003, bpr-linux)

**What the output is now.** A developer build prints one extra line when the
composition refuses startup:

    STARTUP_FAILED_CAUSE: startup_failure=<name>

- It goes to **stderr**, in any `BPR_BUILD_TESTING` build. The name is the
  `github_application_startup_failure` enumerator. An exhaustive switch with
  no default turns it into text, in the existing testing-only helper block in
  `src/app/main.cpp`.
- The **stdout verdict is unchanged**: it is still the one word
  `STARTUP_FAILED`, with the same exit code 1.
- `owner_busy` still prints `PROFILE_ALREADY_OPEN` and gets no cause line. The
  `activation_*` branches return before the new code.
- A tests-disabled build compiles the line away and prints nothing new. The
  attended designated-live refusals stay as uninformative as they were
  designed to be.

**Why this form.** It is the form [[BPR-0435]] already chose for exceptions:
the stdout verdict stays one word, and a developer build explains on stderr.
Nothing matches the word, so both forms the card offered would have kept
every match. Following that precedent needed no new decision.

**Interactive runs.** The same completion also serves interactive launches.
An interactive refusal in a developer build therefore writes the stderr line
too, just as BPR-0435's exception line does. Nothing a person sees changes:
no window, stdout or exit code is different.

## The STARTUP_FAILED inventory (`git grep` at `e3047a116ea68ad7d5591edb7a8f1b9dc713c8b5`)

- **Printers.** `src/app/main.cpp` prints the word at 7564, 7607, 7642, 7648,
  7658, 7668, 7674, 7692, 7703, 7709, 7714, 7726, 8522 (the refusal this card
  changes), 8663, 8705 and 8708, plus `STARTUP_FAILED_CAUSE` at 8701.
  `src/app/src/github_application.cpp:4668` mentions it only in a comment.
- **Matchers.** Nothing matches the word in `tests/`, `cmake/`, `scripts/`,
  `CMakeLists.txt`, `CMakePresets.json`, `packaging/`, `_coding-standard/`,
  `.claude/` or `.mask/`. That includes every `PASS_REGULAR_EXPRESSION`,
  `FAIL_REGULAR_EXPRESSION` and course driver. `PROFILE_ALREADY_OPEN` and
  `PROFILE_ACTIVATION_*` have no matchers either.
- **Documents.** The rest are prose in cards, verification notes and the
  protected operator packets v002 to v005. Those describe the tests-disabled
  attended refusal, which is unchanged.
- **Matches updated:** none. The independent review repeated the search and
  found the same. It also checked the literal-count inventories over
  `main.cpp`, and found no check that the application's stderr is empty.

## Verification (Linux, bpr-linux, WSL2)

Every row runs offscreen by registration (`QT_QPA_PLATFORM=offscreen`, and on
Linux `QT_STYLE_OVERRIDE=Windows`). The build is the stock `dev` preset. The
Secret Service `login` collection read `(<false>,)` before every course.

**The defect, reproduced before the change.** The command was
`better_pull_review --github-fixture github-com --epoch-fixture-root
<dir>/link/root --epoch-fixture-phase seed`, with `link` a symbolic link to a
0700 directory. It exited 1 after 57 ms. Stdout was only `STARTUP_FAILED` and
stderr was empty. The same directory spelled canonically seeded, exit 0, in
1.9 s.

**After the change.** The linked spelling prints the same stdout and adds
`STARTUP_FAILED_CAUSE: startup_failure=registry` on stderr. The card's own
`--self-test` form, in the reuse phase, prints the same pair. The refusal is
`registry`, the value [[BPR-0834]] read on Windows as `startup_failure=5`.

**Focused check.** The new course is
`application_epoch_fixture_noncanonical_root_cause`, registered on macOS and
Linux only. It:

- spells a 0700 root through a symbolic link, and proves the spelling is not
  canonical with `IS_SYMLINK` and `REAL_PATH`;
- requires exit 1, stdout exactly `STARTUP_FAILED\n`, and the `registry`
  cause line on stderr;
- then seeds the same directory spelled canonically, requiring
  `PUBLIC_PATH_OK`, `EPOCH_FIXTURE_SEED_OK` and `GITHUB_FIXTURE_OK:github.com`.

Windows is left out for two reasons. It has no unprivileged symbolic link.
And its contained runs refuse with `registry` for a reason of their own
([[BPR-0834]]), so the pin could pass there for the wrong reason.

**Reverted trial.** With only the `main.cpp` change reverted, the course
fails with `the refusal did not name startup_failure=registry`.

**Gates at the candidate:**

- `git diff --check`: clean.
- `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`.
- `/usr/bin/clang-format` 22.1.8 `--dry-run --Werror` on `main.cpp`: 7
  findings before the change and the same 7 after, none in a changed line.
  They are recorded as a pre-existing defect in the dsp-003 return.
- clang-tidy 21.1.8, through `_coding-standard/scripts/tidy.py --paths
  src/app/main.cpp --warnings-as-errors`: one finding,
  `bugprone-optional-value-conversion` at `main.cpp:3312`, in code this card
  does not touch. The station has no clang-tidy 22.

The courses ran serially with `ctest --test-dir build/dev -j1 -V -R
'^<name>$'` at `0c66037f`, from 13:58:28Z to 14:08:16Z, at load 0.3 to 1.8:

| Course | Result | Time |
|---|---|---|
| `application_epoch_fixture_noncanonical_root_cause`, 5 runs | Pass 5/5, `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry` | 2.27 to 2.36 s |
| `application_epoch_persistence_github-com` | Pass | 3.69 s |
| `application_epoch_persistence_ghes-3_18` | Pass | 3.62 s |
| `application_epoch_maintenance_public_path` | Pass | 5.38 s |
| `application_epoch_portable_occurrence_two_catalogues` | Pass | 11.62 s |
| `application_epoch_ordinary_update_recreated` | Pass | 4.24 s |
| `application_anonymous_public_preview_public_path` | Pass | 1.51 s |
| `application_github_public_path_github_com` | Pass | 1.18 s |
| `application_native_appearance_probe_contract` | Pass | 2.67 s |
| `architecture_boundaries` | Pass | 8.87 s |
| `architecture_boundary_contract` | Pass | 134.44 s |
| `test_network_isolation_structure_contract` | Pass | 0.76 s |
| `test_network_isolation_manifest` | Pass | 1.18 s |
| `documentation_structure` | Pass | 0.55 s |
| `application_review_round_trip_composition_contract` | **Fail, expected**: `production application source content digest drifted: expected c1b3c219…, found 827fe58a…` | 0.05 s |
| `application_composition_contract` | **Fail, same cause**: 6 of 7 pass; `test_async_mutation_host_inventory_rejects_every_direct_bypass` runs the round-trip contract as its baseline and stops on the same drift | 394.54 s |
| `application_profile_activation_public_path` | **Fail, pre-existing**: `FAILED: owner orderly drain`. It fails the same way twice with `main.cpp` at `98262aee` (`recheck-activation-base-*`), and the failing owner is `test_profile_activation_public_path`, which does not compile `main.cpp` | 1.25 s |
| `application_native_notification_attended_contract` | **Fail, pre-existing**: `NATIVE_NOTIFICATION_REFUSED:platform_tray_not_composed`, the WSLg limit on [[BPR-0156]]. Same at `98262aee` | 0.41 s |

**Pinned digests.** None was resealed. `cmake -DBPR_PRINT_PINNED_DIGESTS=ON
-DBPR_SOURCE_ROOT=/home/mmarcin/bpr-linux -P
tests/end_to_end/application_review_round_trip_composition_contract.cmake`
prints these values at `0c66037f`:

| Pin | Pinned (after main's `417ef7b0`) | Printed at `0c66037f` |
|---|---|---|
| `full_application` | `4f41706511c2d40b95809247b1d7f92a49971c4e243699f781c71f4c7ea79b98` | unchanged |
| `production_app_manifest` | `c1b3c219d4e84b53d0a2fbc01ea4b687fe5c25f6a6521708bf7cc69d85056a26` | `827fe58ab3fa55b3bbda97225c214f9460b03e3555f6289de884fbad7cccb754` |
| `production_mutation_branch` | `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a` | unchanged |

`0c66037f` is the only change inside the pinned scope since the pins last
matched.

**Independent review.** An `rfc-reviewer` reviewed the uncommitted candidate
(diff sha256 `a4650a1b37c57ae2209ada36e7501a1f13d5dca5036e5cd5c649796d9188c9a4`,
byte-identical to `0c66037f` over the three paths). It found **no blocking
finding** and confirmed:

- the new code sits entirely inside `BPR_BUILD_TESTING`;
- no attended check can be told apart, because a testing build refuses the
  approval flag before the desktop starts;
- `owner_busy` and `activation_*` are unaffected, and the switch is
  exhaustive and follows the naming standard;
- no matcher, literal inventory or empty-stderr check breaks;
- the contract proves a non-canonical spelling and has a canonical control;
- the registration respects the isolation-manifest rules and the bound
  arithmetic (244000 ms within 255000 ms);
- only `production_app_manifest` drifts.

Its non-blocking notes are all taken up on this card: it asked for the two
failures above to be checked at base (done), for the interactive
developer-build stderr line to be stated, and for the pre-existing format and
tidy findings to be recorded.

## For macOS and Windows

- **macOS:** run `application_epoch_fixture_noncanonical_root_cause`, which
  is new; it is acceptance 2's evidence that the refusal names its cause
  there. After main's reviewed reseal, run
  `application_review_round_trip_composition_contract` and
  `application_composition_contract`. Also run
  `application_epoch_persistence_github-com` as a spot check.
- **Windows:** no course changed or was added. The `main.cpp` change still
  has to compile in the Windows developer build. A contained run's refusal
  will now carry `STARTUP_FAILED_CAUSE: startup_failure=registry` on stderr.
