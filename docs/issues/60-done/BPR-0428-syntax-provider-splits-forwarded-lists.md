---
id: BPR-0428
title: The syntax provider splits a forwarded list on the round after it
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The syntax provider splits a forwarded list on the round after it

The first `windows-dev` configure in the `windows-lane` worktree — the Qt
graph [[BPR-0412]] exists to run — stopped inside the KSyntaxHighlighting
provider:

```
CMake Error at cmake/BprSyntaxHighlightingProvider.cmake:152 (message):
  Verified KSyntaxHighlighting build-Release failed (1).
  error MSB8013: This project doesn't contain the Configuration and Platform
  combination of Release|x64.
```

The nested configure had been given `CMAKE_CONFIGURATION_TYPES=Debug` — its
cache says so — while the outer configure's value is the Visual Studio
default of four, and the provider's build loop iterates the outer list. So
it built Debug, asked for Release, and MSBuild had never generated one.

`bpr_syntax_forward` escapes a value's `;` as `\;` and appends
`-D<name>=<value>` to an accumulator. [[BPR-0363]] fixed the temporary that
shadowed that accumulator on 2026-08-18, and that fix — made on macOS under
Ninja — exposed this one: the append expanded the accumulator *unquoted*, and
an unquoted expansion turns every `\;` a previous round escaped back into a
bare `;`. A list value survived its own round and was split on the next. The
nested configure received `-DCMAKE_CONFIGURATION_TYPES=Debug` followed by
`Release`, `MinSizeRel`, and `RelWithDebInfo` as stray arguments, and the
provider's own signature recorded the same broken shape.

Measured in script mode with both shapes side by side, before the edit:

```
forward_current: 6 elements   [-DB=Debug] [Release] [MinSizeRel] [RelWithDebInfo] ...
forward_fixed:   3 elements   [-DB=Debug;Release;MinSizeRel;RelWithDebInfo]
```

It never surfaced on macOS or Linux because the only list-valued name those
hosts forward is `CMAKE_PREFIX_PATH`, and it has had one element. The main
checkout's 2026-08-17 `windows-dev` configure built all four configurations
for the opposite reason: [[BPR-0363]]'s shadowing bug was still dropping
every forwarded name but the last, so the nested configure never saw
`CMAKE_CONFIGURATION_TYPES` at all and defaulted to the full set.

## Required final state

- `bpr_syntax_forward` appends with `list(APPEND)` and copies the accumulator
  to the parent scope quoted, so every escaped element survives every round
  and the nested configure receives each forwarded value whole.
- A test fails without the change: it includes the provider in script mode,
  forwards a scalar, a four-element list, and a two-element list, and requires
  exactly three arguments with the lists intact.
- The `windows-dev` configure builds and installs KSyntaxHighlighting for all
  four configurations, which [[BPR-0412]] records.

## Verification

```sh
py -3 tests/scripts/dependency_modernization_tests.py <the registered arguments> -k forward
ctest --preset core-only --output-on-failure -R '^dependency_modernization_contract$'
cmake --preset windows-dev
```

## Result

`bpr_syntax_forward` now appends with `list(APPEND)` and copies the
accumulator quoted. The same script-mode probe, run against the previous
revision and against this one:

```
HEAD^  FORWARDED_COUNT=6   -DCMAKE_CONFIGURATION_TYPES=Debug | Release | MinSizeRel | RelWithDebInfo
HEAD   FORWARDED_COUNT=3   -DCMAKE_CONFIGURATION_TYPES=Debug;Release;MinSizeRel;RelWithDebInfo
```

`test_syntax_provider_forwards_list_values_whole` encodes exactly that and
passes; `dependency_modernization_contract` runs 23 cases on this host with
the same five [[BPR-0426]] failures as before and no new one. The
`windows-dev` configure's four-configuration KSyntaxHighlighting build is
recorded on [[BPR-0412]], whose run this defect interrupted.

## Necessary, not sufficient — the same list flattened once more

The rerun after the fix above failed identically, and the nested cache still
read `CMAKE_CONFIGURATION_TYPES=Debug`. The accumulator was whole; the
command line was not. `bpr_syntax_execute(label log)` receives the nested
command through `ARGN`, and CMake re-joins a function's arguments into
`ARGN` with plain `;`, so an argument that contained an escaped list arrived
as one argument and left as four. Measured in script mode with the same
wrapper shape:

```
via ARGN        : -DA=x -DB=Debug Release MinSizeRel
via PARSE_ARGV  : -DA=x -DB=Debug;Release;MinSizeRel
```

`cmake_parse_arguments(PARSE_ARGV …)` exists for exactly this: it hands back
arguments with their escapes intact. `bpr_syntax_execute` now takes its
command that way, and a second unit case drives the real helper with a
forwarded list through `cmake -E echo` and reads the log it writes. Two
rounds of the same escape were lost in two places; the probe for each is
recorded because the first looked complete and was not.
