---
schema: "implement-rfc.card.v1"
id: "BPR-1025"
title: "survey_gcc_aggregate_cleanups.py calibrates with the first database entry's compiler, so an app-release database alone fails"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, aggregate survey of the tests-disabled app-release database under BPR-0975","found_at":"b651554f","reproduced_at":["a8e9b714"],"related_cards":["BPR-0822","BPR-0975"],"reservation":"docs/issues/_reservations/BPR-1024--BPR-1026--f65f9f9f0854.json"}
---

# BPR-1025: survey_gcc_aggregate_cleanups.py calibrates with the first database entry's compiler, so an app-release database alone fails

## Defect

`scripts/survey_gcc_aggregate_cleanups.py` ([[BPR-0822]]) calibrates the
compiler of the first entry in the first database it is given (`main`,
lines 447–448). In the tests-disabled `app-release` database, that first entry
is the vendored SQLite amalgamation, `_bpr/sqlite/3.53.4/sqlite3.c`, compiled
by `/usr/bin/cc`. `compiler_family` asks with `-x c++`, so it identifies `cc`
as GCC and the survey goes on. `calibrate` then links the C++ reproducer with
the C driver, which does not link libstdc++. Calibration fails, and the
survey exits 2 before it compiles a single unit.

The `dev` and `core-only` databases begin with
`tests/network_isolation/linux_network_sandbox.cpp`, compiled by
`/usr/bin/c++`, so they calibrate. The documented command,
`--build-dir build/dev --build-dir build/app-release`
(`docs/development/build-and-test.md`, and the [[BPR-0822]] evidence), works
only because `dev` is named first. The `app-release` database alone, or named
first, fails.

## Reproduction, bpr-main-linux, 2026-09-23

GCC `15.2.0-16ubuntu1`: `cc` is `x86_64-linux-gnu-gcc-15` and `c++` is
`x86_64-linux-gnu-g++-15`. The script is at `a8e9b714`, unchanged since
`de528ce3`. The database is [[BPR-0975]]'s clean `app-release` build of
`b651554f`, with 227 entries: 226 use `/usr/bin/c++`, and the first uses
`/usr/bin/cc`.

```sh
python3 scripts/survey_gcc_aggregate_cleanups.py \
  --root /home/mmarcin/bpr0975-native/source \
  --build-dir /home/mmarcin/bpr0975-native/source/build/app-release
```

It exits 2 and prints `calibration build failed:`, then linker errors for
`operator new(unsigned long)`, `typeinfo for std::runtime_error`,
`__gxx_personality_v0` and others, and ends with
`GCC_AGGREGATE_SURVEY_FAILED:calibration`.

To bound it, two two-entry databases were made from the same one. Each held
the `sqlite3.c` entry and the `src/core/src/anonymous_read_policy.cpp` entry,
in opposite orders:

| First entry | Result |
|---|---|
| `sqlite3.c` (`cc`) | Exit 2, `calibration build failed` |
| `anonymous_read_policy.cpp` (`c++`) | Exit 0: `calibration: 25 shapes; 16 double-destroy at run time (documented GCC 15.2 behaviour: 16); detector flags 16`, `units=2 failures=0 sites=0 gated=0` |

The order alone decides the result, and the C unit itself surveys cleanly.
The temporary databases were removed.

[[BPR-0975]] records an aggregate survey over "`build/dev` and the
`app-release` database" with no gated `src/` site. Its ignored
`build/bpr0975/survey-release-after.log` shows a calibrated 227-unit pass, so
that run did not calibrate with `cc`. How it was invoked is not recorded.

A related limit, read from the code: one compiler is calibrated, while each
unit is recompiled with its own recorded command (`survey_unit`). A database
that records a second C++ compiler would have its units surveyed by a
detector nobody checked against that compiler.

## Scope

- Choose the compiler to calibrate from C++ units only, by each unit's
  language (its suffix or `-x`), not from the first entry.
