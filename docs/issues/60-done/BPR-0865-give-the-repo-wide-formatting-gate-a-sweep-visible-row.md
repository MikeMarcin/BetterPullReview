---
schema: "implement-rfc.card.v1"
id: "BPR-0865"
title: "Give the repo-wide formatting gate a sweep-visible row, once every station is known to have clang-format 22"
status: "Done"
rfcs: []
depends_on: ["BPR-0862"]
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-005, deferred out of BPR-0862","found_at":"70a52630","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"A sweep row exists that goes red when a tracked file is unformatted and is skipped (not failed) where no supported clang-format exists, and it is shown to do both","outcome":"pass","revision":"b9e72812","evidence":{"kind":"command","detail":"coding_format_check (tests/CMakeLists.txt) runs _coding-standard/scripts/format_check_or_skip.py with the clang-format configure found through find_program VALIDATOR (CodingStandardClangFormat.cmake): /usr/bin/clang-format 22.1.8 on JaegAW, passing over ~/.local/bin 21.1.8 first on PATH. Green at b9e72812: ctest --preset dev 6/6 with the gate contract, isolation manifest and documentation rows; the row checks 791 files in 18.5 s. Red: with src/application/src/workbench_application_host.cpp restored to its 95343750 bytes the row is ***Failed naming that file, ctest exit 8. Skipped: core-only configured with -DBPR_CLANG_FORMAT_EXECUTABLE=~/.local/bin/clang-format gives ***Skipped 'reports version 21.1.8', ctest exit 0; configured with CMAKE_IGNORE_PATH over every clang-format directory gives NOTFOUND, a command with no --clang-format and ***Skipped 'configure found no clang-format 22 or newer', ctest exit 0. coding_standard_format_gate_contract pins the driver's pass/fail/skip cases and the discovery order; removing the VALIDATOR fails 2 of its 3 discovery cases. Linux only; Windows and macOS first-sweep confirmations are listed on the card.","path":"docs/issues/60-done/BPR-0865-give-the-repo-wide-formatting-gate-a-sweep-visible-row.md#the-row-added-bpr-main-linux-2026-09-23"}},{"criterion":"Both stations' clang-format versions are recorded here either way","outcome":"pass","revision":"b9e72812","evidence":{"kind":"observation","detail":"The station table records bpr-linux (JaegAW) 21.1.8 on PATH and 22.1.8 at /usr/bin/clang-format, re-observed on 2026-09-23, and bpr-win 22.1.8 at C:\\Program Files\\LLVM\\bin\\clang-format.exe, the only one on PATH, as bpr-win collected it on 2026-09-17; the macOS integrator's 23.1.0 and 22.1.8 are unchanged.","path":"docs/issues/60-done/BPR-0865-give-the-repo-wide-formatting-gate-a-sweep-visible-row.md#station-versions-collected-2026-09-17-dsp-005"}}]
---

# BPR-0865: Give the repo-wide formatting gate a sweep-visible row, once every station is known to have clang-format 22

## Why

[[BPR-0862]] found the repo-wide `coding-format-check` gate red on main for
seventeen files and, more importantly, found that **nothing in a full ctest
sweep would have noticed**. `coding-format-check` is a build target; the one
ctest row with a similar name, `coding_standard_format_gate_contract`, pins the
gate script's own behaviour rather than the project's formatting. A reader of a
green 381-row sweep can conclude the tree is formatted when it is not — and
that is how seventeen files drifted.

## Why BPR-0862 did not just add the row

The gate hard-fails without a supported tool. `_coding_standard.py`
(`MINIMUM_CLANG_FORMAT_VERSION = (22,)`) refuses when `clang-format` is absent,
unreadable, or older than 22, because releases before 22 render `{}` where this
tree holds `{ }` and would call hundreds of files dirty. Registering the
repo-wide check as a ctest row therefore turns **every station's sweep red for
a toolchain reason** on any machine whose `clang-format` is older or missing.

At the time BPR-0862 landed, only the macOS integrator's versions were known
(Homebrew 22.1.8 and 23.1.0, both of which report the tree clean). The Windows
and Linux stations' versions were not, and a sweep was about to be asked of
both. Making that gamble in the same pass would have cost a station round trip
to learn a version. So the row was deferred here, deliberately, rather than
left unrecorded.

