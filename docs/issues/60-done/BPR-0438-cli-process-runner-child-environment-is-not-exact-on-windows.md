---
schema: "implement-rfc.card.v1"
id: "BPR-0438"
title: "The CLI process runner's constructed child environment is not exact on Windows"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: []
---

# The CLI process runner's constructed child environment is not exact on Windows

Uncontained on the Windows host, at `22b991c`:

```
github_cli_process_runner   FAILED: argv, stdin, cwd, and constructed child environment are exact
```

`tests/github_contract/github_cli_process_runner_tests.cpp:470-497` asserts
the exact argument vector, standard input, working directory, and
environment the runner builds for a child process. One of the four differs
on Windows, and the test reports them as a set, so which one is not yet
measured. The candidates are the ones a POSIX-written expectation would get
wrong here: a working directory compared by string after a separator change,
or an environment block that Windows itself supplements (`SystemRoot`,
`COMSPEC`, `PATHEXT`) or that the runner is expected to scrub.

Contained, the same test fails one step earlier — `QProcess: CreateNamedPipe
failed (Access is denied.)` — which is the lowbox named-pipe denial
[[BPR-0362]] measured for `ninja`, now reaching `QProcess`. That is a
registration question for [[BPR-0412]]; this card owns the uncontained
failure only.

## Required final state

- The failing member of the four is measured and named.
- The expectation describes the runner's contract in a form that holds on
  Windows and POSIX alike, or the runner is corrected where it is the
  runner that is wrong; a widened assertion that accepts any environment is
  not a repair.
- `github_cli_process_runner` passes on Windows as registered and is
  unchanged elsewhere.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^github_cli_process_runner$'
```

## Measured and repaired — 2026-08-23

The four were never the question. The helper the test launches is the test
executable itself, which links Qt, and the runner's Windows contract hands a
child an empty `PATH` — so the child could not load `Qt6Cored.dll`, put the
loader's box on the desktop (no uncontained test process had ever set an
error mode; the launcher sets one only for contained children), and sat
there until the runner's two-second timeout. The child now records which
member differed into `exact-boundary.txt` beside its working directory and
the parent prints it; the first run with that in place reported the runner's
`timeout` (code 3) rather than any member, which is what pointed away from
the expectation and at the child.

Three repairs, each where its cause lies:

- `tests/github_contract/CMakeLists.txt` copies the test's runtime DLLs
  beside the executable after each link (`$<TARGET_RUNTIME_DLLS>`), the one
  place the loader looks with an empty `PATH`; the runner's contract is
  untouched.
- `tests/msvc_quiet_crt_reports.cpp`, linked into every automated
  executable, now sets `SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
  SEM_NOOPENFILEERRORBOX` as well, which a child inherits; an uncontained
  test's child that cannot load is an exit code, not a dialog.
- The helper's Windows "crash" was `TerminateProcess(…, 137)`, which Qt
  reports as a normal exit — it is not an NTSTATUS failure — so the runner
  correctly said `nonzero_exit` where the test expected `process_crash`.
  The helper now leaves `STATUS_ACCESS_VIOLATION`, which is what a fault
  leaves.

With those, "argv, stdin, cwd, and constructed child environment are exact"
passes as written: the expectation described the runner's contract
correctly on Windows all along.

### Evidence

| claim | command | result |
|---|---|---|
| the contract passes on Windows as registered (uncontained, `qprocess-named-pipe`) | `ctest --preset windows-dev -R '^github_cli_process_runner$'` | Passed, 1.34 s (`cards-run-5.log`, 2026-08-23) |
| the failing member is named when one differs | `exact-boundary.txt` is written by the child and printed by the parent on any mismatch | in place; exercised by the timeout run before the DLL copy |
| macOS and Linux are unchanged | the DLL copy and the error mode are `WIN32`/`_MSC_VER` only; the crash helper's POSIX branch is untouched | by construction |

Implemented by the integrator.
