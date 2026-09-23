---
schema: "implement-rfc.card.v1"
id: "BPR-0882"
title: "The startup exception boundaries collapse every cause into one word, even in test builds"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review: the Claude/fable src/app partition and Codex independently","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"A startup failure in a developer or test build names its cause, shown in a real failing run","outcome":"pass","revision":"8e080bbc","evidence":{"kind":"command","detail":"bpr-main-linux (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, build/dev Debug, tests enabled). better_pull_review --self-test with no Git on the search path: before (fe2216e3) stderr said only STARTUP_FAILED_CAUSE: startup_failure=unavailable; after, it adds STARTUP_FAILURE_SOURCE:profile-runtime-construction: Application data or installed Git is unavailable, stdout still exactly STARTUP_FAILED, exit 1. The new row application_startup_failure_source pins that and failed with publish's record removed. Temporary uncommitted probes drove seven more arms through the sandboxed self-test, and each named itself with its caught text.","path":"docs/issues/60-done/BPR-0882-startup-failure-arms-discard-the-cause.md#every-arm-named-read-from-a-real-failing-run-bpr-main-linux-2026-09-23"}},{"criterion":"The noncanonical-root pin still passes, against whatever name replaces registry","outcome":"pass","revision":"8e080bbc","evidence":{"kind":"command","detail":"No enumerator is split: BPR-0874 closed keeping registry as one arm, and BPR-0877's profile_startup_refusal_name() names the refusal behind it. The desktop's own registry host, which also says registry, now records desktop-profile-registry on its own line. application_epoch_fixture_noncanonical_root_cause passes before and after the rebase (EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry).","path":"tests/end_to_end/CMakeLists.txt"}},{"criterion":"No production diagnostic leaks a path, token or repository name that it did not leak before; see the redaction properties BPR-0891 covers","outcome":"pass","revision":"8e080bbc","evidence":{"kind":"observation","detail":"Every new output line is inside BPR_BUILD_TESTING, and in a tests-disabled graph the recorder drops what() without reading it, so a shipped build prints and retains nothing it did not before. The card records why production keeps the single word: what() is free text, and the runtime's std::filesystem::canonical can put the data root, under the person's home, into it. application_composition_contract, which compiles the tests-disabled graph, passes (447 s). application_logging, BPR-0891's redaction course, passes in the 34-row integration pass. The typed diagnostics log is unchanged.","path":"docs/issues/60-done/BPR-0882-startup-failure-arms-discard-the-cause.md#every-arm-named-read-from-a-real-failing-run-bpr-main-linux-2026-09-23"}}]
---

# BPR-0882: The startup exception boundaries collapse every cause into one word, even in test builds

## Defect

Two reviews found this separately, and this dispatch paid for it directly:
`bpr-win` had to add a temporary print to `github_application.cpp` just to
learn which startup failure its five red rows were hitting ([[BPR-0874]]).

Three boundaries each throw the cause away:

- `src/app/src/github_application.cpp:12633-12655` — the `catch (...)` around
  `github_profile_runtime` construction, `start_services()` and `open_window`
  (which throws `runtime_error{"initial window unavailable"}`) collapses to
  `github_application_startup_failure::unavailable`, and `main.cpp:8565`
  prints only `startup_failure=unavailable`. A window-wiring failure is
  indistinguishable from a workspace-root throw at `:4866`.
- `src/app/main.cpp:8708` — the desktop completion callback's boundary emits
  only `STARTUP_FAILED`. The **outer** handler at `:8741` deliberately prints
  `error.what()` in test builds, but nothing caught by the inner boundary ever
  reaches it. So the diagnostic that exists is unreachable for this class.
- `src/app/src/profile_desktop.cpp:734` and `:788` — construction and
  window-wiring collapse arbitrary exceptions into `unavailable` too.

[[BPR-0874]] names the sibling `registry` arm, which hides seven distinct
causes. This card is the same disease in the other arms, and the two should
probably be fixed in one pass.

## Scope

- Carry `what()` through to the stderr line **in developer and test builds**,
  where it costs nothing and is what every automated course reads. Production
  can keep the single word if there is a reason to, but say what the reason
  is.
- Give the arms distinct names where the causes are genuinely distinct, rather
  than one catch-all per boundary.
- `tests/end_to_end/CMakeLists.txt:2295-2296` pins
  `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry`. When BPR-0874 splits
  that arm, **this pin has to move with it** or it will fail for the right
  reason. Coordinate the two.