## Scope

- Collect `clang-format --version` from `bpr-win` and `bpr-linux`. (The
  dsp-005 envelopes ask for it; the answers may already be on the board or in
  the station replies by the time this card is picked up.)
- If all three stations satisfy the minimum, register the repo-wide check as a
  ctest row next to `coding_standard_format_gate_contract` in
  `tests/CMakeLists.txt`, guarded so it is registered only when configure found
  a supported `clang-format` — the tree already conditions the coding targets
  on `BPR_ENABLE_CODING_TARGETS` and a centrally discovered
  `Python3::Interpreter`, so follow that pattern rather than inventing a new
  one. A station without the tool must **skip**, not fail: a missing tool is
  not formatting rot, and a row that conflates them is worse than no row.
- If some station cannot have clang-format 22, do not add the row. Instead make
  the gate part of the integrator's own pre-merge pass and say so where a
  sweep's greenness is interpreted, so nobody reads a green sweep as a
  formatted tree.

## Acceptance

- Either a sweep row exists that goes red when a tracked file is unformatted
  and is skipped (not failed) where no supported `clang-format` exists, and it
  is shown to do both; or the card records, with the actual station versions,
  why the row cannot exist and where the gate runs instead.
- Both stations' `clang-format` versions are recorded here either way.

## Station versions, collected 2026-09-17 (dsp-005)

Two of the three are known, and they settle the question against a naive row.
`bpr-linux`'s numbers were collected by that station (`bpr-linux/int-12`) and
re-verified there; macOS is the integrator's own.

| Station | `clang-format` | Python |
|---|---|---|
| `bpr-main-mac` (integrator, macOS) | 23.1.0, and 22.1.8 also installed; both report the tree clean at `754d0538` | 3.14 |
| `bpr-linux` (JaegAW, WSL2, Ubuntu 26.04) | **21.1.8 on the default `PATH`**; 22.1.8 only at `/usr/bin/clang-format` | 3.13.15 on `PATH`, 3.14.4 at `/usr/bin/python3` |
| `bpr-win` | 22.1.8 at `C:\Program Files\LLVM\bin\clang-format.exe`, the only one on `PATH` (collected by `bpr-win`, see "Windows run at the merged head" below) | 3.14.7 on `PATH`; CMake's `Python3::Interpreter` is 3.14.6 |

The Linux station's exact strings, because the shadowing is the whole point:

| Resolution | Version string | Meets the `(22,)` minimum |
|---|---|---|
| `clang-format` (PATH) → `/home/mmarcin/.local/bin/clang-format` | `Ubuntu clang-format version 21.1.8 (6ubuntu1)` | **No** |
| `/usr/bin/clang-format` | `Ubuntu clang-format version 22.1.8 (++20260714015917+ca7933e47d3a-1~exp1~20260714135927.17)` | Yes |

**This is the answer the deferral was waiting for, and it is concrete rather
than hypothetical.** A ctest row that resolved `clang-format` from `PATH` and
gated on version 22 would have gone red on `bpr-linux` **today** — for a
shadowed toolchain, not for formatting rot, which is exactly the outcome this
card was opened to avoid. That station's own gates have had to say
`/usr/bin/clang-format` explicitly since the shadowing was found, including
[[BPR-0855]]'s and [[BPR-0856]]'s runs, which is why its formatting verdicts
have been trustworthy while a naive row would not have been.

**Integrator's decision on these numbers (dsp-005).** They settle this card
against adding the row as first imagined. If the row is ever added it has two
hard requirements rather than one:

- **Resolve the tool explicitly**, the way that station's gates do and the way
  CMake's own discovery would, rather than trusting `PATH`. A row that picks
  up whichever `clang-format` is first on `PATH` is not reproducible across
  these three machines.
- **Skip, not fail**, when no supported tool is found, so a missing or old
  tool never reads as unformatted source.

Windows's version is the last input. It is asked for in the next `bpr-win`
envelope.

**Not run on the Linux station.** That task was runs, not fixes: the repo-wide
`coding-format-check` gate was not executed there and no registration was
added or changed. Only the versions above were collected.