- Calibrate every distinct C++ compiler the databases record, or refuse with
  status 2 and name them. Never survey a unit whose compiler was not
  calibrated.
- Handle C units deliberately: survey them or skip them, and report which.
- Keep the exit statuses and the documented command working. Update
  `docs/development/build-and-test.md` if the invocation or the statuses
  change.

## Acceptance

- AC1 The `app-release` database alone, and named before `build/dev`,
  calibrates with the C++ compiler and prints the same calibration line as
  the `dev` survey. On a tree with no gated site, it exits 0.
- AC2 A script test that needs no compiler pins the selection: a database
  whose first entry is C selects the C++ compiler, and a database that
  records two different C++ compilers is refused or calibrates both. The
  test fails against the current selection.
- AC3 At one revision, the documented two-preset command reports the same
  calibration line, unit count, sites and exit status before and after the
  change.

## Evidence

- `scripts/survey_gcc_aggregate_cleanups.py`, lines 319–327
  (`compiler_family`), 330–375 (`calibrate`) and 447–459 (the selection and
  the calibration call).
- [BPR-0822 survey](../../verification/BPR-0822-linux-gcc-aggregate-cleanup-survey.md)
  and `docs/development/build-and-test.md` for the documented command.

## Fixed: the survey chooses its compilers by each unit's language (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux` (JaegAW: WSL2 Ubuntu 26.04, GCC `15.2.0-16ubuntu1`,
Qt 6.11.2), claimed at `010d6da9`. The logs are station records under
`build/logs/bpr-main-linux-20260924/impl/` (`bpr-1025/*.log`,
`bpr-1025-*.log`) and are not committed.

### The change

`scripts/survey_gcc_aggregate_cleanups.py` no longer looks at the first
entry.

- **Language first.** `unit_language` reads a unit's last `-x` language, or
  else its suffix, by GCC's own rules: `.C` is C++ and `.c` is C.
  `survey_plan` groups the C++ units by their recorded compiler, in
  first-use order, and sets the C units and any other units aside. The order
  of `--build-dir` and of the entries no longer matters.
- **Every C++ compiler is calibrated.** Each GCC compiler the databases
  record is calibrated in turn, in its own work directory, and prints
  `compiler <path>, C++ units: <n>` before its calibration line. If any
  calibration fails, the survey stops with status 2 before a single unit is
  compiled. A unit is surveyed only when its compiler calibrated and is
  affected. If no compiler is affected, it stops with status 0.
- **C units are surveyed, deliberately.** They are recompiled with their
  own recorded compiler, as before, and counted in `units=`. They are not
  calibrated, because C has no destructors, so a C unit can fail to compile
  but cannot yield a finding. The survey prints `C units: <n>, compiled with
  their recorded compiler; C has no destructors to report`. This keeps the
  documented command's unit count.
- **Anything else is named.** Units in another language, and the C++ units
  of a Clang compiler in a database that also records GCC, are skipped and
  counted. A database with no C++ unit is refused with status 2. A compiler
  that is neither GCC nor Clang, such as a launcher, is still refused with
  status 2, and the survey names it.
- `--json` also records each compiler's calibration, `c_units` and
  `skipped_units`. With one compiler, its `calibration` field is the same
  line as before.
- `docs/development/build-and-test.md` says which compiler is calibrated,
  and lists the new status-2 case.

### AC2: the selection test, with no compiler

`tests/scripts/survey_gcc_aggregate_cleanups_tests.py`, registered as
`gcc_aggregate_survey_selection_contract`, drives the survey's `main` with
`compiler_family`, `calibrate` and `survey_unit` replaced by recorders. Its
databases are synthetic, and its temporary directories are contained. It
has 8 tests, all passing:

- a database whose first entry is `cc` compiling `sqlite3.c` calibrates only
  `/usr/bin/c++`, and surveys both units;
