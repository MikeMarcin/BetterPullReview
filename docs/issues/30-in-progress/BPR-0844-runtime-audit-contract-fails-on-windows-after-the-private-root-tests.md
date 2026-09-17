---
schema: "implement-rfc.card.v1"
id: "BPR-0844"
title: "runtime_audit_contract fails on Windows after BPR-0820's private-root script tests"
status: "In Progress"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-private-roots step 1","found_at":"6c44b6eaedfc7a752f0ab78255c2403bc4f406e4","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0844: runtime_audit_contract fails on Windows after BPR-0820's private-root script tests

## Defect

[[BPR-0820]] added `--private-data-root` to `scripts/verify_runtime_surface.py`
and five cases to `tests/scripts/verify_runtime_surface_tests.py`. On Windows,
`runtime_audit_contract` runs contained and reports 18 tests with 1 failure
and 3 errors ([the BPR-0820 Windows confirmation](../../verification/BPR-0820-windows-confirmation.md)):

- `test_application_command_names_a_root_only_when_one_is_owned` compares
  `str(Path("/private/root/data"))` with the literal `"/private/root/data"`.
  Windows renders the path with backslashes.
- The three private-root cases fail inside the container at `Path.resolve()`
  of the `mkdtemp` parent, with `WinError 5`.

The course itself, `runtime_process_surface`, passes on Windows
(`RUNTIME_SURFACE_OK`); it is registered uncontained.

## Scope

- Make the command case compare paths, not a POSIX string literal.
- Make the private-root cases independent of `resolve()` access inside a
  container, for example by patching the resolution the unit test does not
  own, without weakening what the script does on a real run.
- Keep the script's real behaviour unchanged.

## Acceptance

- `runtime_audit_contract` passes on the Windows station as registered, and
  on macOS.

## Windows station, 2026-09-17 (task-20260917-win-creation-descriptor)

The base is `31fff50675a71bc2c5ce55857c72e57dc36933df`. Logs are under
`build/logs/task-20260917-win-creation-descriptor/`.

**Change** in `tests/scripts/verify_runtime_surface_tests.py` only.
`scripts/verify_runtime_surface.py` is unchanged.

- **`test_application_command_names_a_root_only_when_one_is_owned`** expects
  `str(data_root)` for `Path("/private/root/data")`, the path's own spelling,
  instead of the POSIX literal.
- **`private_root_run`, used by the three private-root cases,** patches two
  things **on Windows only**, both outside the script under test:
  - The script's `tempfile.mkdtemp` becomes
    `make_contained_temporary_directory`. Inside the AppContainer a `mkdtemp`
    directory is unusable ([[BPR-0350]]).
  - `Path.resolve` becomes a recorder that returns the absolute path. Inside
    the container CPython's `realpath` cannot resolve a DOS path.

  The case then asserts that the script still called `resolve(strict=True)` on
  the parent it made, so the canonical-spelling requirement stays tested. On
  POSIX the real `mkdtemp` and `resolve` still run, with the existing `0700`
  assertion.

**Results.**

| Check | Result |
|---|---|
| `runtime_audit_contract`, contained as registered | **Passed**, 0.32 s: `Ran 18 tests`, `OK (skipped=1)` |
| `runtime_process_surface`, uncontained as registered | **Passed**, 1.67 s: `RUNTIME_SURFACE_OK` |
| `python tests/scripts/verify_runtime_surface_tests.py`, uncontained | `OK (skipped=1)` |
| **Trial, reverted:** HEAD's test file at this base | `runtime_audit_contract` **fails**: `FAILED (failures=1, errors=3, skipped=1)` |

**Portable sources touched:** `tests/scripts/verify_runtime_surface_tests.py`.
Its POSIX path is unchanged apart from the command case now comparing with
`str(Path(...))`, which on POSIX is the same string. The macOS run is the
integrator's.