## Windows run at the merged head, 2026-09-17 (bpr-win, task-20260917-win-merged-head-runs)

The head is `2b2e5549d81c4f9ba92cd93d542b5e6064b68c2e`. The full row-by-row sweep
(362 rows, 309 passed, 53 failed) is `docs/verification/dsp4-windows-merged-head-sweep.md`.

**The Windows input, which was the last unknown.**

- `clang-format` **22.1.8**, resolved from `PATH` at
  `C:\Program Files\LLVM\bin\clang-format.exe`. It is the only `clang-format`
  on `PATH`; no older copy shadows it, so an absolute path is not needed here as
  it is on `bpr-linux`.
- `python3` **3.14.7**, resolving to
  `C:\Users\mikem\AppData\Local\Microsoft\WindowsApps\python3.exe`.
- A registered row would not run either of those: CMake's `Python3::Interpreter`
  on this station is `C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64/python.exe`,
  reporting **3.14.6**.

So all three stations can satisfy a 22-or-later gate, but only if the row takes
the tool from `PATH` on Windows and from an absolute path on Linux.

## The row, added (bpr-main-linux, 2026-09-23)

**Decision.** All three stations can run clang-format 22 or newer, as long as
the tool is chosen explicitly: macOS 23.1.0 and 22.1.8, Windows 22.1.8 on
`PATH`, and Linux 22.1.8 at `/usr/bin/clang-format` behind a 21.1.8 earlier
on `PATH`. So the row is added. It meets both requirements the dsp-005
integrator set above.

- **Resolve the tool explicitly.**
  `_coding-standard/cmake/CodingStandardClangFormat.cmake` runs `find_program`
  with a `VALIDATOR`. The validator asks each candidate
  `_coding-standard/scripts/supported_clang_format.py`, which uses the gate's
  own `MINIMUM_CLANG_FORMAT_VERSION` and banner parser. It passes over an older
  tool and keeps searching. The result is cached as
  `BPR_CLANG_FORMAT_EXECUTABLE`. Configure on this station prints
  `-- Coding standard clang-format: /usr/bin/clang-format`, skipping the 21.1.8
  in `~/.local/bin`. The `coding-format` and `coding-format-check` targets now
  run the same tool.
- **Skip, not fail.** `coding_format_check` runs
  `_coding-standard/scripts/format_check_or_skip.py` with that tool. It fails
  when a file is unformatted. It exits 77, the row's `SKIP_RETURN_CODE`, in
  three cases: configure found no supported tool, the tool it found is gone, or
  that tool is unreadable or older than 22. CTest then reports **Skipped**. A
  gate result is never turned into 77.

**One deliberate difference from the Scope wording.** Scope says to register
the row only when configure found a supported tool. Instead, the row is
registered whenever the coding targets are enabled (`BPR_ENABLE_CODING_TARGETS`
with the central `Python3::Interpreter`). Where no tool was found, the row
exists and reports Skipped. An unregistered row would leave nothing in the
sweep. A skipped row appears in CTest's "did not run" list and the JUnit
`skipped` count, so no one can read that sweep as proof the tree is formatted.
`docs/development/build-and-test.md` ("The sweep checks formatting") and
`_coding-standard/README.md` explain how to read each outcome.

**The skip reaches CTest as an exit code on purpose.** On Linux and macOS the
isolation launcher prints `TEST_RUNNER_CHILD_EXIT_FAILED:77`, which matches
the inherited error expression. CTest applies `SKIP_RETURN_CODE` first:
measured on CMake 4.4.2 in a scratch project and on the real row below. If a
later CTest reversed that order, the row would fail visibly. An output
expression with exit 0 would pass silently if its text ever stopped matching.

**Windows** registers the row `WINDOWS_UNCONTAINED`, because the gate starts
`clang-format.exe` from outside the checkout. It is pinned in
`scripts/verify_test_network_isolation.py` as `child-process-denied`, by
analogy with the BPR-0872 rows; it has not been measured contained. The
macOS sandbox profile denies only network, so the row runs sandboxed there.

