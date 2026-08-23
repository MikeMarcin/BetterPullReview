---
id: BPR-0438
title: The CLI process runner's constructed child environment is not exact on Windows
status: Ready
rfc: RFC-BPR-0002
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