## Acceptance

- A startup failure in a developer or test build names its cause, shown in a
  real failing run.
- The noncanonical-root pin still passes, against whatever name replaces
  `registry`.
- No production diagnostic leaks a path, token or repository name that it did
  not leak before; see the redaction properties [[BPR-0891]] covers.

## Every arm named, read from a real failing run (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux`: WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, `build/dev`
(Debug, tests enabled), serial and offscreen. Logs are under
`build/logs/bpr-0882/` and are not committed.

**The premise, rechecked at `fe2216e3`.** The line numbers have moved but the
arms have not. Each still dropped what it caught:

- The one `catch (...)` in `github_application_startup`'s `publish`
  (`src/app/src/github_application.cpp`) still turns three steps into
  `unavailable`: building the profile runtime, `start_services()`, and the
  compatibility path's own window.
- The completion boundary in `src/app/main.cpp` still printed only
  `STARTUP_FAILED`. The outer handler's `STARTUP_FAILED_CAUSE: <what()>`
  never saw its exceptions.
- In `src/app/src/profile_desktop.cpp` the card's two lines are now
  `start_runtime`'s factory catch and `runtime_ready`'s profile-management
  catch. Nine more arms report `unavailable` with nothing beside it. The
  recovery-drafts worker also swallowed its exception outright.
- [[BPR-0874]] closed with `registry` kept as one enumerator. The profile
  refusal behind it is named by `profile_startup_refusal_name()`
  (`STARTUP_PROFILE_REFUSAL:`), so no enumerator is split here and the pin
  does not move.

**Before.** With no Git on the search path, `better_pull_review --self-test`
built at `fe2216e3` exits 1 with only this output:

    stdout: STARTUP_FAILED
    stderr: STARTUP_FAILED_CAUSE: startup_failure=unavailable