**The tree was red again at `95343750`.** The gate, run with 22.1.8 across 791
files, named four:
`src/app/src/github_public_path_fixture.hpp`,
`src/application/include/bpr/application/workbench_runtime_ports.hpp`,
`src/application/src/workbench_application_host.cpp` and
`tests/application/workbench_application_host_tests.cpp`. The misalignment
came from new members and assignments added in `8d072d14` (BPR-0976) and
`9210330a` (BPR-0977), five days after BPR-0862 left the tree clean. Every
sweep in between was green. `5795dfa3` fixes them. It changes whitespace only:
`git diff --ignore-all-space` is empty. None of the four files is in a
composition digest manifest, so nothing was resealed.

### Verification (Linux, JaegAW, Ubuntu 26.04, GCC 15.2, CMake 4.4.2)

| Check | Candidate | Result |
|---|---|---|
| `cmake --build --preset dev` after the reformat, then `ctest -R` over `application_workbench_host`, `application_workbench_composition_contract`, `application_review_round_trip_composition_contract`, `architecture_boundaries`, `application_public_path`, `application_github_public_path_github_com`, `coding_standard_format_gate_contract` | `5795dfa3` | build exit 0; 7/7 passed |
| **Green:** `ctest --preset dev -R '^coding_format_check$'` (with the gate contract, `test_network_isolation_manifest`, `test_network_isolation_structure_contract`, `documentation_structure`, `documentation_issue_graph_contract`) | `b9e72812`, the pushed head | 6/6 passed; the row checks 791 files with `/usr/bin/clang-format (22.1.8)` in 18.5 s |
| **Red (negative control):** the same row, with `src/application/src/workbench_application_host.cpp` put back to its `95343750` bytes | `b9e72812` plus that one file | `***Failed`, "Files needing formatting: src/application/src/workbench_application_host.cpp", `ctest` exit 8; the file was restored afterwards |
| **Skipped, old tool:** `cmake --preset core-only -DBPR_CLANG_FORMAT_EXECUTABLE=/home/mmarcin/.local/bin/clang-format`, then the row | `b9e72812` | `***Skipped`, "SKIPPED: /home/mmarcin/.local/bin/clang-format reports version 21.1.8; this style file needs 22 or newer.", `ctest` exit 0 |
| **Skipped, none found:** `cmake --preset core-only -UBPR_CLANG_FORMAT_EXECUTABLE "-DCMAKE_IGNORE_PATH=/usr/bin;/bin;/home/mmarcin/.local/bin"`, then the row | `b9e72812` | configure: "no supported version found; coding_format_check will be skipped". The cache holds `BPR_CLANG_FORMAT_EXECUTABLE-NOTFOUND` and the command carries no `--clang-format`. Result `***Skipped`, "SKIPPED: configure found no clang-format 22 or newer; install one and reconfigure.", `ctest` exit 0. Restored with `-U` on both variables; the row passes in core-only |
| `cmake --build --preset dev --target coding-format-check` | `b9e72812` | exit 0, "Checking 791 file(s) with /usr/bin/clang-format (22.1.8) … Formatting check passed." The previous command, `format_check.py` taking `clang-format` from `PATH`, exits 2: "unsupported clang-format: /home/mmarcin/.local/bin/clang-format reports version 21.1.8" |
| `coding_standard_format_gate_contract` | `fe6aa5d7` | 20 cases, 1 skipped (the existing PATH host probe, which sees 21.1.8 here). The new cases: the driver passes formatted source, fails unformatted source (never a skip), and skips for no tool, a vanished tool, an unreadable banner and an old tool, even over unformatted source, without asking it to format. Discovery run in script mode against stubs takes the second of `[21, 22]`, the first of `[22, 23]`, and finds nothing in `[21]` |
| Negative control for the discovery cases: the `VALIDATOR` line removed | working tree | 2 of 3 fail (`0 != 1`; `0 is not None`); restored |
| `test_network_isolation_manifest`, `test_network_isolation_structure_contract` in dev and core-only; `rfc0016_test_inventory`, `rfc0017_test_inventory`, `platform_configuration_contract`, `contained_temporary_support_contract` in dev | `b9e72812` | all passed |
| `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py`; `git diff --check` | `b9e72812` | `DOCUMENTATION_OK`; 64 OK; clean |

