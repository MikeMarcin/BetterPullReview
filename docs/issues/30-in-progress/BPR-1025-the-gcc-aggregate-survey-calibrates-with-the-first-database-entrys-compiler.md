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