- either order of `app-release` and `dev` calibrates `/usr/bin/c++`;
- two C++ compilers are each calibrated, in first-use order;
- no unit is surveyed when one of them fails calibration;
- the units of an unaffected compiler are not surveyed;
- Clang units and assembler units are skipped and named, and a database of
  only Clang is skipped with status 0;
- a C-only database is refused;
- the language is read from `-x` before the suffix.

**Against the selection at `010d6da9`** (the test run on a copy of the old
script, `bpr-1025-negctl-old-selection.log`), 7 tests fail and 8 subtests
error:

- `Lists differ: ['/usr/bin/cc'] != ['/usr/bin/c++']`, for the first entry
  and for `app-release` named first.
- `['/usr/bin/g++-14'] != ['/usr/bin/g++-14', '/usr/bin/c++']`, for the two
  compilers.
- The language test errors (7 subtests and its last assertion), because
  `unit_language` does not exist there.
- `dev` named first passes, as it always did.

### AC1 and AC3: the real databases at one revision

Both trees were built at `010d6da9` in this checkout:

- `build/dev`: 460 entries, 459 by `/usr/bin/c++` and one C unit.
- `build/app-release`: a fresh Release build, 487 steps, whose only warning
  is the SQLite amalgamation's known `-Wstringop-overread`. It has 228
  entries, and the first is `_bpr/sqlite/3.53.4/sqlite3.c` by `/usr/bin/cc`.

Each run below was serial, with its own `TMPDIR` in an owned directory. None
left anything there. "Before" is the script at `010d6da9`. "After" is this
change (script SHA-256 `611b5d0828d82fb7…`).

| Run | Result |
| --- | --- |
| Before, `--build-dir build/dev --build-dir build/app-release --jobs 10` (the documented command) | Calibration line below; `units=688 failures=0 sites=39 gated=0`; `GCC_AGGREGATE_SURVEY_OK`; exit 0; 315 s |
| After, the same command | `compiler /usr/bin/c++, C++ units: 686`; **the same calibration line**; `C units: 2, …`; `units=688 failures=0 sites=39 gated=0`; exit 0; 315 s. **The 39 noted sites are line for line the same**, all under `tests/` |
| After, `--build-dir build/app-release` alone | `compiler /usr/bin/c++, C++ units: 227`; the same calibration line; `C units: 1, …`; `units=228 failures=0 sites=0 gated=0`; `GCC_AGGREGATE_SURVEY_OK`; exit 0; 121 s |
| After, `--build-dir build/app-release --build-dir build/dev` | The same as the documented order: 686 C++ units, the same line, `units=688 … sites=39 gated=0`, exit 0; 299 s |
| Negative control: before, `app-release` alone | `calibration build failed:` with 7 `undefined reference` lines (`operator new(unsigned long)`, `typeinfo for std::runtime_error`, `__gxx_personality_v0`, …), then `GCC_AGGREGATE_SURVEY_FAILED:calibration`; exit 2 |
| Negative control: before, `app-release` then `dev` | The same failure; exit 2 |

The calibration line in every passing run is
`calibration: 25 shapes; 16 double-destroy at run time (documented GCC 15.2 behaviour: 16); detector flags 16`.

### Acceptance

- **AC1: pass.** The `app-release` database alone, and named before
  `build/dev`, calibrates `/usr/bin/c++` and prints the same calibration line
  as the `dev` survey. With no gated site in the tree, it exits 0. The
  script before the change fails both runs.
- **AC2: pass.** The selection test needs no compiler. It pins the C-first
  case and calibrates both of two C++ compilers, and it fails against the
  selection at `010d6da9`.
- **AC3: pass.** At `010d6da9`'s trees, the documented two-preset command
  reports the same calibration line, `units=688`, the same 39 sites and exit
  0, before and after.

Also run, all passing (`bpr-1025-rows-1.log`, `-rows-2.log`):
`gcc_aggregate_survey_selection_contract`,
`contained_temporary_usage_contract`, `skip_report_contract`,
`test_network_isolation_manifest` and `documentation_structure`.