The red, skipped, target and inventory runs were made on the working tree
before the commit was rebased onto `66f969ed`, and are named here by the
rebased `b9e72812`. The rebase changed nothing in this card's paths except
three [[BPR-0874]] pins in `scripts/verify_test_network_isolation.py`. The
green row and the manifest gate were rerun at `b9e72812` itself.

The logs were transient (`/tmp/bpr0865/`) and are not checked in. The table
above records the observations.

### Not yet observed on Windows or macOS

No acceptance line depends on these runs. The Linux station was the one with
the problem that needed solving: a supported tool shadowed on `PATH`. These are
the first-sweep confirmations to ask of the other two stations:

- **bpr-main-win**. Reconfigure `windows-dev` and expect
  `-- Coding standard clang-format: C:/Program Files/LLVM/bin/clang-format.exe`.
  Then run
  `ctest --preset windows-dev --output-on-failure -R "^(coding_format_check|coding_standard_format_gate_contract|test_network_isolation_manifest)$"`
  and expect all three to pass. The gate contract's new dirty batch stub runs
  there for the first time. The three discovery cases skip on Windows by
  design. Measure the pin: register the row contained in a trial you revert,
  and record the first contained failure. That confirms or corrects
  `child-process-denied`.
- **Mac integrator**. Reconfigure `dev` and expect the status line to name
  Homebrew's clang-format. Then run
  `ctest --preset dev -R "^(coding_format_check|coding_standard_format_gate_contract)$"`
  and expect both to pass under the sandbox runner. The discovery cases run
  there for the first time.

## Post-close macOS run (bpr-main-mac, 2026-09-23)

At `e648db24`, `ctest --preset dev -R "^(coding_format_check|coding_standard_format_gate_contract)$"`
passed 2/2 (`build/bpr0865-mac-rows.log`).
`coding_standard_format_gate_contract` ran 20 tests OK in 4.57 s.
`coding_format_check` checked 791 files with Homebrew `clang-format` 23.1.1,
reported `Formatting check passed.` and took 16.87 s. This note adds evidence
only and changes no acceptance.

## Windows: the row runs contained; the pin is removed (bpr-main-win, 2026-09-23)

`coding_format_check` was registered `WINDOWS_UNCONTAINED` and pinned
`child-process-denied` by analogy with [[BPR-0872]]'s rows, without a
contained measurement. It has now been measured on JaegAW (Windows 11
10.0.26200), at `81356f46` plus this change, in both Windows trees. The logs
are station records under `build/logs/bpr-0865-main-win/` and are not
committed.

**The presets.** `windows-dev` is the documented Windows developer preset:
Visual Studio 18 2026, multi-config, `build/windows-dev`, with the sandbox
launcher under `Debug/`. `dev` is Ninja, single-config Debug, `build/dev`.
Both resolve the same `C:/Program Files/LLVM/bin/clang-format.exe` (22.1.8),
and the `windows-dev` configure status line names it. They resolve different
Pythons: `windows-dev` has `C:/Program Files/Python314`, and `dev` has the
per-user `pythoncore-3.14-64`.

**Measured.**

| Run | Result |
| --- | --- |
| `windows-dev`, as registered (uncontained) | Pass, 19.86 s |
| `windows-dev`, the row's own command run through `Debug/better_pull_review_network_sandbox.exe` | Exit 0: `Checking 791 file(s) with C:\Program Files\LLVM\bin\clang-format.exe (22.1.8)`, `Formatting check passed.` |
| `dev`, contained as registered after the change, 3 runs (`--repeat until-fail:3`) | Pass 3 of 3, 22.8–23.4 s, per-user Python |
| `windows-dev`, contained as registered after the change | Pass, 23.45 s |

The container starts Python and `clang-format.exe` from their install
directories, so the pin was wrong. Its registration in `tests/CMakeLists.txt`
no longer says `WINDOWS_UNCONTAINED`, and its entry is gone from
`scripts/verify_test_network_isolation.py`. After the change,
`test_network_isolation_manifest` passes in both trees
(`TEST_NETWORK_ISOLATION_OK:configured=417:registrations=391:backend=windows-appcontainer`
in `windows-dev`), and so do `test_network_isolation_structure_contract` and
`coding_standard_format_gate_contract`. The contained Python prints `Failed to
find real location of …python.exe` on stderr at startup. The row does not
compare stderr, so it still passes.