The runtime's constructor had thrown `Application data or installed Git is
unavailable`, and nothing said so.

**The change.** One recorder sits beside [[BPR-0877]]'s. `startup_failure_source()`
reads it and `detail::note_startup_failure(name, cause)` writes it. The
first refusal wins, and a mutex guards it. Each arm records a fixed name
before it refuses. A catch also passes `std::current_exception()`, and a
tests-enabled graph keeps that exception's text after the name, on one line.
A tests-enabled `main` prints it after the existing lines:

    STARTUP_FAILED_CAUSE: startup_failure=unavailable
    STARTUP_FAILURE_SOURCE:profile-runtime-construction: Application data or installed Git is unavailable

The existing cause line keeps its exact wording, and stdout keeps its one
word. The three courses that start the application themselves print the same
value as `LARGE_REVIEW_STARTUP_SOURCE:` and `SEED_STARTUP_FAILURE_SOURCE:`,
beside their [[BPR-0877]] refusal line. No caller reads a different failure,
exit code or ordering.

| Arm | Where | Reports |
| --- | --- | --- |
| `profile-runtime-construction` | `publish`, building `github_profile_runtime` | `unavailable` |
| `profile-services-start` | `publish`, `start_services()` | `unavailable` |
| `compatibility-window-open` | `publish`, the non-desktop path's own window | `unavailable` |
| `desktop-runtime-factory` | `start_runtime`, the factory threw | `unavailable` |
| `desktop-runtime-factory-empty` | `start_runtime`, the factory returned nothing | `unavailable` |
| `desktop-startup-serial-exhausted` | `start_runtime`, no startup serial left | `unavailable` |
| `desktop-update-recovery-drafts` | `runtime_ready`, the saved drafts did not load | `unavailable` |
| `desktop-profile-management` | `runtime_ready`, management start or reload threw | `unavailable` |
| `desktop-window-not-current` | `open_window`, the context was no longer current | `unavailable` |
| `desktop-window-layout` | `open_window`, `configure_desktop_layouts` refused | `unavailable` |
| `desktop-window-open` | `open_window`, the window did not open or threw | `unavailable` |
| `desktop-window-wiring` | `open_window`, wiring or publication threw | `unavailable` |
| `desktop-profile-registry` | the desktop's own registry host failed | `registry` |
| `update-transaction-argument` | macOS, the update transaction argument | `unavailable` |
| `update-lifetime-join` | macOS, joining the update lifetime | `unavailable` |
| `update-healthy-start-timeout` | macOS, no healthy start within 30 s | `unavailable` |
| `update-healthy-start-receipt` | macOS, the healthy-start receipt was not recorded | `unavailable` |
| `entry-point-run-setup` | `main`'s completion boundary, after the window exists | (none; `STARTUP_FAILED`) |

`desktop-profile-registry` is there because two different arms say
`registry`. One is the profile mapping, which [[BPR-0877]] names. The other is
the desktop's registry host, which nothing named.

**Why production keeps the single word.** Two reasons:

1. `what()` is free text from whatever threw, and it can carry a path. The
   runtime's constructor calls `std::filesystem::canonical` on the data root,
   and a `filesystem_error`'s `what()` names that path. The data root sits
   under the person's home folder, so the path can include their account name.
   The redaction contract [[BPR-0891]] checks covers the typed diagnostics log,
   not exception text. That log belongs to the runtime whose construction
   failed, so it cannot carry this cause either. A tests-disabled graph's
   recorder drops the text without reading it, so no later reader can print
   it.
2. The fixed name alone would be safe to print. But no course runs a
   tests-disabled startup failure, so a production line would be a promise that
   nothing holds. The startup lines have been developer-build lines since
   [[BPR-0435]] and [[BPR-0843]]. Naming the arm in a shipped build would need
   its own tests-disabled course. That is a possible follow-up, not this card.

**The pin.** Nothing is split, so `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry`
stays as it was, and the row passes.

**A regression course.** `application_startup_failure_source` runs
`--self-test` with an empty search path. It requires exit 1, stdout exactly
`STARTUP_FAILED\n`, the unchanged cause line, and
`STARTUP_FAILURE_SOURCE:profile-runtime-construction: Application data or
installed Git is unavailable`. It is registered on macOS and Linux only. On
Windows the build tree loads Qt through that same search path ([[BPR-0430]]),
so the process would exit before `main`. With `publish`'s record removed, the
course fails with `the refusal did not say STARTUP_FAILURE_SOURCE:...`.

**Every other arm, driven for real.** Temporary probes were added, then
removed and never committed. Each probe, gated by an environment variable,
threw or refused at one arm. The shipped self-test then ran through the
network sandbox with the automated-test marker, and each run named its arm:

| Probe | Exit | Source line |
| --- | --- | --- |
| none | 0 | none; the self-test passes |
| `start_services()` throws | 1 | `profile-services-start: probe: services` |
| factory throws | 1 | `desktop-runtime-factory: probe: factory` |
| layout refused | 1 | `desktop-window-layout` |
| window open throws | 1 | `desktop-window-open: probe: open` |
| wiring throws | 1 | `desktop-window-wiring: probe: wiring` |
| management start throws | 1 | `desktop-profile-management: probe: management` |
| drafts load throws (worker lane) | 1 | `desktop-update-recovery-drafts: probe: drafts` |
| entry point run setup throws | killed by `timeout` | `entry-point-run-setup: probe: entry` |

The candidate was restored from a byte-identical snapshot afterwards.

**The macOS arms.** The four update-lifetime arms compile only on macOS. On
Linux, `profile_desktop.cpp` was type-checked with its eight `__APPLE__`
blocks forced on (GCC 15.2, `-fsyntax-only -Werror`, the file's own flags):
`APPLE_BLOCKS_SYNTAX_OK`. A negative trial misspelled
`note_initial_refusal` in `update-lifetime-join`, and the same check failed
with "has no member named". No Mac built this.

## Found on the way: a throw in the entry point's run setup never exits

The `entry-point-run-setup` probe printed its source line and `STARTUP_FAILED`,
then kept running until `timeout` killed it (exit 124). The same probe with
the inner catch exactly as it is at `fe2216e3` does the same thing: exit 124
at 30 s, `STARTUP_FAILED`, no source line. So it is older than this card. The
catch calls `desktop->quit(EXIT_FAILURE)`. That is the orderly Quit, which
"leaves windows usable" when refused, and the window is already open. A course
that hits it prints the verdict and then runs until the harness kills it.
Not fixed here. It needs its own card.

## Pinned-code review (BPR-0882 reseal)

`github_application.cpp`, `github_application.hpp`, `profile_desktop.cpp` and
`main.cpp` are in pinned scope. The pins matched at `fe2216e3`, and nothing
since `3dca2d7a` had touched pinned scope before this change.

- `github_application.cpp`: a mutex-guarded record, first refusal wins, and
  its reader. `publish` names each step before it runs. Its catch records
  only while the startup is unfinished (`!Terminal`), so a throw from a
  completion that already ran is not named as a refusal. The recorder is
  `noexcept` and catches everything, so a catch block that calls it cannot
  begin to throw. The text of `what()` is kept only in a tests-enabled graph.
- `profile_desktop.cpp`: each arm calls `note_initial_refusal` before its
  existing `finish_initial`. It records only while the initial completion is
  still owed. `open_window`'s `layout && open` became two nested `if`s with
  the same short-circuit, and the same result when either throws. The
  drafts worker hands its exception to `runtime_ready` rather than
  dropping it.
- `main.cpp`: two stderr additions, both inside `BPR_BUILD_TESTING`.
- No credential, provider, vault, transport or mutation path changes. The
  mutation-branch pin is unchanged (`a7e2081e…`). The full-application pin
  and the production manifest pin moved.

## Verification at the candidate (bpr-main-linux)

The candidate is `8e080bbc` on main. The rows below ran on it before the
rebase onto `b9991925`. On the rebased commit the isolation gates, the new
row, the pin, the pinned contract and `application_public_path` passed again.

| Check | Result |
| --- | --- |
| No Git on the search path, `--self-test`, before (`fe2216e3`) | exit 1; `STARTUP_FAILED`; stderr `startup_failure=unavailable` only |
| The same run with the change | exit 1; `STARTUP_FAILED`; adds `STARTUP_FAILURE_SOURCE:profile-runtime-construction: Application data or installed Git is unavailable` |
| `application_startup_failure_source` | Passed; failed as it should with `publish`'s record removed |
| `application_epoch_fixture_noncanonical_root_cause` | Passed, `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry` |
| Eight probed arms and the unprobed self-test | As in the probe table above |
| `profile_desktop.cpp` with its `__APPLE__` blocks forced on | `APPLE_BLOCKS_SYNTAX_OK`; the misspelling trial failed |
| `application_review_round_trip_composition_contract` | Passed with the resealed pins |
| `application_composition_contract` (builds the tests-disabled graph) | Passed, 447 s |
| Integration pass, serial: 25 startup-path rows (profile, public path, device binding, large review, logging, capabilities, epoch fixture), the new row and 8 composition contracts | 34/34 passed |
| `test_network_isolation_manifest`, `test_network_isolation_structure_contract` | Passed, `configured=419:registrations=392` |
| `clang-format` 22 on the seven C++ files; `git diff --check` | Clean |
| `clang-tidy` 21 on the six touched translation units | No finding on a changed line. The findings left are older than this card (`pragma once` in `dif_runtime.hpp`, benchmark member names, `std::endl` in the seed files' existing lines) |
| `scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

