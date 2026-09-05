---
schema: "implement-rfc.card.v1"
id: "BPR-0431"
title: "The live-mutation guard reads dumpbin's file header as a symbol"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The live-mutation guard reads dumpbin's file header as a symbol

In the first Windows Qt-graph run ([[BPR-0412]]) `live_mutation_isolation`
failed on a binary that only exists in that graph:

```
LIVE_MUTATION_ISOLATION_FAILED: better_pull_review_live_review_mutation_provider_tests.exe
  contains a live_review_mutation_provider symbol: Dump of file
  C:\...\test_better_pull_review_live_review_mutation_provider_tests.dir\Debug\live_review_mutation_provider_tests.obj.
```

The "symbol" is `dumpbin`'s own header line. [[BPR-0361]] made the Windows
half of the guard read each binary's objects with `dumpbin /symbols` and scan
the raw output for the forbidden identifier as a substring — a sound choice,
since no demangler ships with Visual Studio — but `dumpbin` prefaces every
object's table with `Dump of file <path>`, and the provider's own test object
is named for the provider it tests. The path contains the identifier; the
object defines no such symbol. `nm` on macOS and Linux prints no header, so
the POSIX half never sees the line, and `core-only` never builds that test,
so the Windows half had never met it until the Qt graph ran.

## Required final state

- `forbidden_symbols_in` ignores `dumpbin`'s `Dump of file` header and scans
  symbol records only; the scan is otherwise unchanged, and a decorated name
  that carries the identifier is still caught.
- A unit case presents an object table whose header names the provider and
  whose records do not, and requires no failure.
- `live_mutation_isolation` passes in the Qt graph on Windows, which
  [[BPR-0412]] records, and `live_mutation_isolation_contract` still passes
  in `core-only`.

## Verification

```sh
py -3 tests/scripts/verify_live_mutation_isolation_tests.py
ctest --preset core-only --output-on-failure -R '^live_mutation_isolation(_contract)?$'
ctest --preset windows-dev --output-on-failure -R '^live_mutation_isolation$'
```

## Result

`forbidden_symbols_in` skips `Dump of file` lines; everything else is as it
was. `test_dumpbin_header_naming_the_provider_is_not_a_symbol` presents a
table whose header names the provider and whose one record is the positive
control, and requires no failure; the existing decorated-name case still
catches a real symbol. In the Qt graph under the boundary:

```
live_mutation_isolation ..............   Passed    7.62 sec
```

with the provider's test object among the objects read.
