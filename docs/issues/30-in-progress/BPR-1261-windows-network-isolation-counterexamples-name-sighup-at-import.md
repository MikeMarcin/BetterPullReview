---
schema: "implement-rfc.card.v1"
id: "BPR-1261"
title: "Windows: the network isolation counterexamples name SIGHUP at import, and four rows fail before they start"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "linux"]
metadata: {"found_by":"bpr-main-win BPR-0905 Windows full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1261--BPR-1261--9d2038aba170.json","related_cards":["BPR-0905","BPR-1113","BPR-0903","BPR-1237"]}
---

# BPR-1261: Windows: the network isolation counterexamples name SIGHUP at import, and four rows fail before they start

Found in the [BPR-0905 Windows full suite](../../verification/BPR-0905-full-suite-2026-10-01.md)
at `db021262a`. Four rows stopped at `AttributeError: module 'signal' has no
attribute 'SIGHUP'`:

- `test_network_isolation_external`;
- `test_network_isolation_child`;
- `test_network_isolation_loopback`;
- `test_network_isolation_concurrent_launch`.

Each loads `tests/scripts/network_isolation_counterexamples.py`.
[[BPR-1113]] (`d03e2d618`) added `LEAK_WINDOWS` there, a module-level tuple
whose third entry reads `signal.SIGHUP.value`. Windows' `signal` module has no
`SIGHUP`, so the import failed. The windows are tried only by the Linux runtime
course, which is the only code that reads them.

This is separate from [[BPR-0903]], which owns the child probe's Windows
denial expectation. These rows failed before they reached it.

## Fix

The `SIGHUP` entry is spliced in only when `hasattr(signal, "SIGHUP")`. On
Linux the tuple is what it was: SIGINT, SIGTERM, SIGHUP, `handlers`, `wait`.
On Windows it is the same without SIGHUP, and nothing reads it there. The
other `signal` names in the file are inside functions that only Linux calls.

## Acceptance

- The four rows get past import on Windows.
- Linux is unchanged: the `test_network_isolation_*` rows give the results
  they gave before.

## Windows evidence (bpr-main-win, 2026-10-01)

Windows 11 Home 10.0.26200, Python 3.14, `build/windows-dev` Debug, at
`e96b1e440` plus this change. Importing the module prints `LEAK_WINDOWS` as
`signal:2`, `signal:15`, `handlers`, `wait`.

```powershell
ctest --preset windows-dev -j1 --output-on-failure -R "^test_network_isolation_"
```

| Row | Before | After |
|---|---|---|
| `test_network_isolation_external` | `AttributeError` (SIGHUP) | Pass, 0.10 s |
| `test_network_isolation_child` | `AttributeError` (SIGHUP) | Pass, 0.17 s |
| `test_network_isolation_loopback` | `AttributeError` (SIGHUP) | Pass, 0.13 s |
| `test_network_isolation_concurrent_launch` | `AttributeError` (SIGHUP) | Pass, 0.94 s |
| `test_network_isolation_structure_contract` | Pass | Pass, 3.69 s |
| `test_network_isolation_application` | Pass | Pass, 0.17 s |
| `test_network_isolation_manifest` | Fail | Fail, 6.96 s: unpinned `bpr_windows_uncontained` rows ([[BPR-1237]]) |

`_child` and `_external` pass on Windows, which [[BPR-0903]] did not expect.
That card should check whether its probe still proves the denial before it
treats them as fixed.

The Linux run is bpr-main-linux's.