`application_logging` is in the integration pass. It is the redaction course
[[BPR-0891]] covers. The typed diagnostics log is unchanged.

## What macOS and Windows owe

The acceptance does not depend on either platform. The change is portable
C++ apart from the four macOS update-lifetime arms. The next build on each
platform covers the rest:

- **macOS.** Build `app_better_pull_review` in the `dev` preset. It compiles
  the four `update-*` arms, which were type-checked on Linux only. Then run
  `ctest --preset dev -R '^application_startup_failure_source$'`. Expected:
  `STARTUP_FAILURE_SOURCE_OK:profile-runtime-construction`.
- **Windows.** The new row is not registered (see above). The next
  `windows-dev` build compiles the recorder and the arms. No Windows row fails
  a startup on purpose, so the first Windows reading will be the next real
  refusal in a sweep. That refusal will now name its arm without a temporary
  print, which is what [[BPR-0874]] needed.

## Post-close: the macOS half (bpr-main-mac, Mac batch 5, 2026-09-23)

Run on `4a1f222d`, which contains `8e080bbc` and its finish `2b810d78`.
Apple Silicon, Qt 6.11.2 Debug. `cmake --build --preset dev` rebuilt 69
steps, among them `profile_desktop.cpp` and `app_better_pull_review`, with
exit 0 and no `warning:` line. The macOS-only `__APPLE__` blocks therefore
compiled here for the first time. The object file carries all four arm
names: `update-transaction-argument`, `update-lifetime-join`,
`update-healthy-start-timeout` and `update-healthy-start-receipt`.

`ctest --preset dev -V -R '^application_startup_failure_source$'` passed in
0.95 s, 1/1, exit 0, and printed
`STARTUP_FAILURE_SOURCE_OK:profile-runtime-construction`. The one-minute load
was 35.55 at the start and at the end. Nothing in the acceptance changes. The
Windows note above still stands.
