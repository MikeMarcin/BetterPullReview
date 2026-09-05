---
schema: "implement-rfc.card.v1"
id: "BPR-0423"
title: "The Windows format-gate stub truncates any banner containing brackets"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The Windows format-gate stub truncates any banner containing brackets

`coding_standard_format_gate_contract` fails on Windows, inside the
AppContainer and outside it alike:

```
FAIL: test_every_distribution_banner_parses_to_its_release
AssertionError: None != (17, 0, 2) : Android (10552028, +pgo, +bolt, +lto, +mlgo, based on r487747c) clang-format version 17.0.2
```

The test writes a stand-in `clang-format` that echoes a distribution banner,
and on Windows that stand-in is a `.bat` file whose `echo` sits inside a
parenthesised `if` block (`tests/scripts/coding_standard_format_gate_tests.py:56-72`).
`cmd` reads the first unescaped `)` in the banner as the end of that block, so
the Android banner — and the Debian one after it, `(tags/RELEASE)` — comes out
truncated and the parser correctly reports no version. The parser is fine; the
fixture lied to it. Measured by invoking the stub directly: the same banner
without brackets echoes cleanly, the bracketed one does not.

This assertion had never run on Windows before this week, because nothing on
Windows executed before [[BPR-0347]]. It is a test-fixture defect in the
Windows branch of the stub writer, not a product defect, and the POSIX stub is
untouched.

## Required final state

- The Windows stub escapes `(` and `)` as `^(` and `^)` before echoing, and
  every banner in the table round-trips to its release.
- The POSIX stub is unchanged.
- `coding_standard_format_gate_contract` passes uncontained on Windows. Whether
  it passes *contained* is a separate question about running `cmd.exe` inside
  an AppContainer and is answered on [[BPR-0362]], not here.

## Verification

```sh
py -3 tests/scripts/coding_standard_format_gate_tests.py
```

## Result

```
py -3 tests/scripts/coding_standard_format_gate_tests.py    9 tests, OK (skipped=1)
```

The skip is the pre-existing execute-permission case the suite already
documents as inexpressible on Windows.
