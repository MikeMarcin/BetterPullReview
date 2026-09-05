---
schema: "implement-rfc.card.v1"
id: "BPR-0434"
title: "The Debug CRT's abort() report box stalls an unattended Windows run"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The Debug CRT's abort() report box stalls an unattended Windows run

The second Windows Qt-graph run ([[BPR-0412]]) put a different dialog in
front of the owner, dozens of times:

> Microsoft Visual C++ Runtime Library — Debug Error!
> Program: …tter_pull_review_agent_invocation_composition_public_path.exe
> abort() has been called
> (Press Retry to debug the application)

[[BPR-0433]] had already stopped the loader's box: the launcher's error mode
is inherited, and `SEM_NOGPFAULTERRORBOX` covers a crash. This one is not
covered by any inherited setting. The Debug CRT reports `abort()`, a failed
assertion, and its own diagnostics with a message box by default, and that
choice is made inside the process — `_set_abort_behavior` and
`_CrtSetReportMode` — so nothing a parent does can make it. A test that
fails by aborting, which is what every failed `expect` in this suite does
under a debug runtime, therefore waits for a person.

## Required final state

- A single translation unit routes the CRT's reports to stderr:
  `_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)`, and under
  `_DEBUG` every report type to `_CRTDBG_MODE_FILE` on `_CRTDBG_FILE_STDERR`.
  It is empty on every other toolchain.
- `bpr_add_executable` links it into every non-application executable under
  MSVC, because that function is the one place every automated executable
  passes through; the application is untouched.
- A failing test's verdict does not change: the same `abort()` is the same
  exit code, now with its message on stderr where CTest captures it.

## Verification

```sh
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure -R '^application_agent_invocation_composition_public_path$'
```

and a desktop with no dialog on it while that test fails.

## Result

`tests/msvc_quiet_crt_reports.cpp` is linked into every non-application
executable by `bpr_add_executable` under MSVC: `core-only` rebuilt 228 of 228
and `windows-dev` relinked every test image with it. Two uncontained control
runs of the fifty Qt-graph failures then executed, including the agent
invocation contracts that had put the `abort()` box on the desktop; each
ended as an exit code (`Failed(3)` for an abort) with its report on stderr,
and no dialog appeared. The loader box that did appear once more during
those runs was the uncontained helper's own, not a CRT report, and the helper
now sets the launcher's error mode itself.

`bpr_add_executable` is also where the unit's scope is exact: the
application is not routed through the non-application branch and is
unchanged.
